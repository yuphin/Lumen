#include "Framework/RenderGraph.h"
#include "LumenPCH.h"
#include "PostFX.h"
#include "Framework/PersistentResourceManager.h"
#include "Framework/DynamicResourceManager.h"

void PostFX::init() {
	VkSamplerCreateInfo sampler_ci = vk::sampler();
	sampler_ci.minFilter = VK_FILTER_NEAREST;
	sampler_ci.magFilter = VK_FILTER_NEAREST;
	sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_ci.maxLod = FLT_MAX;

	sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	vk::check(vkCreateSampler(vk::context().device, &sampler_ci, nullptr, &img_sampler),
			  "Could not create image sampler");
	// Load the kernel
	const char* img_name_kernel = "assets/kernels/Octagonal512.exr";
	int width, height;
	float* data = ImageUtils::load_exr(img_name_kernel, width, height);
	vk::Texture* kernel_org =
		drm::get({.name = "Kernel",
				  .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				  .dimensions = {(uint32_t)width, (uint32_t)height, 1},
				  .format = VK_FORMAT_R32G32B32A32_SFLOAT,
				  .data = {.data = data, .size = width * height * 4 * sizeof(float)},
				  .sampler = img_sampler});

	if (data) {
		free(data);
	}
	// Compute padded sizes
	uint32_t padded_width = 1 << uint32_t(ceil(log2(double(Window::width() + kernel_org->extent.width))));
	uint32_t padded_height = 1 << uint32_t(ceil(log2(double(Window::height() + kernel_org->extent.height))));

	auto empty_tex_desc = vk::TextureDesc{.name = "FFT - Ping",
										  .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
										  .dimensions = {padded_width, padded_height, 1},
										  .format = VK_FORMAT_R32G32B32A32_SFLOAT,
										  .initial_layout = VK_IMAGE_LAYOUT_GENERAL,
										  .sampler = img_sampler};
	fft_ping_padded = prm::get_texture(empty_tex_desc);
	empty_tex_desc.name = "FFT - Pong";
	fft_pong_padded = prm::get_texture(empty_tex_desc);
	empty_tex_desc.name = "Kernel - Ping";
	empty_tex_desc.name = "Kernel - Pong";
	vk::Texture* kernel_ping = drm::get(empty_tex_desc);
	kernel_pong = prm::get_texture(empty_tex_desc);

	vk::CommandBuffer cmd(true);

	// Copy the original kernel image to the padded texture
	uint32_t pad_width = (kernel_org->extent.width + 31) / 32;
	uint32_t pad_height = (kernel_org->extent.height + 31) / 32;

	lumen::RenderGraph* rg = vk::render_graph();
	rg->add_compute("Pad Kernel",
					{.shader = vk::Shader("src/shaders/bloom/pad.comp"), .dims = {pad_width, pad_height, 1}})
		.bind_texture_with_sampler(kernel_org, img_sampler)
		.bind(kernel_ping);

	uint32_t wg_size_x = fft_ping_padded->extent.width;
	uint32_t wg_size_y = fft_ping_padded->extent.height;
	auto dim_y = (uint32_t)(fft_ping_padded->extent.width * fft_ping_padded->extent.height + wg_size_x - 1) / wg_size_x;
	auto dim_x = (uint32_t)(fft_ping_padded->extent.width * fft_ping_padded->extent.height + wg_size_y - 1) / wg_size_y;
	bool vertical = false;

	const int RADIX_X = (31 - std::countl_zero(fft_ping_padded->extent.width)) % 2 ? 2 : 4;
	const int RADIX_Y = (31 - std::countl_zero(fft_ping_padded->extent.height)) % 2 ? 2 : 4;
	const std::vector<vk::ShaderMacro> macros_x =
		RADIX_X == 2 ? std::vector<vk::ShaderMacro>{{"KERNEL_GENERATION"}}
					 : std::vector<vk::ShaderMacro>{{"KERNEL_GENERATION"}, {"RADIX", RADIX_X}};
	const std::vector<vk::ShaderMacro> macros_y =
		RADIX_Y == 2 ? std::vector<vk::ShaderMacro>{{"KERNEL_GENERATION"}}
					 : std::vector<vk::ShaderMacro>{{"KERNEL_GENERATION"}, {"RADIX", RADIX_Y}};
	rg->add_compute("FFT - Horizontal", {.shader = vk::Shader("src/shaders/bloom/fft.comp"),
										 .macros = macros_x,
										 .specialization_data = {wg_size_x / RADIX_X, uint32_t(vertical), 0},
										 .dims = {dim_y, 1, 1}})
		.bind_texture_with_sampler(kernel_ping, img_sampler)
		.bind(kernel_pong);
	vertical = true;
	rg->add_compute("FFT - Vertical", {.shader = vk::Shader("src/shaders/bloom/fft.comp"),
									   .macros = macros_y,
									   .specialization_data = {wg_size_y / RADIX_Y, uint32_t(vertical), 0},
									   .dims = {dim_x, 1, 1}})
		.bind_texture_with_sampler(kernel_ping, img_sampler)
		.bind(kernel_pong);
	rg->run_and_submit(cmd);
	drm::destroy(kernel_org);
	drm::destroy(kernel_ping);
}

