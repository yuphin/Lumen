#include "LumenPCH.h"
#include "RenderGraph.h"
#include "Utils.h"

#define DIRTY_CHECK(x) if(!x) {return *this;}

static 	VkPipelineStageFlags get_pipeline_stage(PassType pass_type) {
	switch (pass_type) {
		case PassType::Compute:
			return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		case PassType::Graphics:
			return VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
		case PassType::RT:
			return VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
		default:
			return  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	}
}

static VkImageLayout get_target_img_layout(const Texture2D& tex, HandleStatus status) {
	if (tex.usage_flags & VK_IMAGE_USAGE_SAMPLED_BIT) {
		if (status == HandleStatus::Write) {
			return VK_IMAGE_LAYOUT_GENERAL;
		}
		return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
	}
	return VK_IMAGE_LAYOUT_GENERAL;
}


void RenderPass::register_dependencies(Buffer& buffer, VkAccessFlags dst_access_flags) {
	if (rg->buffer_resource_map.find(buffer.handle) == rg->buffer_resource_map.end()) {
		return;
	}

	RenderPass& opposing_pass = rg->passes[rg->buffer_resource_map[buffer.handle]];
	// Invariant : Pass with lower index should be the setter
	// Set current pass dependencies
	if (wait_signals_buffer.find(buffer.handle) == wait_signals_buffer.end()) {
		wait_signals_buffer[buffer.handle] = BufferSyncDescriptor{
		   .dst_access_flags = dst_access_flags,
		   .opposing_pass_idx = opposing_pass.pass_idx,
		};
	}
	// Set source pass dependencies (Signalling pass)
	if (opposing_pass.set_signals_buffer.find(buffer.handle) == opposing_pass.set_signals_buffer.end()) {
		opposing_pass.set_signals_buffer[buffer.handle] = BufferSyncDescriptor{
			.dst_access_flags = dst_access_flags,
			.opposing_pass_idx = pass_idx
		};
	}
}

void RenderPass::register_dependencies(Texture2D& tex, VkImageLayout dst_layout) {
	if (tex.layout == dst_layout) {
		return;
	}
	if (tex.layout == VK_IMAGE_LAYOUT_UNDEFINED ||
		rg->img_resource_map.find(tex.img) == rg->img_resource_map.end()) {
		layout_transitions.push_back({ &tex, dst_layout });
		tex.layout = dst_layout;
	} else {
		RenderPass& opposing_pass = rg->passes[rg->img_resource_map[tex.img]];
		VkAccessFlags dst_access_flags = vk::access_flags_for_img_layout(dst_layout);
		if (opposing_pass.pass_idx < pass_idx) {
			// Set current pass dependencies (Waiting pass)
			if (wait_signals_img.find(tex.img) == wait_signals_img.end()) {
				wait_signals_img[tex.img] = ImageSyncDescriptor{
					.old_layout = tex.layout,
					.new_layout = dst_layout,
					.opposing_pass_idx = opposing_pass.pass_idx,
					.image_aspect = tex.aspect_flags
				};
			}
			// Set source pass dependencies (Signalling pass)
			if (opposing_pass.set_signals_img.find(tex.img) == opposing_pass.set_signals_img.end()) {
				opposing_pass.set_signals_img[tex.img] = ImageSyncDescriptor{
					.old_layout = tex.layout,
					.new_layout = dst_layout,
					.opposing_pass_idx = pass_idx,
					.image_aspect = tex.aspect_flags
				};
			}
			tex.layout = dst_layout;
		} else {
			layout_transitions.push_back({ &tex, dst_layout });
			tex.layout = dst_layout;
		}
	}
}

RenderPass& RenderGraph::add_rt(const std::string& name, const RTPassSettings& settings) {
	if (pipeline_cache.find(name) != pipeline_cache.end()) {
		return passes[pipeline_cache[name]->pass_idx];
	}
	size_t pass_idx = passes.size();
	pipeline_cache[name] = std::make_unique<Pipeline>(ctx, pass_idx);
	passes.emplace_back(PassType::RT, pipeline_cache[name].get(), name, this, pass_idx, settings);
	auto& pass = passes.back();
	for (auto& shader : pass.rt_settings->pipeline_settings.shaders) {
		shader.compile();
	}
	return pass;
}

