#include "../LumenPCH.h"
#include "Framework/RenderGraph.h"
#include "RenderGraph.h"
#include "VkUtils.h"

namespace lumen {
#define DIRTY_CHECK(x) \
	if (!(x)) {        \
		return *this;  \
	}

void RenderPass::register_dependencies(Buffer& buffer, VkAccessFlags dst_access_flags) {
	const bool found = rg->buffer_resource_map.find(buffer.handle) != rg->buffer_resource_map.end();
	if (!found || (dst_access_flags == VK_ACCESS_SHADER_READ_BIT &&
				   (rg->buffer_resource_map[buffer.handle].second == dst_access_flags))) {
		return;
	}
	// Invariant : Pass with lower index should be the setter
	// Set current pass dependencies
	auto src_access_flags = rg->buffer_resource_map[buffer.handle].second;
	if (src_access_flags & VK_ACCESS_TRANSFER_WRITE_BIT) {
		dst_access_flags |= VK_ACCESS_TRANSFER_WRITE_BIT;
	}

	auto opposing_pass_idx = rg->buffer_resource_map[buffer.handle].first;
	if (opposing_pass_idx < rg->passes.size()) {
		RenderPass& opposing_pass = rg->passes[opposing_pass_idx];
		if (opposing_pass_idx < rg->passes.size() && opposing_pass.pass_idx < pass_idx) {
			if (wait_signals_buffer.find(buffer.handle) == wait_signals_buffer.end()) {
				wait_signals_buffer[buffer.handle] = BufferSyncDescriptor{
					.src_access_flags = src_access_flags,
					.dst_access_flags = dst_access_flags,
					.opposing_pass_idx = opposing_pass.pass_idx,
				};
			}
			// Set source pass dependencies (Signalling pass)
			if (opposing_pass.set_signals_buffer.find(buffer.handle) == opposing_pass.set_signals_buffer.end()) {
				opposing_pass.set_signals_buffer[buffer.handle] =
					BufferSyncDescriptor{.src_access_flags = src_access_flags,
										 .dst_access_flags = dst_access_flags,
										 .opposing_pass_idx = pass_idx};
			}
		} else if (src_access_flags != dst_access_flags) {
			buffer_barriers.push_back({buffer.handle, src_access_flags, dst_access_flags});
		}
	} else if (src_access_flags != dst_access_flags) {
		buffer_barriers.push_back({buffer.handle, src_access_flags, dst_access_flags});
	}
}

void RenderPass::register_dependencies(Texture2D& tex, VkImageLayout dst_layout) {
	const bool has_storage_bit = (tex.usage_flags & VK_IMAGE_USAGE_STORAGE_BIT) == VK_IMAGE_USAGE_STORAGE_BIT;
	const bool eq_layouts = tex.layout == dst_layout;
	// Note: Currently, the following optimization doesn't work for this:
	// if (eq_layouts && (!has_storage_bit || dst_layout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)) {
	// The reason is that when both src and dst layout are VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, it's possible that prior
	// to this pass, there was another pass that signalled a transition to READ_ONLY_OPTIMAL, and that pass might have a
	// different pipeline type. So, for example:
	// (compute - Image layout : general) -> (compute - Image layout: read_only (for compute only)) - (fragment - Image
	// layout: read only) In this case: The correct synchronization is needed to ensure a fragment shader read access.
	// So in principle it would be possible to feed that access flag in the 2nd stage,
	// but our architecture currently doesn't allow this as we don't explicitly store the pipeline stage for signals
	if (eq_layouts && !has_storage_bit) {
		return;
	}
	auto img_resource = rg->img_resource_map.find(tex.img);
	const bool resource_exists = img_resource != rg->img_resource_map.end();
	if (has_storage_bit && tex.layout == dst_layout && !resource_exists) {
		return;
	}
	if (tex.layout == VK_IMAGE_LAYOUT_UNDEFINED || !resource_exists) {
		layout_transitions.push_back({&tex, tex.layout, dst_layout});
		tex.layout = dst_layout;
	} else if (img_resource->second < rg->passes.size()) {
		RenderPass& opposing_pass = rg->passes[img_resource->second];
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
		} else if (tex.layout != dst_layout) {
			// Means the opposing pass has already executed
			layout_transitions.push_back({&tex, tex.layout, dst_layout});
			tex.layout = dst_layout;
		}
	} else if (tex.layout != dst_layout) {
		// Means the opposing pass has already executed
		layout_transitions.push_back({&tex, tex.layout, dst_layout});
		tex.layout = dst_layout;
	}
}

