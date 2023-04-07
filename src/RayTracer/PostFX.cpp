#include "LumenPCH.h"
#include "PostFX.h"


void PostFX::init(LumenInstance& instance) {
	ctx = &instance.vk_ctx;
	rg = instance.vkb.rg.get();

	VkSamplerCreateInfo sampler_ci = vk::sampler_create_info();
	sampler_ci.minFilter = VK_FILTER_NEAREST;
	sampler_ci.magFilter = VK_FILTER_NEAREST;
	sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_ci.maxLod = FLT_MAX;

	sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	vk::check(vkCreateSampler(instance.vkb.ctx.device, &sampler_ci, nullptr, &img_sampler),
			  "Could not create image sampler");
	// Load the kernel
	const char* img_name_kernel = "PupilsResized.exr";
	int width, height;
	float* data = load_exr(img_name_kernel, width, height);
	auto img_dims = VkExtent2D{(uint32_t)width, (uint32_t)height};
	auto ci = make_img2d_ci(img_dims, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, false);
	Texture2D kernel_org;
	kernel_org.load_from_data(&instance.vkb.ctx, data, width * height * 4 * sizeof(float), ci, img_sampler,
							  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, false);
	if (data) {
		free(data);
	}
	// Compute padded sizes
	auto padded_width = 1 << uint32_t(ceil(log2(double(instance.width + kernel_org.base_extent.width))));
	auto padded_height = 1 << uint32_t(ceil(log2(double(instance.height + kernel_org.base_extent.height))));

	TextureSettings settings;
	settings.base_extent.width = padded_width;
	settings.base_extent.height = padded_height;

	settings.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	settings.usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	fft_ping_padded.create_empty_texture("FFT - Ping", ctx, settings, VK_IMAGE_LAYOUT_GENERAL, img_sampler);
	fft_pong_padded.create_empty_texture("FFT - Pong", ctx, settings, VK_IMAGE_LAYOUT_GENERAL, img_sampler);
	kernel_ping.create_empty_texture("Kernel - Ping", ctx, settings, VK_IMAGE_LAYOUT_GENERAL, img_sampler);
	kernel_pong.create_empty_texture("Kernel - Pong", ctx, settings, VK_IMAGE_LAYOUT_GENERAL, img_sampler);

	CommandBuffer cmd(ctx, true);

	// Copy the original kernel image to the padded texture
	uint32_t pad_width = (kernel_org.base_extent.width + 31) / 32;
	uint32_t pad_height = (kernel_org.base_extent.height + 31) / 32;
	rg->add_compute("Pad Kernel", {.shader = Shader("src/shaders/fft/pad.comp"), .dims = {pad_width, pad_height, 1}})
		.bind(kernel_org, img_sampler)
		.bind(kernel_ping);

	const bool FFT_SHARED_MEM = true;
	uint32_t wg_size_x = fft_ping_padded.base_extent.width;
	uint32_t wg_size_y = fft_ping_padded.base_extent.height;
	auto dim_y =
		(uint32_t)(fft_ping_padded.base_extent.width * fft_ping_padded.base_extent.height + wg_size_x - 1) / wg_size_x;
	auto dim_x =
		(uint32_t)(fft_ping_padded.base_extent.width * fft_ping_padded.base_extent.height + wg_size_y - 1) / wg_size_y;
	bool vertical = false;

	const int RADIX_X = (31 - std::countl_zero(fft_ping_padded.base_extent.width)) % 2 ? 2 : 4;
	const int RADIX_Y = (31 - std::countl_zero(fft_ping_padded.base_extent.height)) % 2 ? 2 : 4;
	rg->add_compute("FFT - Horizontal", {.shader = Shader("src/shaders/fft/fft.comp"),
										 .macros = {{"KERNEL_GENERATION"}, {"RADIX", RADIX_X}},
										 .specialization_data = {wg_size_x / RADIX_X, uint32_t(vertical), 0},
										 .dims = {dim_y, 1, 1}})
		.bind(kernel_ping, img_sampler)
		.bind(kernel_pong);
	vertical = true;
	rg->add_compute("FFT - Vertical", {.shader = Shader("src/shaders/fft/fft.comp"),
									   .macros = {{"KERNEL_GENERATION"}, {"RADIX", RADIX_Y}},
									   .specialization_data = {wg_size_y / RADIX_Y, uint32_t(vertical), 0},
									   .dims = {dim_x, 1, 1}})
		.bind(kernel_ping, img_sampler)
		.bind(kernel_pong);
	rg->run_and_submit(cmd);
}