RenderPass& RenderGraph::add_gfx(const std::string& name, const GraphicsPassSettings& settings) {
	if (pipeline_cache.find(name) != pipeline_cache.end()) {
		auto& curr_pass = passes[pipeline_cache[name]->pass_idx];
		curr_pass.gfx_settings->color_outputs = settings.color_outputs;
		curr_pass.gfx_settings->depth_output = settings.depth_output;
		return curr_pass;
	}
	size_t pass_idx = passes.size();
	pipeline_cache[name] = std::make_unique<Pipeline>(ctx, pass_idx);
	passes.emplace_back(PassType::Graphics, pipeline_cache[name].get(), name, this, pass_idx, settings);
	auto& pass = passes.back();
	for (auto& shader : pass.gfx_settings->pipeline_settings.shaders) {
		shader.compile();
	}
	return pass;
}

RenderPass& RenderGraph::add_compute(const std::string& name, const ComputePassSettings& settings) {
	if (pipeline_cache.find(name) != pipeline_cache.end()) {
		//passes.emplace_back(PassType::Compute, pipeline_cache[name].get(), name, this, passes.size(), settings);
		//return passes.back();
		return passes[pipeline_cache[name]->pass_idx];
	}
	size_t pass_idx = passes.size();
	pipeline_cache[name] = std::make_unique<Pipeline>(ctx, pass_idx);
	passes.emplace_back(PassType::Compute, pipeline_cache[name].get(), name, this, pass_idx, settings);
	auto& pass = passes.back();
	pass.compute_settings->pipeline_settings.shader.compile();
	return passes.back();
}

RenderPass& RenderPass::bind(const ResourceBinding& binding) {
	DIRTY_CHECK(rg->dirty);
	bound_resources.push_back(binding);
	descriptor_counts.push_back(1);
	return *this;

}

RenderPass& RenderPass::bind(const std::vector<ResourceBinding>& bindings) {
	DIRTY_CHECK(rg->dirty);
	bound_resources.insert(bound_resources.end(), bindings.begin(), bindings.end());
	for (int i = 0; i < bindings.size(); i++) {
		descriptor_counts.push_back(1);
	}
	return *this;

}

RenderPass& RenderPass::bind(const Texture2D& tex, VkSampler sampler) {
	DIRTY_CHECK(rg->dirty);
	bound_resources.emplace_back(tex, sampler);
	descriptor_counts.push_back(1);
	return *this;
}

RenderPass& RenderPass::bind_texture_array(const std::vector<Texture2D>& textures) {
	DIRTY_CHECK(rg->dirty)
		for (const auto& texture : textures) {
			bound_resources.emplace_back(texture);
		}
	descriptor_counts.push_back((uint32_t)textures.size());
	return *this;
}

RenderPass& RenderPass::bind_buffer_array(const std::vector<Buffer>& buffers) {
	DIRTY_CHECK(rg->dirty);
	for (int i = 0; i < buffers.size(); i++) {
		bound_resources.emplace_back(buffers[i]);
	}
	descriptor_counts.push_back((uint32_t)buffers.size());
	return *this;
}

RenderPass& RenderPass::bind_tlas(const AccelKHR& tlas) {
	DIRTY_CHECK(rg->dirty);
	tlas_info = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
	tlas_info.accelerationStructureCount = 1;
	tlas_info.pAccelerationStructures = &tlas.accel;
	return *this;
}


RenderPass& RenderPass::read(Buffer& buffer) {
	register_dependencies(buffer, VK_ACCESS_SHADER_READ_BIT);
	rg->buffer_resource_map[buffer.handle] = pass_idx;
	return *this;
}

RenderPass& RenderPass::read(Texture2D& tex) {

	VkImageLayout target_layout = get_target_img_layout(tex, HandleStatus::Read);
	register_dependencies(tex, target_layout);
	rg->img_resource_map[tex.img] = pass_idx;
	return *this;
}


RenderPass& RenderPass::write(Buffer& buffer) {
	register_dependencies(buffer, VK_ACCESS_SHADER_WRITE_BIT);
	rg->buffer_resource_map[buffer.handle] = pass_idx;
	return *this;
}