void RenderPass::transition_resources() {
	if (rg->settings.shader_inference) {
		for (auto i = 0; i < pipeline_storage->bound_resources.size(); i++) {
			auto& bound_resource = pipeline_storage->bound_resources[i];
			if (!bound_resource.active) {
				if (bound_resource.tex) {
					descriptor_infos[i] = bound_resource.tex->descriptor(
						vk::get_image_layout(pipeline_storage->pipeline->descriptor_types[i]));
				} else {
					descriptor_infos[i] = pipeline_storage->bound_resources[i].get_descriptor_info();
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
			descriptor_infos[i] = pipeline_storage->bound_resources[i].get_descriptor_info();
		}
		for (const auto& [buffer_str, status] : pipeline_storage->affected_buffer_pointers) {
			auto buffer = rg->registered_buffer_pointers[buffer_str];
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
		for (int i = 0; i < pipeline_storage->bound_resources.size(); i++) {
			descriptor_infos[i] = pipeline_storage->bound_resources[i].get_descriptor_info();
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
			} else {
				write_impl(*dst.tex, VK_ACCESS_TRANSFER_WRITE_BIT);
			}
		} else {  // buffer
			read_impl(*src.buf, VK_ACCESS_TRANSFER_READ_BIT);
			if (dst.buf) {
				write_impl(*dst.buf, VK_ACCESS_TRANSFER_WRITE_BIT);
			} else {
				write_impl(*dst.tex, VK_ACCESS_TRANSFER_WRITE_BIT);
			}
		}
	}
}

static void build_shaders(RenderPass* pass, const std::vector<Shader*>& active_shaders) {
	// todo: make resource processing in order
	auto process_bindless_resources = [pass](const Shader& shader) {
		if (!pass->rg->settings.shader_inference) {
			return;
		}
		for (auto& [k, v] : shader.buffer_status_map) {
			if (v.read) {
				pass->pipeline_storage->affected_buffer_pointers[k].read = v.read;
			}
			if (v.write) {
				pass->pipeline_storage->affected_buffer_pointers[k].write = v.write;
			}
		}
	};
	auto process_bindings = [pass](const Shader& shader) {
		for (auto& [k, v] : shader.resource_binding_map) {
			assert(k < pass->pipeline_storage->bound_resources.size());
			pass->pipeline_storage->bound_resources[k].active = v.active;
			pass->pipeline_storage->bound_resources[k].read = v.read;
			pass->pipeline_storage->bound_resources[k].write = v.write;
		}
	};
	switch (pass->type) {
		case PassType::Graphics: {
			std::vector<std::future<Shader*>> shader_tasks;
			shader_tasks.reserve(pass->gfx_settings->shaders.size());
			for (auto& shader : active_shaders) {
				pass->rg->shader_map_mutex.lock();
				auto shader_it = pass->rg->shader_cache.find(shader->name_with_macros);
				pass->rg->shader_map_mutex.unlock();
				if (shader_it != pass->rg->shader_cache.end()) {
					*shader = shader_it->second;
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
					pass->rg->shader_cache[shader->name_with_macros] = *shader;
				}
			}
			for (auto& shader : active_shaders) {
				process_bindless_resources(*shader);
				process_bindings(*shader);
			}
		} break;
		case PassType::RT: {
			std::vector<std::future<Shader*>> shader_tasks;
			shader_tasks.reserve(pass->rt_settings->shaders.size());
			for (auto& shader : active_shaders) {
				pass->rg->shader_map_mutex.lock();
				auto shader_it = pass->rg->shader_cache.find(shader->name_with_macros);
				pass->rg->shader_map_mutex.unlock();
				if (shader_it != pass->rg->shader_cache.end()) {
					*shader = shader_it->second;
				} else {
					shader_tasks.push_back(ThreadPool::submit(
						[pass](Shader* shader) {
							shader->compile(pass);
							return shader;
						},
						shader));
					// shader->compile(pass);
				}
			}
			for (auto& task : shader_tasks) {
				auto shader = task.get();
				{
					std::lock_guard<std::mutex> lock(pass->rg->shader_map_mutex);
					pass->rg->shader_cache[shader->name_with_macros] = *shader;
				}
			}
			for (auto& shader : active_shaders) {
				process_bindless_resources(*shader);
				process_bindings(*shader);
			}

		} break;
		case PassType::Compute: {
			for (auto& shader : active_shaders) {
				pass->rg->shader_map_mutex.lock();
				auto shader_it = pass->rg->shader_cache.find(shader->name_with_macros);
				pass->rg->shader_map_mutex.unlock();
				if (shader_it != pass->rg->shader_cache.end()) {
					*shader = shader_it->second;
				} else {
					shader->compile(pass);
					{
						std::lock_guard<std::mutex> lock(pass->rg->shader_map_mutex);
						pass->rg->shader_cache[shader->name_with_macros] = *shader;
					}
				}
				// shader->compile(pass);
				pass->pipeline_storage->affected_buffer_pointers = shader->buffer_status_map;
				process_bindings(*shader);
			}
		} break;
		default:
			break;
	}
}

RenderGraph::RenderGraph(VulkanContext* ctx) : ctx(ctx) { pipeline_tasks.reserve(32); }

RenderPass& RenderGraph::current_pass() { return passes[passes.size() - 1]; }

RenderPass& RenderGraph::add_rt(const std::string& name, const RTPassSettings& settings) {
	return add_pass_impl(name, settings);
}

RenderPass& RenderGraph::add_gfx(const std::string& name, const GraphicsPassSettings& settings) {
	return add_pass_impl(name, settings);
}

RenderPass& RenderGraph::add_compute(const std::string& name, const ComputePassSettings& settings) {
	return add_pass_impl(name, settings);
}

RenderPass& RenderPass::bind(const ResourceBinding& binding) {
	if (next_binding_idx >= pipeline_storage->bound_resources.size()) {
		pipeline_storage->bound_resources.push_back(binding);
		descriptor_counts.push_back(1);
	} else {
		pipeline_storage->bound_resources[next_binding_idx].replace(binding);
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

RenderPass& RenderPass::bind_texture_with_sampler(Texture2D& tex, VkSampler sampler) {
	if (next_binding_idx >= pipeline_storage->bound_resources.size()) {
		pipeline_storage->bound_resources.emplace_back(tex, sampler);
		descriptor_counts.push_back(1);
	} else {
		pipeline_storage->bound_resources[next_binding_idx].replace(tex, sampler);
	}
	next_binding_idx++;
	return *this;
}

RenderPass& RenderPass::bind_texture_array(std::vector<Texture2D>& textures, bool force_update) {
	if (next_binding_idx >= pipeline_storage->bound_resources.size()) {
		for (auto& texture : textures) {
			pipeline_storage->bound_resources.emplace_back(texture);
		}
		descriptor_counts.push_back((uint32_t)textures.size());
	} else {
		for (auto i = 0; i < textures.size(); i++) {
			pipeline_storage->bound_resources[next_binding_idx + i].replace(textures[i]);
		}
	}
	return *this;
}

RenderPass& RenderPass::bind_buffer_array(std::vector<Buffer>& buffers, bool force_update) {
	if (next_binding_idx >= pipeline_storage->bound_resources.size()) {
		for (auto& buffer : buffers) {
			pipeline_storage->bound_resources.emplace_back(buffer);
		}
		descriptor_counts.push_back((uint32_t)buffers.size());
	} else {
		for (auto i = 0; i < buffers.size(); i++) {
			pipeline_storage->bound_resources[next_binding_idx + i].replace(buffers[i]);
		}
	}
	return *this;
}

RenderPass& RenderPass::bind_tlas(const AccelKHR& tlas) {
	pipeline_storage->pipeline->tlas_info = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
	pipeline_storage->pipeline->tlas_info.accelerationStructureCount = 1;
	pipeline_storage->pipeline->tlas_info.pAccelerationStructures = &tlas.accel;
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

RenderPass& RenderPass::skip_execution(bool condition) {
	disable_execution = condition;
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
	resource_copies.push_back({src, dst});
	return *this;
}

void RenderPass::finalize() {
	// Create pipelines/push descriptor templates
	if (!is_pipeline_cached) {
		switch (type) {
			case PassType::Graphics: {
				auto func = [](RenderPass* pass) {
					pass->pipeline_storage->pipeline->create_gfx_pipeline(*pass->gfx_settings, pass->descriptor_counts,
														pass->gfx_settings->color_outputs,
														pass->gfx_settings->depth_output);
				};
				if (rg->multithreaded_pipeline_compilation) {
					rg->pipeline_tasks.push_back({func, pass_idx});
				} else {
					func(this);
				}
				break;
			}
			case PassType::RT: {
				auto func = [](RenderPass* pass) {
					pass->pipeline_storage->pipeline->create_rt_pipeline(*pass->rt_settings, pass->descriptor_counts);
					// Create descriptor pool and sets
					if (!pass->pipeline_storage->pipeline->tlas_descriptor_pool) {
						auto pool_size = vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1);
						auto descriptor_pool_ci = vk::descriptor_pool(1, &pool_size, 1);

						vk::check(vkCreateDescriptorPool(pass->rg->ctx->device, &descriptor_pool_ci, nullptr,
														 &pass->pipeline_storage->pipeline->tlas_descriptor_pool),
								  "Failed to create descriptor pool");
						VkDescriptorSetAllocateInfo set_allocate_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
						set_allocate_info.descriptorPool = pass->pipeline_storage->pipeline->tlas_descriptor_pool;
						set_allocate_info.descriptorSetCount = 1;
						set_allocate_info.pSetLayouts = &pass->pipeline_storage->pipeline->tlas_layout;
						vkAllocateDescriptorSets(pass->rg->ctx->device, &set_allocate_info,
												 &pass->pipeline_storage->pipeline->tlas_descriptor_set);
					}
					auto descriptor_write = vk::write_descriptor_set(
						pass->pipeline_storage->pipeline->tlas_descriptor_set,
						VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 0, &pass->pipeline_storage->pipeline->tlas_info);
					vkUpdateDescriptorSets(pass->rg->ctx->device, 1, &descriptor_write, 0, nullptr);
				};
				if (rg->multithreaded_pipeline_compilation) {
					rg->pipeline_tasks.push_back({func, pass_idx});
				} else {
					func(this);
				}
				break;
			}
			case PassType::Compute: {
				auto func = [](RenderPass* pass) {
					pass->pipeline_storage->pipeline->create_compute_pipeline(*pass->compute_settings,
																			  pass->descriptor_counts);
				};
				if (rg->multithreaded_pipeline_compilation) {
					rg->pipeline_tasks.push_back({func, pass_idx});
				} else {
					func(this);
				}
				break;
			}
			default:
				break;
		}
	}
}

void RenderPass::write_impl(Buffer& buffer, VkAccessFlags access_flags) {
	register_dependencies(buffer, access_flags);
	rg->buffer_resource_map[buffer.handle] = {pass_idx, access_flags};
}

void RenderPass::write_impl(Texture2D& tex, VkAccessFlags access_flags) {
	VkImageLayout target_layout = vk::get_target_img_layout(tex, access_flags);
	register_dependencies(tex, target_layout);
	rg->img_resource_map[tex.img] = pass_idx;
}

void RenderPass::read_impl(Buffer& buffer) { read_impl(buffer, VK_ACCESS_SHADER_READ_BIT); }

void RenderPass::read_impl(Buffer& buffer, VkAccessFlags access_flags) {
	register_dependencies(buffer, access_flags);
	rg->buffer_resource_map[buffer.handle] = {pass_idx, access_flags};
}

void RenderPass::read_impl(Texture2D& tex) {
	VkImageLayout target_layout = vk::get_target_img_layout(tex, VK_ACCESS_SHADER_READ_BIT);
	register_dependencies(tex, target_layout);
	rg->img_resource_map[tex.img] = pass_idx;
}

void RenderPass::post_execution_barrier(Buffer& buffer, VkAccessFlags access_flags) {
	auto src_access_flags = rg->buffer_resource_map[buffer.handle].second;
	post_execution_buffer_barriers.push_back({buffer.handle, src_access_flags, access_flags});
}

void RenderPass::run(VkCommandBuffer cmd) {
	std::vector<VkEvent> wait_events;
	const bool use_events = rg->settings.use_events;
	if (use_events) {
		wait_events.reserve(wait_signals_buffer.size());
	}
	vk::DebugMarker::begin_region(rg->ctx->device, cmd, name.c_str(), glm::vec4(1.0f, 0.78f, 0.05f, 1.0f));
	// Wait: Buffer
	auto& buffer_sync = rg->buffer_sync_resources[pass_idx];
	auto& img_sync = rg->img_sync_resources[pass_idx];
	int i = 0;
	for (const auto& [k, v] : wait_signals_buffer) {
		if (use_events) {
			LUMEN_ASSERT(rg->passes[v.opposing_pass_idx].set_signals_buffer[k].event, "Event can't be null");
		}
		buffer_sync.buffer_bariers[i] =
			vk::buffer_barrier2(k, v.src_access_flags, v.dst_access_flags,
								vk::get_pipeline_stage(rg->passes[v.opposing_pass_idx].type, v.src_access_flags),
								vk::get_pipeline_stage(type, v.dst_access_flags));
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
	{
		std::vector<VkBufferMemoryBarrier2> buffer_memory_barriers;
		buffer_memory_barriers.reserve(buffer_barriers.size());
		for (auto& barrier : buffer_barriers) {
			auto curr_stage = vk::get_pipeline_stage(type, barrier.src_access_flags);
			auto dst_stage = vk::get_pipeline_stage(type, barrier.dst_access_flags);
			buffer_memory_barriers.push_back(vk::buffer_barrier2(barrier.buffer, barrier.src_access_flags,
																 barrier.dst_access_flags, curr_stage, dst_stage));
		}
		auto dependency_info =
			vk::dependency_info((uint32_t)buffer_memory_barriers.size(), buffer_memory_barriers.data());
		vkCmdPipelineBarrier2(cmd, &dependency_info);
	}

	// Wait: Images
	wait_events.clear();
	i = 0;
	for (const auto& [k, v] : wait_signals_img) {
		if (use_events) {
			LUMEN_ASSERT(rg->passes[v.opposing_pass_idx].set_signals_img[k].event, "Event can't be null");
		}
		auto src_access_flags = vk::access_flags_for_img_layout(v.old_layout);
		auto dst_access_flags = vk::access_flags_for_img_layout(v.new_layout);
		auto src_stage = vk::get_pipeline_stage(rg->passes[v.opposing_pass_idx].type, src_access_flags);
		auto dst_stage = vk::get_pipeline_stage(type, dst_access_flags);
		img_sync.img_barriers[i] =
			vk::image_barrier2(k, src_access_flags, dst_access_flags, v.old_layout, v.new_layout, v.image_aspect,
							   src_stage, dst_stage, rg->ctx->indices.gfx_family.value());
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
	if (pipeline_storage->bound_resources.size()) {
		vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipeline_storage->pipeline->update_template,
											  pipeline_storage->pipeline->pipeline_layout, 0, descriptor_infos);
	}
	// Push constants
	if (pipeline_storage->pipeline->push_constant_size) {
		vkCmdPushConstants(cmd, pipeline_storage->pipeline->pipeline_layout, pipeline_storage->pipeline->pc_stages, 0,
						   pipeline_storage->pipeline->push_constant_size, push_constant_data);
	}
	// Run
	if (!disable_execution) {
		switch (type) {
			case PassType::RT: {
				LUMEN_ASSERT(pipeline_storage->pipeline->tlas_descriptor_set, "TLAS descriptor set cannot be NULL!");
				// This doesnt work because we can't push TLAS descriptor with
				// template...
				// vkCmdPushDescriptorSetWithTemplateKHR(cmd,
				// pipeline->rt_update_template, pipeline->pipeline_layout, 0,
				// &tlas_buffer.descriptor);
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
										pipeline_storage->pipeline->pipeline_layout, 1, 1,
										&pipeline_storage->pipeline->tlas_descriptor_set, 0, nullptr);

				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline_storage->pipeline->handle);

				if (rt_settings->pass_func) {
					rt_settings->pass_func(cmd, *this);
				} else {
					auto& regions = pipeline_storage->pipeline->get_rt_regions();
					auto& dims = rt_settings->dims;
					vkCmdTraceRaysKHR(cmd, &regions[0], &regions[1], &regions[2], &regions[3], dims.x, dims.y, dims.z);
				}
				break;
			}
			case PassType::Compute: {
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_storage->pipeline->handle);

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
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_storage->pipeline->handle);

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

	// Post execution buffer barriers
	{
		std::vector<VkBufferMemoryBarrier2> post_execution_buffer_memory_barriers;
		post_execution_buffer_memory_barriers.reserve(post_execution_buffer_barriers.size());
		for (auto& barrier : post_execution_buffer_barriers) {
			auto curr_stage = vk::get_pipeline_stage(type, barrier.src_access_flags);
			auto dst_stage = vk::get_pipeline_stage(type, barrier.dst_access_flags);
			post_execution_buffer_memory_barriers.push_back(vk::buffer_barrier2(
				barrier.buffer, barrier.src_access_flags, barrier.dst_access_flags, curr_stage, dst_stage));
		}
		auto dependency_info = vk::dependency_info((uint32_t)post_execution_buffer_memory_barriers.size(),
												   post_execution_buffer_memory_barriers.data());
		vkCmdPipelineBarrier2(cmd, &dependency_info);
	}

	for (const auto& [src, dst] : resource_copies) {
		if (src.tex) {
			// Assumption: The copy(...) is called in the pass after the src is produced
			if (dst.buf) {
				VkBufferImageCopy region = {};
				region.imageSubresource.aspectMask = src.tex->aspect_flags;
				region.imageSubresource.mipLevel = 0;
				region.imageSubresource.baseArrayLayer = 0;
				region.imageSubresource.layerCount = 1;
				region.imageExtent = src.tex->base_extent;
				VkImageLayout old_layout = src.tex->layout;
				src.tex->transition(cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
				vkCmdCopyImageToBuffer(cmd, src.tex->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.buf->handle, 1,
									   &region);
				src.tex->transition(cmd, old_layout);
			} else {
				LUMEN_ASSERT(src.tex->aspect_flags == dst.tex->aspect_flags, "Aspect flags mismatch");
				VkImageCopy region = {};
				VkImageLayout old_layout = src.tex->layout;
				src.tex->transition(cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
				dst.tex->transition(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
				region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.srcSubresource.layerCount = 1;
				region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.dstSubresource.layerCount = 1;
				region.extent = src.tex->base_extent;
				vkCmdCopyImage(cmd, src.tex->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.tex->img,
							   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
			}
		} else {  // buffer
			if (dst.buf) {
				VkBufferCopy copy_region = {.size = src.buf->size};
				vkCmdCopyBuffer(cmd, src.buf->handle, dst.buf->handle, 1, &copy_region);
			}
		}
	}

	// Set: Buffer
	for (const auto& [k, v] : set_signals_buffer) {
		LUMEN_ASSERT(v.event == nullptr, "VkEvent should be null in the setter");
		VkBufferMemoryBarrier2 mem_barrier = vk::buffer_barrier2(
			k, v.src_access_flags, v.dst_access_flags, vk::get_pipeline_stage(type, v.src_access_flags),
			vk::get_pipeline_stage(rg->passes[v.opposing_pass_idx].type, v.dst_access_flags));
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
		auto mem_barrier = vk::image_barrier2(
			k, vk::access_flags_for_img_layout(v.old_layout), vk::access_flags_for_img_layout(v.new_layout),
			v.old_layout, v.new_layout, v.image_aspect, vk::get_pipeline_stage(type, src_access_flags),
			vk::get_pipeline_stage(rg->passes[v.opposing_pass_idx].type, dst_access_flags),
			rg->ctx->indices.gfx_family.value());

		VkDependencyInfo dependency_info = vk::dependency_info(1, &mem_barrier);
		if (use_events) {
			set_signals_img[k].event = rg->event_pool.get_event(rg->ctx->device, cmd);
			vkCmdSetEvent2(cmd, set_signals_img[k].event, &dependency_info);
		}
	}
	vk::DebugMarker::end_region(rg->ctx->device, cmd);
}

void RenderGraph::run(VkCommandBuffer cmd) {
	buffer_sync_resources.resize(passes.size());
	img_sync_resources.resize(passes.size());

	// Compile shaders and process resources
	const bool recording_or_reload = dirty_pass_encountered || reload_shaders;
	if (recording_or_reload) {
		auto cmp = [](const std::pair<Shader*, RenderPass*>& a, const std::pair<Shader*, RenderPass*>& b) {
			return a.first->name_with_macros < b.first->name_with_macros;
		};
		std::set<std::pair<Shader*, RenderPass*>, decltype(cmp)> unique_shaders_set;
		std::unordered_map<RenderPass*, std::vector<Shader*>> unique_shaders;
		std::unordered_map<RenderPass*, std::vector<Shader*>> existing_shaders;

		if (recording_or_reload) {
			for (auto i = 0; i < passes.size(); i++) {
				if (passes[i].gfx_settings) {
					for (auto& shader : passes[i].gfx_settings->shaders) {
						if (!unique_shaders_set.insert({&shader, &passes[i]}).second) {
							existing_shaders[&passes[i]].push_back(&shader);
						}
					}
				} else if (passes[i].rt_settings) {
					for (auto& shader : passes[i].rt_settings->shaders) {
						if (!unique_shaders_set.insert({&shader, &passes[i]}).second) {
							existing_shaders[&passes[i]].push_back(&shader);
						}
					}
				} else {
					if (!unique_shaders_set.insert({&passes[i].compute_settings->shader, &passes[i]}).second) {
						existing_shaders[&passes[i]].push_back(&passes[i].compute_settings->shader);
					}
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

	for (auto i = 0; i < passes.size(); i++) {
		passes[i].finalize();
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
		// for (auto& [_, idx] : pipeline_tasks) {
		// 	passes[idx].transition_resources();
		// }
		pipeline_tasks.clear();
	}

	for (auto i = 0; i < passes.size(); i++) {
		passes[i].transition_resources();
	}

	for (auto i = 0; i < passes.size(); i++) {
		buffer_sync_resources[i].buffer_bariers.resize(passes[i].wait_signals_buffer.size());
		buffer_sync_resources[i].dependency_infos.resize(passes[i].wait_signals_buffer.size());
		img_sync_resources[i].img_barriers.resize(passes[i].wait_signals_img.size());
		img_sync_resources[i].dependency_infos.resize(passes[i].wait_signals_img.size());
		passes[i].run(cmd);
	}
}

void RenderGraph::reset() {
	event_pool.reset_events(ctx->device);

	passes.clear();
	if (pipeline_tasks.size()) {
		pipeline_tasks.clear();
	}
	buffer_sync_resources.clear();
	img_sync_resources.clear();
	reload_shaders = false;
}

void RenderGraph::submit(CommandBuffer& cmd) {
	cmd.submit();
	passes.clear();
}

void RenderGraph::run_and_submit(CommandBuffer& cmd) {
	run(cmd.handle);
	submit(cmd);
	dirty_pass_encountered = false;
}

void RenderGraph::destroy() {
	passes.clear();
	event_pool.cleanup(ctx->device);
	for (auto& pass : passes) {
		if (pass.push_constant_data) {
			free(pass.push_constant_data);
		}
	}
	for (const auto& [k, v] : pipeline_cache) {
		v.pipeline->cleanup();
	}
	buffer_resource_map.clear();
	img_resource_map.clear();
	registered_buffer_pointers.clear();
	shader_cache.clear();
	pipeline_cache.clear();
}

void RenderGraph::set_pipelines_dirty() {
	for (auto& [k, v] : pipeline_cache) {
		v.dirty = true;
	}
}

}  // namespace lumen
