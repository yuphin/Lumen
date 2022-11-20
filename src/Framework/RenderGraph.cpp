#include "LumenPCH.h"
#include "RenderGraph.h"
#include "VkUtils.h"
#include <unordered_set>

// TODO: "Handle" the stupid bug where the multithreaded pipeline compilation
// doesn't show any output from the debugPrintf...
// Possible solution: Make compilation with debugPrintf shaders synchronous?

#define DIRTY_CHECK(x) \
	if (!(x)) {          \
		return *this;  \
	}
static VkPipelineStageFlags get_pipeline_stage(PassType pass_type, VkAccessFlags access_flags) {
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

inline static VkImageLayout get_image_layout(VkDescriptorType type) {
	switch (type) {
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			return VK_IMAGE_LAYOUT_GENERAL;
		default:
			break;
	}
	LUMEN_ERROR("Unhandled image layout case");
	return VK_IMAGE_LAYOUT_GENERAL;
}

void RenderPass::register_dependencies(Buffer& buffer, VkAccessFlags dst_access_flags) {
	const bool found = rg->buffer_resource_map.find(buffer.handle) != rg->buffer_resource_map.end();
	if (!found || (dst_access_flags == VK_ACCESS_SHADER_READ_BIT &&
				   (rg->buffer_resource_map[buffer.handle].second == dst_access_flags))) {
		return;
	}
	RenderPass& opposing_pass = rg->passes[rg->buffer_resource_map[buffer.handle].first];
	if (opposing_pass.submitted) {
		return;
	}
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
			opposing_pass.set_signals_buffer[buffer.handle] = BufferSyncDescriptor{.src_access_flags = src_access_flags,
																				   .dst_access_flags = dst_access_flags,
																				   .opposing_pass_idx = pass_idx};
		}
	} else {
		buffer_barriers.push_back({buffer.handle, src_access_flags, dst_access_flags});
	}
}

void RenderPass::register_dependencies(Texture2D& tex, VkImageLayout dst_layout) {
	if (tex.layout == dst_layout) {
		return;
	}
	if (tex.layout == VK_IMAGE_LAYOUT_UNDEFINED || rg->img_resource_map.find(tex.img) == rg->img_resource_map.end()) {
		layout_transitions.push_back({&tex, tex.layout, dst_layout});
		tex.layout = dst_layout;
	} else {
		RenderPass& opposing_pass = rg->passes[rg->img_resource_map[tex.img]];
		if (opposing_pass.submitted) {
			return;
		}
		if (opposing_pass.pass_idx < pass_idx) {
			// Set current pass dependencies (Waiting pass)
			if (wait_signals_img.find(tex.img) == wait_signals_img.end()) {
				wait_signals_img[tex.img] = ImageSyncDescriptor{.old_layout = tex.layout,
																.new_layout = dst_layout,
																.opposing_pass_idx = opposing_pass.pass_idx,
																.image_aspect = tex.aspect_flags};
			}
			// Set source pass dependencies (Signalling pass)
			if (opposing_pass.set_signals_img.find(tex.img) == opposing_pass.set_signals_img.end()) {
				opposing_pass.set_signals_img[tex.img] = ImageSyncDescriptor{.old_layout = tex.layout,
																			 .new_layout = dst_layout,
																			 .opposing_pass_idx = pass_idx,
																			 .image_aspect = tex.aspect_flags};
			}
			tex.layout = dst_layout;
		} else {
			layout_transitions.push_back({&tex, tex.layout, dst_layout});
			tex.layout = dst_layout;
		}
	}
}