RenderPass& RenderPass::write(Texture2D& tex) {
	VkImageLayout target_layout = get_target_img_layout(tex, HandleStatus::Write);
	register_dependencies(tex, target_layout);
	rg->img_resource_map[tex.img] = pass_idx;
	return *this;
}

RenderPass& RenderPass::push_constants(void* data, uint32_t size) {
	push_constant = { data, size };
	return *this;
}

RenderPass& RenderPass::read_write(Buffer& buffer) {
	// TODO
	return *this;
}

void RenderPass::finalize() {
	if (!rg->dirty) {
		return;
	}
	// Create pipelines/push descriptor templates
	switch (type) {
		case PassType::Graphics:
		{
			pipeline->create_gfx_pipeline(gfx_settings->pipeline_settings, descriptor_counts,
										  gfx_settings->color_outputs, gfx_settings->depth_output);
			break;
		}
		case PassType::RT:
		{
			pipeline->create_rt_pipeline(rt_settings->pipeline_settings, descriptor_counts);
			// Create descriptor pool and sets
			if (!tlas_descriptor_pool) {
				auto pool_size = vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1);
				auto descriptor_pool_ci = vk::descriptor_pool_CI(1, &pool_size, 1);

				vk::check(vkCreateDescriptorPool(rg->ctx->device, &descriptor_pool_ci, nullptr, &tlas_descriptor_pool),
						  "Failed to create descriptor pool");
				VkDescriptorSetAllocateInfo set_allocate_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
				set_allocate_info.descriptorPool = tlas_descriptor_pool;
				set_allocate_info.descriptorSetCount = 1;
				set_allocate_info.pSetLayouts = &pipeline->tlas_layout;
				vkAllocateDescriptorSets(rg->ctx->device, &set_allocate_info, &tlas_descriptor_set);
			}
			auto descriptor_write = vk::write_descriptor_set(tlas_descriptor_set, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
															 0, &tlas_info);
			vkUpdateDescriptorSets(rg->ctx->device, 1, &descriptor_write, 0, nullptr);
			break;
		}
		case PassType::Compute:
		{
			pipeline->create_compute_pipeline(compute_settings->pipeline_settings, descriptor_counts);
			break;
		}
		default:
			break;
	}
	// Fill in descriptor infos
	for (int i = 0; i < bound_resources.size(); i++) {
		descriptor_infos[i] = bound_resources[i].buf ?
			DescriptorInfo(bound_resources[i].buf->descriptor) :
			(bound_resources[i].sampler ?
			 DescriptorInfo(bound_resources[i].tex->descriptor(bound_resources[i].sampler)) :
			 DescriptorInfo(bound_resources[i].tex->descriptor()));
	}
}

