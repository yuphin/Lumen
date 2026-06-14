#include "PostFX.h"
#include "Framework/CommandBuffer.h"
#include "Framework/DynamicResourceManager.h"
#include "Framework/ImageUtils.h"
#include "Framework/PersistentResourceManager.h"
#include "Framework/VulkanBase.h"
#include "Framework/Window.h"

void PostFX::init_fft() {
	// Load the kernel
	const char* img_name_kernel = "assets/kernels/Octagonal512.exr";
	i32 width, height;
	f32* data = ImageUtils::load_exr(img_name_kernel, width, height);
	vk::Texture* kernel_org =
		drm::get({.name = CSTR("Kernel"),
				  .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				  .dimensions = {(u32)width, (u32)height, 1},
				  .format = VK_FORMAT_R32G32B32A32_SFLOAT,
				  .data = {.data = data, .size = width * height * 4 * sizeof(f32)},
				  .sampler = img_sampler});

	if (data) {
		free(data);
	}
	// Compute padded sizes
	u32 padded_width = 1 << u32(ceil(log2(double(Window::width() + kernel_org->extent.width))));
	u32 padded_height = 1 << u32(ceil(log2(double(Window::height() + kernel_org->extent.height))));

	vk::TextureDesc empty_tex_desc = {.name = CSTR("FFT - Ping"),
									  .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
									  .dimensions = {padded_width, padded_height, 1},
									  .format = VK_FORMAT_R32G32B32A32_SFLOAT,
									  .initial_layout = VK_IMAGE_LAYOUT_GENERAL,
									  .sampler = img_sampler};
	fft_ping_padded = prm::get_texture(empty_tex_desc);
	empty_tex_desc.name = CSTR("FFT - Pong");
	fft_pong_padded = prm::get_texture(empty_tex_desc);
	empty_tex_desc.name = CSTR("Kernel - Pong");
	vk::Texture* kernel_ping = drm::get(empty_tex_desc);
	kernel_pong = prm::get_texture(empty_tex_desc);

	vk::CommandBuffer cmd(true, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	// Copy the original kernel image to the padded texture
	u32 pad_width = (kernel_org->extent.width + 31) / 32;
	u32 pad_height = (kernel_org->extent.height + 31) / 32;

	lm::RenderGraph* rg = vk::render_graph();
	rg->add_compute(CSTR("Pad Kernel"),
					{.shader = vk::Shader(CSTR("src/shaders/bloom/pad.comp")), .dims = {pad_width, pad_height, 1}})
		.bind_texture_with_sampler(kernel_org, img_sampler)
		.bind(kernel_ping);

	u32 wg_size_x = fft_ping_padded->extent.width;
	u32 wg_size_y = fft_ping_padded->extent.height;
	u32 dim_y = (u32)(fft_ping_padded->extent.width * fft_ping_padded->extent.height + wg_size_x - 1) / wg_size_x;
	u32 dim_x = (u32)(fft_ping_padded->extent.width * fft_ping_padded->extent.height + wg_size_y - 1) / wg_size_y;
	bool vertical = false;

	const i32 RADIX_X = (31 - lm::count_leading_zeros(fft_ping_padded->extent.width)) % 2 ? 2 : 4;
	const i32 RADIX_Y = (31 - lm::count_leading_zeros(fft_ping_padded->extent.height)) % 2 ? 2 : 4;
	vk::ShaderMacroArray macros_x;
	vk::ShaderMacroArray macros_y;
	macros_x.push_back({"KERNEL_GENERATION"});
	macros_y.push_back({"KERNEL_GENERATION"});
	if (RADIX_X != 2) {
		macros_x.push_back({"RADIX", RADIX_X});
		macros_y.push_back({"RADIX", RADIX_Y});
	}
	rg->add_compute(CSTR("FFT - Horizontal"), {.shader = vk::Shader(CSTR("src/shaders/bloom/fft.comp")),
											   .macros = macros_x,
											   .specialization_data = {wg_size_x / RADIX_X, u32(vertical), 0},
											   .dims = {dim_y, 1, 1}})
		.bind_texture_with_sampler(kernel_ping, img_sampler)
		.bind(kernel_pong);
	vertical = true;
	rg->add_compute(CSTR("FFT - Vertical"), {.shader = vk::Shader(CSTR("src/shaders/bloom/fft.comp")),
											 .macros = macros_y,
											 .specialization_data = {wg_size_y / RADIX_Y, u32(vertical), 0},
											 .dims = {dim_x, 1, 1}})
		.bind_texture_with_sampler(kernel_ping, img_sampler)
		.bind(kernel_pong);
	rg->run_and_submit(cmd);
	drm::destroy(kernel_org);
	drm::destroy(kernel_ping);
}

void PostFX::init() {
	VkSamplerCreateInfo sampler_ci = vk::sampler();
	sampler_ci.minFilter = VK_FILTER_NEAREST;
	sampler_ci.magFilter = VK_FILTER_NEAREST;
	sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_ci.maxLod = FLT_MAX;

	sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	vk::check(vkCreateSampler(vk::context().device, &sampler_ci, nullptr, &img_sampler));
}

void PostFX::render(vk::Texture* input, vk::Texture* output) {
	lm::RenderGraph* rg = vk::render_graph();
	// Copy the original image to the padded texture
	if (enable_bloom) {
		if (!fft_ping_padded) {
			init_fft();
		}

		u32 pad_width = (fft_ping_padded->extent.width + 31) / 32;
		u32 pad_height = (fft_ping_padded->extent.height + 31) / 32;

		rg->add_compute(CSTR("Pad Image"),
						{.shader = vk::Shader(CSTR("src/shaders/bloom/pad.comp")), .dims = {pad_width, pad_height, 1}})
			.bind_texture_with_sampler(input, img_sampler)
			.bind(fft_ping_padded);
		u32 wg_size_x = fft_ping_padded->extent.width;
		u32 wg_size_y = fft_ping_padded->extent.height;
		u32 dim_y = (u32)(fft_ping_padded->extent.width * fft_ping_padded->extent.height + wg_size_x - 1) / wg_size_x;
		u32 dim_x = (u32)(fft_ping_padded->extent.width * fft_ping_padded->extent.height + wg_size_y - 1) / wg_size_y;
		bool vertical = false;
		const i32 RADIX_X = (31 - lm::count_leading_zeros(fft_ping_padded->extent.width)) % 2 ? 2 : 4;
		const i32 RADIX_Y = (31 - lm::count_leading_zeros(fft_ping_padded->extent.height)) % 2 ? 2 : 4;
		vk::ShaderMacroArray macros_x;
		vk::ShaderMacroArray macros_y;
		if (RADIX_X != 2) {
			macros_x.push_back({"RADIX", RADIX_X});
		}
		if (RADIX_Y != 2) {
			macros_y.push_back({"RADIX", RADIX_Y});
		}

		rg->add_compute(CSTR("FFT - Horizontal"), {.shader = vk::Shader(CSTR("src/shaders/bloom/fft.comp")),
												   .macros = macros_x,
												   .specialization_data = {wg_size_x / RADIX_X, u32(vertical), 0},
												   .dims = {dim_y, 1, 1}})
			.bind_texture_with_sampler(fft_ping_padded, img_sampler)
			.bind(fft_pong_padded)
			.bind_texture_with_sampler(kernel_pong, img_sampler);
		vertical = true;
		rg->add_compute(CSTR("FFT - Vertical"), {.shader = vk::Shader(CSTR("src/shaders/bloom/fft.comp")),
												 .macros = macros_y,
												 .specialization_data = {wg_size_y / RADIX_Y, u32(vertical), 0},
												 .dims = {dim_x, 1, 1}})
			.bind_texture_with_sampler(fft_ping_padded, img_sampler)
			.bind(fft_pong_padded)
			.bind_texture_with_sampler(kernel_pong, img_sampler);
		rg->add_compute(CSTR("FFT - Vertical - Inverse"),
						{.shader = vk::Shader(CSTR("src/shaders/bloom/fft.comp")),
						 .macros = macros_y,
						 .specialization_data = {wg_size_y / RADIX_Y, u32(vertical), 1},
						 .dims = {dim_x, 1, 1}})
			.bind_texture_with_sampler(fft_ping_padded, img_sampler)
			.bind(fft_pong_padded)
			.bind_texture_with_sampler(kernel_pong, img_sampler);
		vertical = false;
		rg->add_compute(CSTR("FFT - Horizontal - Inverse"),
						{.shader = vk::Shader(CSTR("src/shaders/bloom/fft.comp")),
						 .macros = macros_x,
						 .specialization_data = {wg_size_x / RADIX_X, u32(vertical), 1},
						 .dims = {dim_y, 1, 1}})
			.bind_texture_with_sampler(fft_ping_padded, img_sampler)
			.bind(fft_pong_padded)
			.bind_texture_with_sampler(kernel_pong, img_sampler);
	}

	pc_post_settings.enable_tonemapping = enable_tonemapping;
	pc_post_settings.bloom_amount = bloom_amount;
	pc_post_settings.bloom_exposure = bloom_exposure;
	pc_post_settings.width = output->extent.width;
	pc_post_settings.height = output->extent.height;

	rg->add_gfx(CSTR("Post FX"), {.shaders = {{CSTR("src/shaders/post.vert")}, {CSTR("src/shaders/post.frag")}},
								  .macros = {vk::ShaderMacro("ENABLE_BLOOM", enable_bloom)},
								  .width = output->extent.width,
								  .height = output->extent.height,
								  .clear_color = {VkClearColorValue{{0.25f, 0.25f, 0.25f, 1.0f}}},
								  .clear_depth_stencil = {{{1.0f, 0}}},
								  .cull_mode = VK_CULL_MODE_NONE,
								  .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
								  .color_outputs = {output},
								  .pass_func =
									  [](VkCommandBuffer cmd, const lm::RenderPass& render_pass) {
										  vkCmdDraw(cmd, 4, 1, 0, 0);
										  ImGui::Render();
										  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
									  }})
		.push_constants(&pc_post_settings)
		.bind_texture_with_sampler(input, img_sampler);
	if (enable_bloom) {
		rg->current_pass().bind_texture_with_sampler(fft_pong_padded, img_sampler);
	}
}

bool PostFX::gui() {
	bool updated = false;
	ImGui::NewLine();
	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
	ImGui::Text("PostFX Settings:");
	ImGui::PopStyleColor();
	ImGui::Checkbox("Enable ACES tonemapping", &enable_tonemapping);
	if (ImGui::Checkbox("Enable bloom", &enable_bloom) && !enable_bloom) {
		vkDeviceWaitIdle(vk::context().device);
		std::initializer_list<vk::Texture*> tex_list = {kernel_pong, fft_ping_padded, fft_pong_padded};
		for (vk::Texture* t : tex_list) {
			prm::remove(t);
		}
		kernel_pong = nullptr;
		fft_ping_padded = nullptr;
		fft_pong_padded = nullptr;
	}
	f32 exposure = log10f(bloom_exposure);
	ImGui::SliderFloat("Bloom exposure", &exposure, -20.0f, 0.0f, "%.2f");
	bloom_exposure = powf(10.0f, exposure);
	ImGui::SliderFloat("Bloom amount", &bloom_amount, 0.0f, 1.0f, "%.2f");
	return updated;
}

void PostFX::destroy() {
	std::initializer_list<vk::Texture*> tex_list = {kernel_pong, fft_ping_padded, fft_pong_padded};
	for (vk::Texture* t : tex_list) {
		prm::remove(t);
	}
	kernel_pong = nullptr;
	fft_ping_padded = nullptr;
	fft_pong_padded = nullptr;
	vkDestroySampler(vk::context().device, img_sampler, 0);
}