void RenderPass::transition_resources() {
	if (rg->settings.shader_inference) {
		for (auto i = 0; i < bound_resources.size(); i++) {
			auto& bound_resource = bound_resources[i];
			if (!bound_resource.active) {
				if (bound_resource.tex) {
					descriptor_infos[i] =
						bound_resource.tex->descriptor(get_image_layout(pipeline->descriptor_types[i]));
				} else {
					descriptor_infos[i] = bound_resources[i].get_descriptor_info();
				}
				continue;
			}
			if (bound_resource.write) {
				if (bound_resource.buf) {
					write_impl(*bound_resource.buf, VK_ACCESS_SHADER_WRITE_BIT);
				} else {
					write_impl(*bound_resource.tex);
				}
			} else if (bound_resource.read) {
				if (bound_resource.buf) {
					read_impl(*bound_resource.buf);
				} else {
					read_impl(*bound_resource.tex);
				}
			}
			descriptor_infos[i] = bound_resources[i].get_descriptor_info();
		}
		for (auto [buffer, status] : affected_buffer_pointers) {
			if (status.write) {
				write_impl(*buffer, VK_ACCESS_SHADER_WRITE_BIT);
			} else if (status.read) {
				read_impl(*buffer);
			}
		}
	} else {
		for (auto& buf : explicit_buffer_reads) {
			read_impl(*buf);
		}
		for (auto& buf : explicit_buffer_writes) {
			write_impl(*buf, VK_ACCESS_SHADER_WRITE_BIT);
		}
		for (auto& tex : explicit_tex_reads) {
			read_impl(*tex);
		}
		for (auto& tex : explicit_tex_writes) {
			write_impl(*tex);
		}
		for (int i = 0; i < bound_resources.size(); i++) {
			descriptor_infos[i] = bound_resources[i].get_descriptor_info();
		}
	}
	for (const Resource& resource : resource_zeros) {
		if (resource.buf) {
			write_impl(*resource.buf, VK_ACCESS_TRANSFER_WRITE_BIT);
		} else {
			write_impl(*resource.tex);
		}
	}

	for (const auto& [src, dst] : resource_copies) {
		if (src.tex) {
			if (dst.buf) {
				write_impl(*dst.buf, VK_ACCESS_TRANSFER_WRITE_BIT);
			}
		} else { // buffer
			read_impl(*src.buf, VK_ACCESS_TRANSFER_READ_BIT);
			if (dst.buf) {
				write_impl(*dst.buf, VK_ACCESS_TRANSFER_WRITE_BIT);
			}
		}
	}

}

static void build_shaders(RenderPass* pass, const std::vector<Shader*>& active_shaders) {
	// todo: make resource processing in order
	auto process_resources = [pass](const Shader& shader) {
		if (!pass->rg->settings.shader_inference) {
			return;
		}
		for (auto& [k, v] : shader.buffer_status_map) {
			if (v.read) {
				pass->affected_buffer_pointers[k].read = v.read;
			}
			if (v.write) {
				pass->affected_buffer_pointers[k].write = v.write;
			}
		}
	};
	switch (pass->type) {
		case PassType::Graphics: {
			std::vector<std::future<Shader*>> shader_tasks;
			shader_tasks.reserve(pass->gfx_settings->shaders.size());
			for (auto& shader : active_shaders) {
				if (pass->rg->shader_cache.find(shader->filename) != pass->rg->shader_cache.end()) {
					*shader = pass->rg->shader_cache[shader->filename];
				} else {
					shader_tasks.push_back(ThreadPool::submit(
						[pass](Shader* shader) {
							shader->compile(pass);
							return shader;
						},
						shader));
				}
			}
			for (auto& task : shader_tasks) {
				auto shader = task.get();
				{
					std::lock_guard<std::mutex> lock(pass->rg->shader_map_mutex);
					pass->rg->shader_cache[shader->filename] = *shader;
				}
			}
			for (auto& shader : active_shaders) {
				process_resources(*shader);
			}
		} break;
		case PassType::RT: {
			std::vector<std::future<Shader*>> shader_tasks;
			shader_tasks.reserve(pass->rt_settings->shaders.size());
			for (auto& shader : active_shaders) {
				if (pass->rg->shader_cache.find(shader->filename) != pass->rg->shader_cache.end()) {
					*shader = pass->rg->shader_cache[shader->filename];
				} else {
					shader_tasks.push_back(ThreadPool::submit(
						[pass](Shader* shader) {
							shader->compile(pass);
							return shader;
						},
						shader));
					// shader.compile(this);
					pass->rg->shader_cache[shader->filename] = *shader;
				}
			}
			for (auto& task : shader_tasks) {
				auto shader = task.get();
				{
					std::lock_guard<std::mutex> lock(pass->rg->shader_map_mutex);
					pass->rg->shader_cache[shader->filename] = *shader;
				}
			}
			for (auto& shader : active_shaders) {
				process_resources(*shader);
			}

		} break;
		case PassType::Compute: {
			for (auto& shader : active_shaders) {
				if (pass->rg->shader_cache.find(shader->filename) != pass->rg->shader_cache.end()) {
					*shader = pass->rg->shader_cache[shader->filename];
				} else {
					shader->compile(pass);
					{
						std::lock_guard<std::mutex> lock(pass->rg->shader_map_mutex);
						pass->rg->shader_cache[shader->filename] = *shader;
					}
				}
				pass->affected_buffer_pointers = shader->buffer_status_map;
			}
		} break;
		default:
			break;
	}
}