void RenderPass::run(VkCommandBuffer cmd) {

	// Transition layouts inside the pass
	for (auto& [tex, dst_layout] : layout_transitions) {
		tex->transition_without_state(cmd, dst_layout);
	}

	std::vector<VkEvent> wait_events;
	wait_events.resize(wait_signals_buffer.size());
	// Wait: Buffer
	auto& buffer_sync = rg->buffer_sync_resources[pass_idx];
	auto& img_sync = rg->img_sync_resources[pass_idx];
	int i = 0;
	for (const auto& [k, v] : wait_signals_buffer) {
		LUMEN_ASSERT(rg->passes[v.opposing_pass_idx].set_signals_buffer[k].event, "Event can't be null");
		buffer_sync.buffer_bariers[i] = buffer_barrier2(k, v.src_access_flags, v.dst_access_flags,
														get_pipeline_stage(rg->passes[v.opposing_pass_idx].type),
														get_pipeline_stage(type));
		buffer_sync.dependency_infos[i] = vk::dependency_info(1, &buffer_sync.buffer_bariers[i]);
		wait_events.push_back(rg->passes[v.opposing_pass_idx].set_signals_buffer[k].event);
		i++;
	}
	if (wait_events.size()) {
	
		vkCmdWaitEvents2(cmd, (uint32_t)wait_events.size(), wait_events.data(), buffer_sync.dependency_infos.data());
		for (int i = 0; i < wait_events.size(); i++) {
			vkCmdResetEvent2(cmd, wait_events[i], buffer_sync.buffer_bariers[i].dstStageMask);
		}
	}
	// Wait: Images
	wait_events.clear();
	i = 0;
	for (const auto& [k, v] : wait_signals_img) {
		LUMEN_ASSERT(rg->passes[v.opposing_pass_idx].set_signals_img[k].event, "Event can't be null");
		auto src_access_flags = vk::access_flags_for_img_layout(v.old_layout);
		auto dst_access_flags = vk::access_flags_for_img_layout(v.new_layout);
		auto src_stage = get_pipeline_stage(rg->passes[v.opposing_pass_idx].type);
		auto dst_stage = get_pipeline_stage(type);
		img_sync.img_barriers[i] = image_barrier2(k,
												  src_access_flags,
												  dst_access_flags,
												  v.old_layout,
												  v.new_layout,
												  v.image_aspect,
												  src_stage,
												  dst_stage,
												  rg->ctx->indices.gfx_family.value());
		img_sync.dependency_infos[i] = vk::dependency_info(1, &img_sync.img_barriers[i]);
		wait_events.push_back(rg->passes[v.opposing_pass_idx].set_signals_img[k].event);
		i++;
	}
	if (wait_events.size()) {

		vkCmdWaitEvents2(cmd, (uint32_t)wait_events.size(), wait_events.data(), img_sync.dependency_infos.data());
		for (int i = 0; i < wait_events.size(); i++) {
			vkCmdResetEvent2(cmd, wait_events[i], img_sync.img_barriers[i].dstStageMask);
		}
	}


	// Push descriptors
	if (bound_resources.size()) {
		vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipeline->update_template, pipeline->pipeline_layout, 0, descriptor_infos);
	}
	// Push constants
	if (push_constant.size) {
		vkCmdPushConstants(cmd, pipeline->pipeline_layout, pipeline->pc_stages, 0, push_constant.size, push_constant.data);
	}
	// Run
	switch (type) {
		case PassType::RT:
		{

			LUMEN_ASSERT(tlas_descriptor_set, "TLAS descriptor set cannot be NULL!");
			// This doesnt work because we can't push TLAS descriptor with template...
			 //vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipeline->rt_update_template, pipeline->pipeline_layout, 0, &tlas_buffer.descriptor);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
									pipeline->pipeline_layout, 1, 1, &tlas_descriptor_set, 0, nullptr);


			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline->handle);

			auto& regions = pipeline->get_rt_regions();
			auto& dims = rt_settings->dims;
			vkCmdTraceRaysKHR(cmd, &regions[0], &regions[1], &regions[2], &regions[3], dims.x, dims.y, dims.z);
			break;
		}
		case PassType::Compute:
		{
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->handle);
			auto& dims = compute_settings->dims;
			vkCmdDispatch(cmd, dims.x, dims.y, dims.z);
			break;

		}
		case PassType::Graphics:
		{
			auto& color_outputs = gfx_settings->color_outputs;
			auto& depth_output = gfx_settings->depth_output;
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->handle);

			auto& width = gfx_settings->width;
			auto& height = gfx_settings->height;
			VkViewport viewport = vk::viewport((float)width, (float)height, 0.0f, 1.0f);
			VkRect2D scissor = vk::rect2D(width, height, 0, 0);
			vkCmdSetViewport(cmd, 0, 1, &viewport);
			vkCmdSetScissor(cmd, 0, 1, &scissor);

			if (gfx_settings->pipeline_settings.vertex_buffers.size()) {
				std::vector<VkDeviceSize> offsets(gfx_settings->pipeline_settings.vertex_buffers.size(), 0);
				std::vector<VkBuffer> vert_buffers(gfx_settings->pipeline_settings.vertex_buffers.size(), 0);
				for (auto& buf : gfx_settings->pipeline_settings.vertex_buffers) {
					vert_buffers[i] = buf->handle;
				}
				vkCmdBindVertexBuffers(cmd, 0, vert_buffers.size(),
									   vert_buffers.data(), offsets.data());
			}

			if (gfx_settings->pipeline_settings.index_buffer) {
				vkCmdBindIndexBuffer(cmd, gfx_settings->pipeline_settings.index_buffer->handle, 0,
									 gfx_settings->pipeline_settings.index_type);
			}
			std::vector<VkRenderingAttachmentInfo> rendering_attachments;
			rendering_attachments.reserve(color_outputs.size());
			for (Texture2D* color_output : color_outputs) {
				color_output->transition(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
				rendering_attachments.push_back(
					vk::rendering_attachment_info(
					color_output->img_view,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR,
					VK_ATTACHMENT_STORE_OP_STORE, gfx_settings->clear_color)
				);
			}
			VkRenderingAttachmentInfo depth_stencil_attachment;
			if (depth_output) {
				depth_output->transition(cmd, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
				depth_stencil_attachment =
					vk::rendering_attachment_info(
					depth_output->img_view,
					VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
					gfx_settings->clear_depth_stencil);
			}

			// Render
			{
				VkRenderingInfo render_info{
							.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
							.renderArea = {0, 0, gfx_settings->width, gfx_settings->height},
							.layerCount = 1,
							.colorAttachmentCount = (uint32_t)color_outputs.size(),
							.pColorAttachments = rendering_attachments.data(),
							.pDepthAttachment = depth_output ? &depth_stencil_attachment : nullptr
				};
				vkCmdBeginRendering(cmd, &render_info);
				gfx_settings->pass_func(cmd);
				vkCmdEndRendering(cmd);
			}

			// Present
			for (Texture2D* color_output : color_outputs) {
				if (color_output->present) {
					color_output->transition(cmd, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
				}
			}
			if (depth_output && depth_output->present) {
				depth_output->transition(cmd, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
			}
			break;
		}
		default:
			break;
	}

	// Set: Buffer
	for (const auto& [k, v] : set_signals_buffer) {
		LUMEN_ASSERT(v.event == nullptr, "VkEvent should be null in the setter");
		VkBufferMemoryBarrier2 mem_barrier = buffer_barrier2(k, v.src_access_flags, v.dst_access_flags,
															 get_pipeline_stage(type),
															 get_pipeline_stage(rg->passes[v.opposing_pass_idx].type));
		VkDependencyInfo dependency_info = vk::dependency_info(1, &mem_barrier);
		set_signals_buffer[k].event = rg->event_pool.get_event(rg->ctx->device, cmd);
		vkCmdSetEvent2(cmd, set_signals_buffer[k].event, &dependency_info);
	}

	// Set: Images
	for (const auto& [k, v] : set_signals_img) {
		LUMEN_ASSERT(v.event == nullptr, "VkEvent should be null in the setter");
		auto mem_barrier = image_barrier2(k,
										  vk::access_flags_for_img_layout(v.old_layout),
										  vk::access_flags_for_img_layout(v.new_layout),
										  v.old_layout,
										  v.new_layout,
										  v.image_aspect,
										  get_pipeline_stage(type),
										  get_pipeline_stage(rg->passes[v.opposing_pass_idx].type),
										  rg->ctx->indices.gfx_family.value());

		VkDependencyInfo dependency_info = vk::dependency_info(1, &mem_barrier);
		set_signals_img[k].event = rg->event_pool.get_event(rg->ctx->device, cmd);
		vkCmdSetEvent2(cmd, set_signals_img[k].event, &dependency_info);
	}
}


void RenderGraph::run(VkCommandBuffer cmd) {
	buffer_sync_resources.resize(passes.size());
	img_sync_resources.resize(passes.size());
	for (int i = 0; i < passes.size(); i++) {
		buffer_sync_resources[i].buffer_bariers.resize(passes[i].wait_signals_buffer.size());
		buffer_sync_resources[i].dependency_infos.resize(passes[i].wait_signals_buffer.size());
		img_sync_resources[i].img_barriers.resize(passes[i].wait_signals_img.size());
		img_sync_resources[i].dependency_infos.resize(passes[i].wait_signals_img.size());
		passes[i].run(cmd);
	}
	dirty = false;
}

void RenderGraph::reset(VkCommandBuffer cmd) {
	event_pool.reset_events(ctx->device, cmd);
	for (int i = 0; i < passes.size(); i++) {
		passes[i].set_signals_buffer.clear();
		passes[i].wait_signals_buffer.clear();
		passes[i].set_signals_img.clear();
		passes[i].wait_signals_img.clear();
		passes[i].layout_transitions.clear();

	}
	buffer_sync_resources.clear();
	img_sync_resources.clear();
}
