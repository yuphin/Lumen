#include "LumenPCH.h"
#include "RenderGraph.h"
#include "Utils.h"

#define DIRTY_CHECK(x) if(!x) {return *this;}
static 	VkPipelineStageFlags get_pipeline_stage(PassType pass_type, VkAccessFlags access_flags) {
	VkPipelineStageFlags res = 0;
	switch (pass_type) {
		case PassType::Compute:
			res = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			break;
		case PassType::Graphics:
			res = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
			break;
		case PassType::RT:
			res = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
			break;
		default:
			res = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			break;
	}
	if ((access_flags & VK_ACCESS_TRANSFER_READ_BIT) || (access_flags & VK_ACCESS_TRANSFER_WRITE_BIT)) {
		res |= VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	return res;
}

void RenderPass::register_dependencies(Buffer& buffer, VkAccessFlags dst_access_flags) {
	const bool found = rg->buffer_resource_map.find(buffer.handle) != rg->buffer_resource_map.end();
	if (!found || (dst_access_flags == VK_ACCESS_SHADER_READ_BIT
		&& (rg->buffer_resource_map[buffer.handle].second == dst_access_flags))) {
		return;
	}
	RenderPass& opposing_pass = rg->passes[rg->buffer_resource_map[buffer.handle].first];
	// Invariant : Pass with lower index should be the setter
	// Set current pass dependencies
	auto src_access_flags = rg->buffer_resource_map[buffer.handle].second;
	if (src_access_flags & VK_ACCESS_TRANSFER_WRITE_BIT) {
		dst_access_flags |= VK_ACCESS_TRANSFER_WRITE_BIT;
	}

	if (opposing_pass.pass_idx < pass_idx) {
		if (wait_signals_buffer.find(buffer.handle) == wait_signals_buffer.end()) {
			wait_signals_buffer[buffer.handle] = BufferSyncDescriptor{
			   .src_access_flags = src_access_flags,
			   .dst_access_flags = dst_access_flags,
			   .opposing_pass_idx = opposing_pass.pass_idx,
			};
		}
		// Set source pass dependencies (Signalling pass)
		if (opposing_pass.set_signals_buffer.find(buffer.handle) == opposing_pass.set_signals_buffer.end()) {
			opposing_pass.set_signals_buffer[buffer.handle] = BufferSyncDescriptor{
				.src_access_flags = src_access_flags,
				.dst_access_flags = dst_access_flags,
				.opposing_pass_idx = pass_idx
			};
		}
	} else {
		buffer_barriers.push_back({ buffer.handle, src_access_flags, dst_access_flags });
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
	Pipeline* pipeline;
	uint32_t pass_idx = (uint32_t)passes.size();
	bool cached = false;
	if (pipeline_cache.find(name) != pipeline_cache.end()) {
		auto& storage = pipeline_cache[name];
		if (!recording && storage.pass_idxs.size()) {
			auto idx = storage.pass_idxs[storage.offset_idx];
			++storage.offset_idx;
			return passes[idx];
		}
		pipeline = pipeline_cache[name].pipeline.get();
		cached = true;
	} else {
		pipeline_cache[name] = { std::make_unique<Pipeline>(ctx, pass_idx, name) };
		pipeline = pipeline_cache[name].pipeline.get();
	}
	passes.emplace_back(PassType::RT, pipeline, name, this, pass_idx, settings, cached);
	auto& pass = passes.back();
	std::vector<std::future<Shader*>> shader_tasks;
	shader_tasks.reserve(pass.rt_settings->shaders.size());
	for (auto& shader : pass.rt_settings->shaders) {
		if (shader_cache.find(shader.filename) != shader_cache.end()) {
			shader = shader_cache[shader.filename];
		} else {
			shader_tasks.push_back(ThreadPool::submit([](Shader* shader) {shader->compile(); return shader; }, &shader));
			shader_cache[shader.filename] = shader;
		}
	}
	for (auto& task : shader_tasks) {
		auto shader = task.get();
		shader_cache[shader->filename] = *shader;
	}
	return pass;
}

RenderPass& RenderGraph::add_gfx(const std::string& name, const GraphicsPassSettings& settings) {
	Pipeline* pipeline;
	uint32_t pass_idx = (uint32_t)passes.size();
	bool cached = false;
	if (pipeline_cache.find(name) != pipeline_cache.end()) {
		auto& storage = pipeline_cache[name];
		if (!recording && storage.pass_idxs.size()) {
			auto idx = storage.pass_idxs[storage.offset_idx];
			++storage.offset_idx;
			auto& curr_pass = passes[idx];
			curr_pass.gfx_settings->color_outputs = settings.color_outputs;
			curr_pass.gfx_settings->depth_output = settings.depth_output;
			return curr_pass;
		}
		pipeline = pipeline_cache[name].pipeline.get();
		cached = true;
	} else {
		pipeline_cache[name] = { std::make_unique<Pipeline>(ctx, pass_idx, name) };
		pipeline = pipeline_cache[name].pipeline.get();
	}
	passes.emplace_back(PassType::Graphics, pipeline, name, this, pass_idx, settings, cached);
	auto& pass = passes.back();
	std::vector<std::future<Shader*>> shader_tasks;
	shader_tasks.reserve(pass.gfx_settings->shaders.size());
	for (auto& shader : pass.gfx_settings->shaders) {
		if (shader_cache.find(shader.filename) != shader_cache.end()) {
			shader = shader_cache[shader.filename];
		} else {
			shader_tasks.push_back(ThreadPool::submit([](Shader* shader) {shader->compile(); return shader; }, &shader));
			shader_cache[shader.filename] = shader;
		}
	}
	for (auto& task : shader_tasks) {
		auto shader = task.get();
		shader_cache[shader->filename] = *shader;
	}
	return pass;
}

RenderPass& RenderGraph::add_compute(const std::string& name, const ComputePassSettings& settings) {
	Pipeline* pipeline;
	uint32_t pass_idx = (uint32_t)passes.size();
	bool cached = false;
	if (pipeline_cache.find(name) != pipeline_cache.end()) {
		auto& storage = pipeline_cache[name];
		if (!recording && storage.pass_idxs.size()) {
			auto idx = storage.pass_idxs[storage.offset_idx];
			++storage.offset_idx;
			return passes[idx];
		}
		pipeline = pipeline_cache[name].pipeline.get();
		cached = true;
	} else {
		pipeline_cache[name] = { std::make_unique<Pipeline>(ctx, pass_idx, name) };
		pipeline = pipeline_cache[name].pipeline.get();
	}
	passes.emplace_back(PassType::Compute, pipeline, name, this, pass_idx, settings, cached);
	auto& pass = passes.back();
	auto& shader = pass.compute_settings->shader;
	if (shader_cache.find(shader.filename) != shader_cache.end()) {
		shader = shader_cache[shader.filename];
	} else {
		shader.compile();
		shader_cache[shader.filename] = shader;
	}
	return pass;
}

RenderPass& RenderGraph::get_current_pass() {
	LUMEN_ASSERT(passes.size(), "Make sure there are passes in the render graph");
	return passes.back();

}

RenderPass& RenderPass::bind(const ResourceBinding& binding) {
	DIRTY_CHECK(rg->recording);
	bound_resources.push_back(binding);
	descriptor_counts.push_back(1);
	return *this;

}

RenderPass& RenderPass::bind(std::initializer_list<ResourceBinding> bindings) {
	DIRTY_CHECK(rg->recording);
	bound_resources.insert(bound_resources.end(), bindings.begin(), bindings.end());
	for (size_t i = 0; i < bindings.size(); i++) {
		descriptor_counts.push_back(1);
	}
	return *this;

}

RenderPass& RenderPass::bind(const Texture2D& tex, VkSampler sampler) {
	DIRTY_CHECK(rg->recording);
	bound_resources.emplace_back(tex, sampler);
	descriptor_counts.push_back(1);
	return *this;
}

RenderPass& RenderPass::bind_texture_array(const std::vector<Texture2D>& textures) {
	DIRTY_CHECK(rg->recording);
	for (const auto& texture : textures) {
		bound_resources.emplace_back(texture);
	}
	descriptor_counts.push_back((uint32_t)textures.size());
	return *this;
}

RenderPass& RenderPass::bind_buffer_array(const std::vector<Buffer>& buffers) {
	DIRTY_CHECK(rg->recording);
	for (const auto& buffer : buffers) {
		bound_resources.emplace_back(buffer);
	}
	descriptor_counts.push_back((uint32_t)buffers.size());
	return *this;
}

RenderPass& RenderPass::bind_tlas(const AccelKHR& tlas) {
	DIRTY_CHECK(rg->recording);
	tlas_info = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
	tlas_info.accelerationStructureCount = 1;
	tlas_info.pAccelerationStructures = &tlas.accel;
	return *this;
}


RenderPass& RenderPass::read(Buffer& buffer) {
	register_dependencies(buffer, VK_ACCESS_SHADER_READ_BIT);
	rg->buffer_resource_map[buffer.handle] = { pass_idx, VK_ACCESS_SHADER_READ_BIT };
	return *this;
}

RenderPass& RenderPass::read(Texture2D& tex) {

	VkImageLayout target_layout = get_target_img_layout(tex, VK_ACCESS_SHADER_READ_BIT);
	register_dependencies(tex, target_layout);
	rg->img_resource_map[tex.img] = pass_idx;
	return *this;
}

RenderPass& RenderPass::read(std::initializer_list<std::reference_wrapper<Texture2D>> texes) {
	for (Texture2D& tex : texes) {
		read(tex);
	}
	return *this;
}

RenderPass& RenderPass::read(std::initializer_list<std::reference_wrapper<Buffer>> buffers) {
	for (Buffer& buff : buffers) {
		read(buff);
	}
	return *this;
}

RenderPass& RenderPass::write(Buffer& buffer) {
	return write(buffer, VK_ACCESS_SHADER_WRITE_BIT);
}

RenderPass& RenderPass::write(Texture2D& tex) {
	VkImageLayout target_layout = get_target_img_layout(tex, VK_ACCESS_SHADER_WRITE_BIT);
	register_dependencies(tex, target_layout);
	rg->img_resource_map[tex.img] = pass_idx;
	return *this;
}

RenderPass& RenderPass::write(std::initializer_list<std::reference_wrapper<Buffer>> buffers) {
	for (Buffer& buff : buffers) {
		write(buff);
	}
	return *this;
}

RenderPass& RenderPass::write(std::initializer_list<std::reference_wrapper<Texture2D>> texes) {
	for (Texture2D& tex : texes) {
		write(tex);
	}
	return *this;

}

RenderPass& RenderPass::skip_execution() {
	disable_execution = true;
	return *this;
}

RenderPass& RenderPass::write(Buffer& buffer, VkAccessFlags access_flags) {
	register_dependencies(buffer, access_flags);
	rg->buffer_resource_map[buffer.handle] = { pass_idx, access_flags };
	return *this;
}

RenderPass& RenderPass::push_constants(void* data) {
	push_constant_data = data;
	return *this;
}


RenderPass& RenderPass::zero(Buffer& buffer) {
	buffer_zeros.push_back(&buffer);
	write(buffer, VK_ACCESS_TRANSFER_WRITE_BIT);
	return *this;
}

RenderPass& RenderPass::zero(Buffer& buffer, bool cond) {
	if (cond) {
		return zero(buffer);
	}
	return *this;

}

RenderPass& RenderPass::zero(std::initializer_list<std::reference_wrapper<Buffer>> buffers) {
	for (Buffer& buf : buffers) {
		zero(buf);
	}
	return *this;
}

void RenderPass::finalize() {
	if (!rg->recording) {
		return;
	}
	// Create pipelines/push descriptor templates
	if (!cached) {
		switch (type) {
			case PassType::Graphics:
			{
				auto func = [](RenderPass* pass) {
					pass->pipeline->create_gfx_pipeline(*pass->gfx_settings,
														pass->descriptor_counts, pass->gfx_settings->color_outputs,
														pass->gfx_settings->depth_output);
				};
				rg->pipeline_tasks.push_back({ func, pass_idx });

				break;
			}
			case PassType::RT:
			{
				auto func = [](RenderPass* pass) {
					pass->pipeline->create_rt_pipeline(*pass->rt_settings,
													   pass->descriptor_counts);
					// Create descriptor pool and sets
					if (!pass->tlas_descriptor_pool) {
						auto pool_size = vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1);
						auto descriptor_pool_ci = vk::descriptor_pool_CI(1, &pool_size, 1);

						vk::check(vkCreateDescriptorPool(pass->rg->ctx->device, &descriptor_pool_ci, nullptr, &pass->tlas_descriptor_pool),
								  "Failed to create descriptor pool");
						VkDescriptorSetAllocateInfo set_allocate_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
						set_allocate_info.descriptorPool = pass->tlas_descriptor_pool;
						set_allocate_info.descriptorSetCount = 1;
						set_allocate_info.pSetLayouts = &pass->pipeline->tlas_layout;
						vkAllocateDescriptorSets(pass->rg->ctx->device, &set_allocate_info, &pass->tlas_descriptor_set);
					}
					auto descriptor_write = vk::write_descriptor_set(pass->tlas_descriptor_set, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
																	 0, &pass->tlas_info);
					vkUpdateDescriptorSets(pass->rg->ctx->device, 1, &descriptor_write, 0, nullptr);
				};

				rg->pipeline_tasks.push_back({ func, pass_idx });

				break;
			}
			case PassType::Compute:
			{
				auto func = [](RenderPass* pass) {
					pass->pipeline->create_compute_pipeline(*pass->compute_settings,
															pass->descriptor_counts);
				};
				rg->pipeline_tasks.push_back({ func, pass_idx });
				break;
			}
			default:
				break;
		}
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
	std::vector<VkEvent> wait_events;
	wait_events.reserve(wait_signals_buffer.size());
	DebugMarker::begin_region(rg->ctx->device, cmd, name.c_str(), glm::vec4(1.0f, 0.78f, 0.05f, 1.0f));
	// Wait: Buffer
	auto& buffer_sync = rg->buffer_sync_resources[pass_idx];
	auto& img_sync = rg->img_sync_resources[pass_idx];
	int i = 0;
	for (const auto& [k, v] : wait_signals_buffer) {
		LUMEN_ASSERT(rg->passes[v.opposing_pass_idx].set_signals_buffer[k].event, "Event can't be null");
		buffer_sync.buffer_bariers[i] = buffer_barrier2(k, v.src_access_flags, v.dst_access_flags,
														get_pipeline_stage(rg->passes[v.opposing_pass_idx].type, v.src_access_flags),
														get_pipeline_stage(type, v.dst_access_flags));
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

	// Zero out resources
	for (Buffer* buffer : buffer_zeros) {
		vkCmdFillBuffer(cmd, buffer->handle, 0, buffer->size, 0);

	}

	// Buffer barriers
	std::vector<VkBufferMemoryBarrier2> buffer_memory_barriers;
	buffer_memory_barriers.reserve(buffer_barriers.size());
	for (auto& barrier : buffer_barriers) {
		auto curr_stage = get_pipeline_stage(type, barrier.src_access_flags);
		auto dst_stage = get_pipeline_stage(type, barrier.dst_access_flags);
		buffer_memory_barriers.push_back(buffer_barrier2(barrier.buffer,
										 barrier.src_access_flags,
										 barrier.dst_access_flags,
										 curr_stage,
										 dst_stage));
	}
	auto dependency_info = vk::dependency_info((uint32_t)buffer_memory_barriers.size(), buffer_memory_barriers.data());
	vkCmdPipelineBarrier2(cmd, &dependency_info);

	// Wait: Images
	wait_events.clear();
	i = 0;
	for (const auto& [k, v] : wait_signals_img) {
		LUMEN_ASSERT(rg->passes[v.opposing_pass_idx].set_signals_img[k].event, "Event can't be null");
		auto src_access_flags = vk::access_flags_for_img_layout(v.old_layout);
		auto dst_access_flags = vk::access_flags_for_img_layout(v.new_layout);
		auto src_stage = get_pipeline_stage(rg->passes[v.opposing_pass_idx].type, src_access_flags);
		auto dst_stage = get_pipeline_stage(type, dst_access_flags);
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

	// Transition layouts inside the pass
	for (auto& [tex, dst_layout] : layout_transitions) {
		tex->transition_without_state(cmd, dst_layout);
	}

	// Push descriptors
	if (bound_resources.size()) {
		vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipeline->update_template, pipeline->pipeline_layout, 0, descriptor_infos);
	}
	// Push constants
	if (pipeline->push_constant_size) {
		vkCmdPushConstants(cmd, pipeline->pipeline_layout, pipeline->pc_stages, 0, pipeline->push_constant_size, push_constant_data);
	}
	// Run
	if (!disable_execution) {
		switch (type) {
			case PassType::RT:
			{

				LUMEN_ASSERT(tlas_descriptor_set, "TLAS descriptor set cannot be NULL!");
				// This doesnt work because we can't push TLAS descriptor with template...
				 //vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipeline->rt_update_template, pipeline->pipeline_layout, 0, &tlas_buffer.descriptor);
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
										pipeline->pipeline_layout, 1, 1, &tlas_descriptor_set, 0, nullptr);


				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline->handle);

				if (rt_settings->pass_func) {
					rt_settings->pass_func(cmd, *this);
				} else {
					auto& regions = pipeline->get_rt_regions();
					auto& dims = rt_settings->dims;
					vkCmdTraceRaysKHR(cmd, &regions[0], &regions[1], &regions[2], &regions[3], dims.x, dims.y, dims.z);
				}
				break;
			}
			case PassType::Compute:
			{
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->handle);

				if (compute_settings->pass_func) {
					compute_settings->pass_func(cmd, *this);
				} else {
					auto& dims = compute_settings->dims;
					vkCmdDispatch(cmd, dims.x, dims.y, dims.z);
				}
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

				if (gfx_settings->vertex_buffers.size()) {
					std::vector<VkDeviceSize> offsets(gfx_settings->vertex_buffers.size(), 0);
					std::vector<VkBuffer> vert_buffers(gfx_settings->vertex_buffers.size(), 0);
					for (auto& buf : gfx_settings->vertex_buffers) {
						vert_buffers[i] = buf->handle;
					}
					vkCmdBindVertexBuffers(cmd, 0, (uint32_t)vert_buffers.size(),
										   vert_buffers.data(), offsets.data());
				}

				if (gfx_settings->index_buffer) {
					vkCmdBindIndexBuffer(cmd, gfx_settings->index_buffer->handle, 0,
										 gfx_settings->index_type);
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
					gfx_settings->pass_func(cmd, *this);
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
	}


	// Set: Buffer
	for (const auto& [k, v] : set_signals_buffer) {
		LUMEN_ASSERT(v.event == nullptr, "VkEvent should be null in the setter");
		VkBufferMemoryBarrier2 mem_barrier = buffer_barrier2(k, v.src_access_flags, v.dst_access_flags,
															 get_pipeline_stage(type, v.src_access_flags),
															 get_pipeline_stage(rg->passes[v.opposing_pass_idx].type, v.dst_access_flags));
		VkDependencyInfo dependency_info = vk::dependency_info(1, &mem_barrier);
		set_signals_buffer[k].event = rg->event_pool.get_event(rg->ctx->device, cmd);

		vkCmdSetEvent2(cmd, set_signals_buffer[k].event, &dependency_info);
	}

	// Set: Images
	for (const auto& [k, v] : set_signals_img) {
		LUMEN_ASSERT(v.event == nullptr, "VkEvent should be null in the setter");
		auto src_access_flags = vk::access_flags_for_img_layout(v.old_layout);
		auto dst_access_flags = vk::access_flags_for_img_layout(v.new_layout);
		auto mem_barrier = image_barrier2(k,
										  vk::access_flags_for_img_layout(v.old_layout),
										  vk::access_flags_for_img_layout(v.new_layout),
										  v.old_layout,
										  v.new_layout,
										  v.image_aspect,
										  get_pipeline_stage(type, src_access_flags),
										  get_pipeline_stage(rg->passes[v.opposing_pass_idx].type, dst_access_flags),
										  rg->ctx->indices.gfx_family.value());

		VkDependencyInfo dependency_info = vk::dependency_info(1, &mem_barrier);
		set_signals_img[k].event = rg->event_pool.get_event(rg->ctx->device, cmd);
		vkCmdSetEvent2(cmd, set_signals_img[k].event, &dependency_info);
	}
	DebugMarker::end_region(rg->ctx->device, cmd);
}

void RenderGraph::run(VkCommandBuffer cmd) {
	buffer_sync_resources.resize(passes.size());
	img_sync_resources.resize(passes.size());

	if (pipeline_tasks.size()) {
		std::vector<std::future<void>> futures;
		futures.reserve(pipeline_tasks.size());
		for (auto& [task, idx] : pipeline_tasks) {
			futures.push_back(ThreadPool::submit(task, &passes[idx]));
		}
		for (auto& future : futures) {
			future.wait();
		}
	}
	for (int i = 0; i < passes.size(); i++) {
		buffer_sync_resources[i].buffer_bariers.resize(passes[i].wait_signals_buffer.size());
		buffer_sync_resources[i].dependency_infos.resize(passes[i].wait_signals_buffer.size());
		img_sync_resources[i].img_barriers.resize(passes[i].wait_signals_img.size());
		img_sync_resources[i].dependency_infos.resize(passes[i].wait_signals_img.size());
		passes[i].run(cmd);
	}
}

void RenderGraph::reset(VkCommandBuffer cmd) {
	event_pool.reset_events(ctx->device, cmd);
	for (int i = 0; i < passes.size(); i++) {
		passes[i].set_signals_buffer.clear();
		passes[i].wait_signals_buffer.clear();
		passes[i].set_signals_img.clear();
		passes[i].wait_signals_img.clear();
		passes[i].layout_transitions.clear();
		passes[i].buffer_zeros.clear();
		passes[i].buffer_barriers.clear();
		passes[i].disable_execution = false;
	}
	if (recording) {
		for (auto& pass : passes) {
			pipeline_cache[pass.name].pass_idxs.push_back(pass.pass_idx);
			pipeline_cache[pass.name].offset_idx = 0;
		}
		recording = false;
	} else {
		for (auto& [k, v] : pipeline_cache) {
			v.offset_idx = 0;
		}
	}
	if (pipeline_tasks.size()) {
		pipeline_tasks.clear();
	}
	buffer_sync_resources.clear();
	img_sync_resources.clear();
}