RenderPass& RenderGraph::add_rt(const std::string& name, const RTPassSettings& settings) {
	Pipeline* pipeline;
	uint32_t pass_idx = (uint32_t)passes.size();
	bool cached = false;
	ending_pass_idx++;
	if (pipeline_cache.find(name) != pipeline_cache.end()) {
		auto& storage = pipeline_cache[name];
		if (!recording && storage.pass_idxs.size()) {
			auto idx = storage.pass_idxs[storage.offset_idx];
			passes[idx].active = true;
			if (reload_shaders) {
				if (storage.offset_idx == 0) {
					pipeline_cache[name].pipeline->cleanup();
					pipeline_cache[name].pipeline = std::make_unique<Pipeline>(ctx, name);
				}
				passes[idx].pipeline = pipeline_cache[name].pipeline.get();
			}
			++storage.offset_idx;
			return passes[idx];
		}

		pipeline = pipeline_cache[name].pipeline.get();
		cached = true;
	} else {
		pipeline_cache[name] = {std::make_unique<Pipeline>(ctx, name)};
		pipeline = pipeline_cache[name].pipeline.get();
	}
	passes.emplace_back(PassType::RT, pipeline, name, this, pass_idx, settings, cached);
	return passes.back();
}

RenderPass& RenderGraph::add_gfx(const std::string& name, const GraphicsPassSettings& settings) {
	Pipeline* pipeline;
	uint32_t pass_idx = (uint32_t)passes.size();
	bool cached = false;
	ending_pass_idx++;
	if (pipeline_cache.find(name) != pipeline_cache.end()) {
		auto& storage = pipeline_cache[name];
		if (!recording && storage.pass_idxs.size()) {
			auto idx = storage.pass_idxs[storage.offset_idx];
			auto& curr_pass = passes[idx];
			curr_pass.gfx_settings->color_outputs = settings.color_outputs;
			curr_pass.gfx_settings->depth_output = settings.depth_output;
			curr_pass.active = true;
			if (reload_shaders) {
				if (storage.offset_idx == 0) {
					pipeline_cache[name].pipeline->cleanup();
					pipeline_cache[name].pipeline = std::make_unique<Pipeline>(ctx, name);
				}
				passes[idx].pipeline = pipeline_cache[name].pipeline.get();
			}
			++storage.offset_idx;
			return curr_pass;
		}
		pipeline = pipeline_cache[name].pipeline.get();
		cached = true;
	} else {
		pipeline_cache[name] = {std::make_unique<Pipeline>(ctx, name)};
		pipeline = pipeline_cache[name].pipeline.get();
	}
	passes.emplace_back(PassType::Graphics, pipeline, name, this, pass_idx, settings, cached);
	return passes.back();
}

RenderPass& RenderGraph::add_compute(const std::string& name, const ComputePassSettings& settings) {
	Pipeline* pipeline;
	uint32_t pass_idx = (uint32_t)passes.size();
	bool cached = false;
	ending_pass_idx++;
	if (pipeline_cache.find(name) != pipeline_cache.end()) {
		auto& storage = pipeline_cache[name];
		if (!recording && storage.pass_idxs.size()) {
			auto idx = storage.pass_idxs[storage.offset_idx];
			passes[idx].active = true;
			if (reload_shaders) {
				if (storage.offset_idx == 0) {
					pipeline_cache[name].pipeline->cleanup();
					pipeline_cache[name].pipeline = std::make_unique<Pipeline>(ctx, name);
				}
				passes[idx].pipeline = pipeline_cache[name].pipeline.get();
			}
			++storage.offset_idx;
			return passes[idx];
		}
		pipeline = pipeline_cache[name].pipeline.get();
		cached = true;
	} else {
		pipeline_cache[name] = {std::make_unique<Pipeline>(ctx, name)};
		pipeline = pipeline_cache[name].pipeline.get();
	}
	passes.emplace_back(PassType::Compute, pipeline, name, this, pass_idx, settings, cached);
	return passes.back();
}

RenderPass& RenderPass::bind(const ResourceBinding& binding) {
	if(next_binding_idx >= bound_resources.size()) {
		bound_resources.push_back(binding);
		descriptor_counts.push_back(1);
	} else {
		bound_resources[next_binding_idx].replace(binding);
	}
	next_binding_idx++;
	return *this;
}

RenderPass& RenderPass::bind(std::initializer_list<ResourceBinding> bindings) {
	for (auto& binding : bindings) {
		bind(binding);
	}
	return *this;
}