void PostFX::render(vk::Texture* input, vk::Texture* output) {
	lumen::RenderGraph* rg = vk::render_graph();
	// Copy the original image to the padded texture
	if (enable_bloom) {
		uint32_t pad_width = (fft_ping_padded->extent.width + 31) / 32;
		uint32_t pad_height = (fft_ping_padded->extent.height + 31) / 32;

		rg->add_compute("Pad Image",
						{.shader = vk::Shader("src/shaders/bloom/pad.comp"), .dims = {pad_width, pad_height, 1}})
			.bind_texture_with_sampler(input, img_sampler)
			.bind(fft_ping_padded);
		uint32_t wg_size_x = fft_ping_padded->extent.width;
		uint32_t wg_size_y = fft_ping_padded->extent.height;
		auto dim_y =
			(uint32_t)(fft_ping_padded->extent.width * fft_ping_padded->extent.height + wg_size_x - 1) / wg_size_x;
		auto dim_x =
			(uint32_t)(fft_ping_padded->extent.width * fft_ping_padded->extent.height + wg_size_y - 1) / wg_size_y;
		bool vertical = false;
		const int RADIX_X = (31 - std::countl_zero(fft_ping_padded->extent.width)) % 2 ? 2 : 4;
		const int RADIX_Y = (31 - std::countl_zero(fft_ping_padded->extent.height)) % 2 ? 2 : 4;
		const std::vector<vk::ShaderMacro> macros_x =
			RADIX_X == 2 ? std::vector<vk::ShaderMacro>{} : std::vector<vk::ShaderMacro>{{"RADIX", RADIX_X}};
		const std::vector<vk::ShaderMacro> macros_y =
			RADIX_Y == 2 ? std::vector<vk::ShaderMacro>{} : std::vector<vk::ShaderMacro>{{"RADIX", RADIX_Y}};
		rg->add_compute("FFT - Horizontal", {.shader = vk::Shader("src/shaders/bloom/fft.comp"),
											 .macros = macros_x,
											 .specialization_data = {wg_size_x / RADIX_X, uint32_t(vertical), 0},
											 .dims = {dim_y, 1, 1}})
			.bind_texture_with_sampler(fft_ping_padded, img_sampler)
			.bind(fft_pong_padded)
			.bind_texture_with_sampler(kernel_pong, img_sampler);
		vertical = true;
		rg->add_compute("FFT - Vertical", {.shader = vk::Shader("src/shaders/bloom/fft.comp"),
										   .macros = macros_y,
										   .specialization_data = {wg_size_y / RADIX_Y, uint32_t(vertical), 0},
										   .dims = {dim_x, 1, 1}})
			.bind_texture_with_sampler(fft_ping_padded, img_sampler)
			.bind(fft_pong_padded)
			.bind_texture_with_sampler(kernel_pong, img_sampler);
		rg->add_compute("FFT - Vertical - Inverse",
						{.shader = vk::Shader("src/shaders/bloom/fft.comp"),
						 .macros = macros_y,
						 .specialization_data = {wg_size_y / RADIX_Y, uint32_t(vertical), 1},
						 .dims = {dim_x, 1, 1}})
			.bind_texture_with_sampler(fft_ping_padded, img_sampler)
			.bind(fft_pong_padded)
			.bind_texture_with_sampler(kernel_pong, img_sampler);
		vertical = false;
		rg->add_compute("FFT - Horizontal - Inverse",
						{.shader = vk::Shader("src/shaders/bloom/fft.comp"),
						 .macros = macros_x,
						 .specialization_data = {wg_size_x / RADIX_X, uint32_t(vertical), 1},
						 .dims = {dim_y, 1, 1}})
			.bind_texture_with_sampler(fft_ping_padded, img_sampler)
			.bind(fft_pong_padded)
			.bind_texture_with_sampler(kernel_pong, img_sampler);
	}

	pc_post_settings.enable_tonemapping = enable_tonemapping;
	pc_post_settings.enable_bloom = enable_bloom;
	pc_post_settings.bloom_amount = bloom_amount;
	pc_post_settings.bloom_exposure = bloom_exposure;
	pc_post_settings.width = output->extent.width;
	pc_post_settings.height = output->extent.height;

	rg->add_gfx("Post FX", {.shaders = {{"src/shaders/post.vert"}, {"src/shaders/post.frag"}},
							.width = output->extent.width,
							.height = output->extent.height,
							.clear_color = {VkClearColorValue{{0.25f, 0.25f, 0.25f, 1.0f}}},
							.clear_depth_stencil = {{{1.0f, 0}}},
							.cull_mode = VK_CULL_MODE_NONE,
							.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
							.color_outputs = {output},
							.pass_func =
								[](VkCommandBuffer cmd, const lumen::RenderPass& render_pass) {
									vkCmdDraw(cmd, 4, 1, 0, 0);
									ImGui::Render();
									ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
								}})
		.push_constants(&pc_post_settings)
		.bind_texture_with_sampler(fft_pong_padded, img_sampler)
		.bind_texture_with_sampler(input, img_sampler);
}

bool PostFX::gui() {
	bool updated = false;
	ImGui::NewLine();
	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
	ImGui::Text("PostFX Settings:");
	ImGui::PopStyleColor();
	ImGui::Checkbox("Enable ACES tonemapping", &enable_tonemapping);
	ImGui::Checkbox("Enable bloom", &enable_bloom);
	float exposure = log10f(bloom_exposure);
	ImGui::SliderFloat("Bloom exposure", &exposure, -20.0f, 0.0f, "%.2f");
	bloom_exposure = powf(10.0f, exposure);
	ImGui::SliderFloat("Bloom amount", &bloom_amount, 0.0f, 1.0f, "%.2f");
	return updated;
}

void PostFX::destroy() {
	std::vector<vk::Texture*> tex_list = {kernel_pong, fft_ping_padded, fft_pong_padded};
	for (auto t : tex_list) {
		prm::remove(t);
	}
	vkDestroySampler(vk::context().device, img_sampler, 0);
}