void PostFX::render(Texture2D& input, Texture2D& output) {
	// Copy the original image to the padded texture
	uint32_t pad_width = (fft_ping_padded.base_extent.width + 31) / 32;
	uint32_t pad_height = (fft_ping_padded.base_extent.height + 31) / 32;
	rg->add_compute("Pad Image", {.shader = Shader("src/shaders/fft/pad.comp"), .dims = {pad_width, pad_height, 1}})
		.bind(input, img_sampler)
		.bind(fft_ping_padded);
	uint32_t wg_size_x = fft_ping_padded.base_extent.width;
	uint32_t wg_size_y = fft_ping_padded.base_extent.height;
	auto dim_y =
		(uint32_t)(fft_ping_padded.base_extent.width * fft_ping_padded.base_extent.height + wg_size_x - 1) / wg_size_x;
	auto dim_x =
		(uint32_t)(fft_ping_padded.base_extent.width * fft_ping_padded.base_extent.height + wg_size_y - 1) / wg_size_y;
	bool vertical = false;
	const int RADIX_X = (31 - std::countl_zero(fft_ping_padded.base_extent.width)) % 2 ? 2 : 4;
	const int RADIX_Y = (31 - std::countl_zero(fft_ping_padded.base_extent.height)) % 2 ? 2 : 4;
	const std::vector<ShaderMacro> macros_x =
		RADIX_X == 2 ? std::vector<ShaderMacro>{} : std::vector<ShaderMacro>{{"RADIX", RADIX_X}};
	const std::vector<ShaderMacro> macros_y =
		RADIX_Y == 2 ? std::vector<ShaderMacro>{} : std::vector<ShaderMacro>{{"RADIX", RADIX_Y}};
	rg->add_compute("FFT - Horizontal", {.shader = Shader("src/shaders/fft/fft.comp"),
										 .macros = macros_x,
										 .specialization_data = {wg_size_x / RADIX_X, uint32_t(vertical), 0},
										 .dims = {dim_y, 1, 1}})
		.bind(fft_ping_padded, img_sampler)
		.bind(fft_pong_padded)
		.bind(kernel_pong, img_sampler);
	vertical = true;
	rg->add_compute("FFT - Vertical", {.shader = Shader("src/shaders/fft/fft.comp"),
									   .macros = macros_y,
									   .specialization_data = {wg_size_y / RADIX_Y, uint32_t(vertical), 0},
									   .dims = {dim_x, 1, 1}})
		.bind(fft_ping_padded, img_sampler)
		.bind(fft_pong_padded)
		.bind(kernel_pong, img_sampler);
	rg->add_compute("FFT - Vertical - Inverse", {.shader = Shader("src/shaders/fft/fft.comp"),
												 .macros = macros_y,
												 .specialization_data = {wg_size_y / RADIX_Y, uint32_t(vertical), 1},
												 .dims = {dim_x, 1, 1}})
		.bind(fft_ping_padded, img_sampler)
		.bind(fft_pong_padded)
		.bind(kernel_pong, img_sampler);
	vertical = false;
	rg->add_compute("FFT - Horizontal - Inverse", {.shader = Shader("src/shaders/fft/fft.comp"),
												   .macros = macros_x,
												   .specialization_data = {wg_size_x / RADIX_X, uint32_t(vertical), 1},
												   .dims = {dim_y, 1, 1}})
		.bind(fft_ping_padded, img_sampler)
		.bind(fft_pong_padded)
		.bind(kernel_pong, img_sampler);

	pc_post_settings.enable_tonemapping = enable_tonemapping;

	rg->add_gfx("Post FX", {.shaders = {{"src/shaders/post.vert"}, {"src/shaders/post.frag"}},
							.width = output.base_extent.width,
							.height = output.base_extent.height,
							.clear_color = {0.25f, 0.25f, 0.25f, 1.0f},
							.clear_depth_stencil = {1.0f, 0},
							.cull_mode = VK_CULL_MODE_NONE,
							.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
							.color_outputs = {&output},
							.pass_func =
								[](VkCommandBuffer cmd, const RenderPass& render_pass) {
									vkCmdDraw(cmd, 4, 1, 0, 0);
									ImGui::Render();
									ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
								}})
		.push_constants(&pc_post_settings)
		.bind(fft_pong_padded, img_sampler)
		.bind(kernel_pong, img_sampler);
}

bool PostFX::gui() {
	return ImGui::Checkbox("Enable ACES tonemapping", &enable_tonemapping);

}