RenderPass& RenderPass::bind(Texture2D& tex, VkSampler sampler) {
	if (next_binding_idx >= bound_resources.size()) {
		bound_resources.emplace_back(tex, sampler);
		descriptor_counts.push_back(1);
	} else {
		bound_resources[next_binding_idx].replace(tex, sampler);
	}
	next_binding_idx++;
	return *this;
}

RenderPass& RenderPass::bind_texture_array(std::vector<Texture2D>& textures) {
	next_binding_idx++;
	DIRTY_CHECK(rg->recording);
	for (auto& texture : textures) {
		bound_resources.emplace_back(texture);
	}
	descriptor_counts.push_back((uint32_t)textures.size());
	return *this;
}

RenderPass& RenderPass::bind_buffer_array(std::vector<Buffer>& buffers) {
	next_binding_idx++;
	DIRTY_CHECK(rg->recording);
	for (auto& buffer : buffers) {
		bound_resources.emplace_back(buffer);
	}
	descriptor_counts.push_back((uint32_t)buffers.size());
	return *this;
}

RenderPass& RenderPass::bind_tlas(const AccelKHR& tlas) {
	DIRTY_CHECK(rg->recording || rg->reload_shaders);
	pipeline->tlas_info = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
	pipeline->tlas_info.accelerationStructureCount = 1;
	pipeline->tlas_info.pAccelerationStructures = &tlas.accel;
	return *this;
}

RenderPass& RenderPass::read(Buffer& buffer) {
	explicit_buffer_reads.push_back(&buffer);
	return *this;
}

RenderPass& RenderPass::read(Texture2D& tex) {
	explicit_tex_reads.push_back(&tex);
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

RenderPass& RenderPass::read(ResourceBinding& resource) {
	if (resource.tex) {
		read(*resource.tex);
	} else {
		read(*resource.buf);
	}
	return *this;
}

RenderPass& RenderPass::write(Buffer& buffer) {
	explicit_buffer_writes.push_back(&buffer);
	return *this;
}

RenderPass& RenderPass::write(Texture2D& tex) {
	explicit_tex_writes.push_back(&tex);
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

RenderPass& RenderPass::write(ResourceBinding& resource) {
	if (resource.tex) {
		write(*resource.tex);
	} else {
		write(*resource.buf);
	}
	return *this;
}

RenderPass& RenderPass::skip_execution() {
	disable_execution = true;
	return *this;
}

RenderPass& RenderPass::zero(const Resource& resource) {
	if (resource.tex) {
		LUMEN_ERROR("Unimplemented: Immage zeroing")
	}
	resource_zeros.push_back(resource);
	return *this;
}

RenderPass& RenderPass::zero(const Resource& resource, bool cond) {
	if (cond) {
		return zero(resource);
	}
	return *this;
}

RenderPass& RenderPass::zero(std::initializer_list<std::reference_wrapper<Buffer>> buffers) {
	for (Buffer& buf : buffers) {
		zero(buf);
	}
	return *this;
}

RenderPass& RenderPass::zero(std::initializer_list<std::reference_wrapper<Texture2D>> textures) {
	for (Texture2D& tex : textures) {
		zero(tex);
	}
	return *this;
}

RenderPass& RenderPass::copy(const Resource& src, const Resource& dst) {
	// TODO: Extend this if check upon extending this function
	if ((src.tex || src.buf) && dst.buf) {
		if (dst.buf) {
			resource_copies.push_back({ src,dst });
		}
	} else {
		LUMEN_ERROR("Unimplemented copy operation")
	}

	return *this;
}

void RenderPass::finalize() {
	if (!rg->recording && !rg->reload_shaders) {
		// Handle resource transitions
		transition_resources();
		return;
	}

	// Create pipelines/push descriptor templates
	if (!is_pipeline_cached) {
		switch (type) {
			case PassType::Graphics: {
				auto func = [](RenderPass* pass) {
					pass->pipeline->create_gfx_pipeline(*pass->gfx_settings, pass->descriptor_counts,
														pass->gfx_settings->color_outputs,
														pass->gfx_settings->depth_output);
				};
				rg->pipeline_tasks.push_back({func, pass_idx});
				break;
			}
			case PassType::RT: {
				auto func = [](RenderPass* pass) {
					pass->pipeline->create_rt_pipeline(*pass->rt_settings, pass->descriptor_counts);
					// Create descriptor pool and sets
					if (!pass->pipeline->tlas_descriptor_pool) {
						auto pool_size = vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1);
						auto descriptor_pool_ci = vk::descriptor_pool_CI(1, &pool_size, 1);

						vk::check(vkCreateDescriptorPool(pass->rg->ctx->device, &descriptor_pool_ci, nullptr,
														 &pass->pipeline->tlas_descriptor_pool),
								  "Failed to create descriptor pool");
						VkDescriptorSetAllocateInfo set_allocate_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
						set_allocate_info.descriptorPool = pass->pipeline->tlas_descriptor_pool;
						set_allocate_info.descriptorSetCount = 1;
						set_allocate_info.pSetLayouts = &pass->pipeline->tlas_layout;
						vkAllocateDescriptorSets(pass->rg->ctx->device, &set_allocate_info, &pass->pipeline->tlas_descriptor_set);
					}
					auto descriptor_write = vk::write_descriptor_set(
						pass->pipeline->tlas_descriptor_set, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 0, &pass->pipeline->tlas_info);
					vkUpdateDescriptorSets(pass->rg->ctx->device, 1, &descriptor_write, 0, nullptr);
				};
				// TODO: Fix multithreaded pipeline building for RT
				 //func(this);
				rg->pipeline_tasks.push_back({func, pass_idx});
				break;
			}
			case PassType::Compute: {
				auto func = [](RenderPass* pass) {
					pass->pipeline->create_compute_pipeline(*pass->compute_settings, pass->descriptor_counts);
				};
				 //func(this);
				rg->pipeline_tasks.push_back({func, pass_idx});
				break;
			}
			default:
				break;
		}
	} else {
		rg->pipeline_tasks.push_back({ nullptr , pass_idx});
	}
}

void RenderPass::write_impl(Buffer& buffer, VkAccessFlags access_flags) {
	register_dependencies(buffer, access_flags);
	rg->buffer_resource_map[buffer.handle] = {pass_idx, access_flags};
}


void RenderPass::write_impl(Texture2D& tex) {
	VkImageLayout target_layout = get_target_img_layout(tex, VK_ACCESS_SHADER_WRITE_BIT);
	register_dependencies(tex, target_layout);
	rg->img_resource_map[tex.img] = pass_idx;
}

void RenderPass::read_impl(Buffer& buffer) {
	read_impl(buffer, VK_ACCESS_SHADER_READ_BIT);
}

void RenderPass::read_impl(Buffer& buffer, VkAccessFlags access_flags) {
	register_dependencies(buffer, access_flags);
	rg->buffer_resource_map[buffer.handle] = {pass_idx, access_flags };
}

void RenderPass::read_impl(Texture2D& tex) {
	VkImageLayout target_layout = get_target_img_layout(tex, VK_ACCESS_SHADER_READ_BIT);
	register_dependencies(tex, target_layout);
	rg->img_resource_map[tex.img] = pass_idx;
}

void RenderPass::run(VkCommandBuffer cmd) {
	std::vector<VkEvent> wait_events;
	const bool use_events = rg->settings.use_events;
	if (use_events) {
		wait_events.reserve(wait_signals_buffer.size());
	}
	DebugMarker::begin_region(rg->ctx->device, cmd, name.c_str(), glm::vec4(1.0f, 0.78f, 0.05f, 1.0f));
	// Wait: Buffer
	auto& buffer_sync = rg->buffer_sync_resources[pass_idx];
	auto& img_sync = rg->img_sync_resources[pass_idx];
	int i = 0;
	for (const auto& [k, v] : wait_signals_buffer) {
		if (use_events) {
			LUMEN_ASSERT(rg->passes[v.opposing_pass_idx].set_signals_buffer[k].event, "Event can't be null");
		}
		buffer_sync.buffer_bariers[i] =
			buffer_barrier2(k, v.src_access_flags, v.dst_access_flags,
							get_pipeline_stage(rg->passes[v.opposing_pass_idx].type, v.src_access_flags),
							get_pipeline_stage(type, v.dst_access_flags));
		buffer_sync.dependency_infos[i] = vk::dependency_info(1, &buffer_sync.buffer_bariers[i]);
		if (use_events) {
			wait_events.push_back(rg->passes[v.opposing_pass_idx].set_signals_buffer[k].event);
		}
		i++;
	}
	if (wait_events.size()) {
		vkCmdWaitEvents2(cmd, (uint32_t)wait_events.size(), wait_events.data(), buffer_sync.dependency_infos.data());
		for (int i = 0; i < wait_events.size(); i++) {
			vkCmdResetEvent2(cmd, wait_events[i], buffer_sync.buffer_bariers[i].dstStageMask);
		}
	} else if (!use_events) {
		for (const auto& info : buffer_sync.dependency_infos) {
			vkCmdPipelineBarrier2(cmd, &info);
		}
	}

	// Zero out resources
	for (const Resource& resource : resource_zeros) {
		if (resource.buf) {
			vkCmdFillBuffer(cmd, resource.buf->handle, 0, resource.buf->size, 0);
		}
	}

	// Buffer barriers
	std::vector<VkBufferMemoryBarrier2> buffer_memory_barriers;
	buffer_memory_barriers.reserve(buffer_barriers.size());
	for (auto& barrier : buffer_barriers) {
		auto curr_stage = get_pipeline_stage(type, barrier.src_access_flags);
		auto dst_stage = get_pipeline_stage(type, barrier.dst_access_flags);
		buffer_memory_barriers.push_back(
			buffer_barrier2(barrier.buffer, barrier.src_access_flags, barrier.dst_access_flags, curr_stage, dst_stage));
	}
	auto dependency_info = vk::dependency_info((uint32_t)buffer_memory_barriers.size(), buffer_memory_barriers.data());
	vkCmdPipelineBarrier2(cmd, &dependency_info);

	// Wait: Images
	wait_events.clear();
	i = 0;
	for (const auto& [k, v] : wait_signals_img) {
		if (use_events) {
			LUMEN_ASSERT(rg->passes[v.opposing_pass_idx].set_signals_img[k].event, "Event can't be null");
		}
		auto src_access_flags = vk::access_flags_for_img_layout(v.old_layout);
		auto dst_access_flags = vk::access_flags_for_img_layout(v.new_layout);
		auto src_stage = get_pipeline_stage(rg->passes[v.opposing_pass_idx].type, src_access_flags);
		auto dst_stage = get_pipeline_stage(type, dst_access_flags);
		img_sync.img_barriers[i] =
			image_barrier2(k, src_access_flags, dst_access_flags, v.old_layout, v.new_layout, v.image_aspect, src_stage,
						   dst_stage, rg->ctx->indices.gfx_family.value());
		img_sync.dependency_infos[i] = vk::dependency_info(1, &img_sync.img_barriers[i]);
		if (use_events) {
			wait_events.push_back(rg->passes[v.opposing_pass_idx].set_signals_img[k].event);
		}
		i++;
	}

	if (wait_events.size()) {
		vkCmdWaitEvents2(cmd, (uint32_t)wait_events.size(), wait_events.data(), img_sync.dependency_infos.data());
		for (int i = 0; i < wait_events.size(); i++) {
			vkCmdResetEvent2(cmd, wait_events[i], img_sync.img_barriers[i].dstStageMask);
		}
	} else if (!use_events) {
		for (const auto& info : img_sync.dependency_infos) {
			vkCmdPipelineBarrier2(cmd, &info);
		}
	}

	// Transition layouts inside the pass
	for (auto& [tex, old_layout, dst_layout] : layout_transitions) {
		tex->force_transition(cmd, old_layout, dst_layout);
	}

	// Push descriptors
	if (bound_resources.size()) {
		vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipeline->update_template, pipeline->pipeline_layout, 0,
											  descriptor_infos);
	}
	// Push constants
	if (pipeline->push_constant_size) {
		vkCmdPushConstants(cmd, pipeline->pipeline_layout, pipeline->pc_stages, 0, pipeline->push_constant_size,
						   push_constant_data);
	}
	// Run
	if (!disable_execution) {
		switch (type) {
			case PassType::RT: {
				LUMEN_ASSERT(pipeline->tlas_descriptor_set, "TLAS descriptor set cannot be NULL!");
				// This doesnt work because we can't push TLAS descriptor with
				// template...
				// vkCmdPushDescriptorSetWithTemplateKHR(cmd,
				// pipeline->rt_update_template, pipeline->pipeline_layout, 0,
				// &tlas_buffer.descriptor);
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline->pipeline_layout, 1, 1,
										&pipeline->tlas_descriptor_set, 0, nullptr);

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
			case PassType::Compute: {
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->handle);

				if (compute_settings->pass_func) {
					compute_settings->pass_func(cmd, *this);
				} else {
					auto& dims = compute_settings->dims;
					vkCmdDispatch(cmd, dims.x, dims.y, dims.z);
				}
				break;
			}
			case PassType::Graphics: {
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
					vkCmdBindVertexBuffers(cmd, 0, (uint32_t)vert_buffers.size(), vert_buffers.data(), offsets.data());
				}

				if (gfx_settings->index_buffer) {
					vkCmdBindIndexBuffer(cmd, gfx_settings->index_buffer->handle, 0, gfx_settings->index_type);
				}
				std::vector<VkRenderingAttachmentInfo> rendering_attachments;
				rendering_attachments.reserve(color_outputs.size());
				for (Texture2D* color_output : color_outputs) {
					color_output->transition(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
					rendering_attachments.push_back(vk::rendering_attachment_info(
						color_output->img_view, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR,
						VK_ATTACHMENT_STORE_OP_STORE, gfx_settings->clear_color));
				}
				VkRenderingAttachmentInfo depth_stencil_attachment;
				if (depth_output) {
					depth_output->transition(cmd, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
					depth_stencil_attachment = vk::rendering_attachment_info(
						depth_output->img_view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR,
						VK_ATTACHMENT_STORE_OP_STORE, gfx_settings->clear_depth_stencil);
				}

				// Render
				{
					VkRenderingInfo render_info{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
												.renderArea = {{0, 0}, {gfx_settings->width, gfx_settings->height}},
												.layerCount = 1,
												.colorAttachmentCount = (uint32_t)color_outputs.size(),
												.pColorAttachments = rendering_attachments.data(),
												.pDepthAttachment = depth_output ? &depth_stencil_attachment : nullptr};
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

	for (const auto& [src, dst] : resource_copies) {
		if (src.tex) {
			if (dst.buf) {
				// Assumption: The copy(...) is called in the pass after the src is produced
				VkBufferImageCopy region = {};
				region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.imageSubresource.mipLevel = 0;
				region.imageSubresource.baseArrayLayer = 0;
				region.imageSubresource.layerCount = 1;
				region.imageExtent = src.tex->base_extent;
				VkImageLayout old_layout = src.tex->layout;
				src.tex->transition(cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
				vkCmdCopyImageToBuffer(cmd, src.tex->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
									   dst.buf->handle, 1, &region);
				src.tex->transition(cmd, old_layout);
			}
		} else { // buffer
			if (dst.buf) {
				VkBufferCopy copy_region = {.size = src.buf->size};
				vkCmdCopyBuffer(cmd, src.buf->handle, dst.buf->handle, 1, &copy_region);
			}
		}
	}

	// Set: Buffer
	for (const auto& [k, v] : set_signals_buffer) {
		LUMEN_ASSERT(v.event == nullptr, "VkEvent should be null in the setter");
		VkBufferMemoryBarrier2 mem_barrier =
			buffer_barrier2(k, v.src_access_flags, v.dst_access_flags, get_pipeline_stage(type, v.src_access_flags),
							get_pipeline_stage(rg->passes[v.opposing_pass_idx].type, v.dst_access_flags));
		VkDependencyInfo dependency_info = vk::dependency_info(1, &mem_barrier);

		if (use_events) {
			set_signals_buffer[k].event = rg->event_pool.get_event(rg->ctx->device, cmd);
			vkCmdSetEvent2(cmd, set_signals_buffer[k].event, &dependency_info);
		}
	}

	// Set: Images
	for (const auto& [k, v] : set_signals_img) {
		LUMEN_ASSERT(v.event == nullptr, "VkEvent should be null in the setter");
		auto src_access_flags = vk::access_flags_for_img_layout(v.old_layout);
		auto dst_access_flags = vk::access_flags_for_img_layout(v.new_layout);
		auto mem_barrier = image_barrier2(k, vk::access_flags_for_img_layout(v.old_layout),
										  vk::access_flags_for_img_layout(v.new_layout), v.old_layout, v.new_layout,
										  v.image_aspect, get_pipeline_stage(type, src_access_flags),
										  get_pipeline_stage(rg->passes[v.opposing_pass_idx].type, dst_access_flags),
										  rg->ctx->indices.gfx_family.value());

		VkDependencyInfo dependency_info = vk::dependency_info(1, &mem_barrier);
		if (use_events) {
			set_signals_img[k].event = rg->event_pool.get_event(rg->ctx->device, cmd);
			vkCmdSetEvent2(cmd, set_signals_img[k].event, &dependency_info);
		}
	}
	DebugMarker::end_region(rg->ctx->device, cmd);
}

void RenderGraph::run(VkCommandBuffer cmd) {
	buffer_sync_resources.resize(passes.size());
	img_sync_resources.resize(passes.size());

	// Compile shaders and process resources
	if (recording || reload_shaders) {
		auto cmp = [](const std::pair<Shader*, RenderPass*>& a, const std::pair<Shader*, RenderPass*>& b) {
			return a.first->filename < b.first->filename;
		};
		std::set<std::pair<Shader*, RenderPass*>, decltype(cmp)> unique_shaders_set;
		std::unordered_map<RenderPass*, std::vector<Shader*>> unique_shaders;
		std::unordered_map<RenderPass*, std::vector<Shader*>> existing_shaders;
		
		// Recording or reload -> The pass is active by default
		for (auto i = beginning_pass_idx; i < ending_pass_idx; i++) {
			if (passes[i].gfx_settings) {
				for (auto& shader : passes[i].gfx_settings->shaders) {
					if (!unique_shaders_set.insert({ &shader, &passes[i] }).second) {
						existing_shaders[&passes[i]].push_back(&shader);
					}
				}
			} else if (passes[i].rt_settings) {
				for (auto& shader : passes[i].rt_settings->shaders) {
					if (!unique_shaders_set.insert({ &shader, &passes[i] }).second) {
						existing_shaders[&passes[i]].push_back(&shader);
					}
				}
			} else {
				if (!unique_shaders_set.insert({ &passes[i].compute_settings->shader, &passes[i] }).second) {
					existing_shaders[&passes[i]].push_back(&passes[i].compute_settings->shader);
				}
			}
		}
		std::vector<std::future<void>> futures;
		for (auto& [shader, rp] : unique_shaders_set) {
			unique_shaders[rp].push_back(shader);
		}
		// Compile and process resources for unique shaders
		for (auto& [pass, shaders] : unique_shaders) {
			futures.push_back(ThreadPool::submit(std::bind(&build_shaders, pass, shaders)));
		}
		for (auto& future : futures) {
			future.wait();
		}
		futures.clear();
		// Process resources for duplicate shaders
		for (auto& [pass, shaders] : existing_shaders) {
			futures.push_back(ThreadPool::submit(std::bind(&build_shaders, pass, shaders)));
		}
		for (auto& future : futures) {
			future.wait();
		}
	}
	uint32_t i = beginning_pass_idx;
	uint32_t rem_passes = ending_pass_idx - beginning_pass_idx;
	while (rem_passes > 0) {
		if (passes[i].active) {
			passes[i].finalize();
			rem_passes--;
		}
		i++;
	}

	if (pipeline_tasks.size()) {
		std::vector<std::future<void>> futures;
		futures.reserve(pipeline_tasks.size());
		for (auto& [task, idx] : pipeline_tasks) {
			if (task) {
				futures.push_back(ThreadPool::submit(task, &passes[idx]));
			}
		}
		for (auto& future : futures) {
			future.wait();
		}
		for (auto& [_, idx] : pipeline_tasks) {
			passes[idx].transition_resources();
		}
		pipeline_tasks.clear();

	}
	i = beginning_pass_idx;
	rem_passes = ending_pass_idx - beginning_pass_idx;
	while (rem_passes > 0) {
		if (!passes[i].active) {
			i++;
			continue;
		}
		buffer_sync_resources[i].buffer_bariers.resize(passes[i].wait_signals_buffer.size());
		buffer_sync_resources[i].dependency_infos.resize(passes[i].wait_signals_buffer.size());
		img_sync_resources[i].img_barriers.resize(passes[i].wait_signals_img.size());
		img_sync_resources[i].dependency_infos.resize(passes[i].wait_signals_img.size());
		passes[i].run(cmd);
		rem_passes--;
		i++;
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
		passes[i].resource_zeros.clear();
		passes[i].resource_copies.clear();
		passes[i].buffer_barriers.clear();
		passes[i].disable_execution = false;
		passes[i].active = false;
		passes[i].next_binding_idx = 0;
		if (!settings.shader_inference) {
			passes[i].explicit_buffer_reads.clear();
			passes[i].explicit_buffer_writes.clear();
			passes[i].explicit_tex_reads.clear();
			passes[i].explicit_tex_writes.clear();
		}
		passes[i].submitted = false;
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
	beginning_pass_idx = ending_pass_idx = 0;
	reload_shaders = false;
}

void RenderGraph::submit(CommandBuffer& cmd) {
	cmd.submit();
	uint32_t i = beginning_pass_idx;
	uint32_t rem_passes = ending_pass_idx - beginning_pass_idx;
	while (rem_passes > 0) {
		if (passes[i].active) {
			passes[i].submitted = true;
			rem_passes--;
		}
		i++;
	}
	beginning_pass_idx = ending_pass_idx;
}

void RenderGraph::run_and_submit(CommandBuffer& cmd) {
	run(cmd.handle);
	submit(cmd);
}

void RenderGraph::destroy() {
	event_pool.cleanup(ctx->device);
	for (auto& pass : passes) {
		if (pass.push_constant_data) {
			free(pass.push_constant_data);
		}
	}
	for (const auto& [k, v] : pipeline_cache) {
		v.pipeline->cleanup();
	}
}
