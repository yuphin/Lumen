#include "Framework/RenderGraph.h"
#include "RenderGraph.h"
#include "VkUtils.h"
#include "GPUQueryManager.h"
#include "PersistentResourceManager.h"
#include "DynamicResourceManager.h"
#include "Framework/ThreadPool.h"

////////////////////////////
// --- Limits for fixed size arrays inside Render Graph ---
static constexpr u64 MAX_PASSES_PER_FRAME = 1024;
////////////////////////////
// --- Limits for shader and pipeline compilation inside render graph ---
static constexpr u64 MAX_PIPELINE_TASKS = 128;
static constexpr u64 MAX_SHADER_COMPILATIONS_PER_FRAME = 1024;

namespace lm {

static lm::Arena* _arena_rendergraph = nullptr;
static lm::Arena* _arena_per_frame = nullptr;
// TODO: Investigate get_or_create behavior

static VkPipelineStageFlags pipeline_stage_from_pass_type(vk::PassType pass_type, VkAccessFlags access_flags) {
	VkPipelineStageFlags res = 0;
	switch (pass_type) {
		case vk::PassType::Compute:
			res = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			break;
		case vk::PassType::Graphics:
			res = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
			break;
		case vk::PassType::RT:
			if (access_flags &
				(VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR)) {
				res = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
			} else {
				res = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
			}
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

static bool is_read_flag(VkAccessFlags flags) {
	return flags &
		   (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);
}

void RenderPass::register_dependencies(const vk::Buffer* buffer, VkAccessFlags dst_access_flags,
									   BufferSyncFlags flags) {
	// Invariant : Pass with lower index should be the setter before the cmd buffer submission
	auto* entry = rg->buffer_resource_map.find(buffer->handle);

	if (!entry || (is_read_flag(dst_access_flags) && is_read_flag(entry->value.second))) {
		return;
	}
	u32 opposing_pass_idx = entry->value.first;
	VkAccessFlags src_access_flags = entry->value.second;
	// If this condition fails, that means the cmd buffer is already submitted last frame
	// For single cmd buffer setup, ignoring the else condition would be fine
	// However when multiple cmd buffers are in flight, we must still ensure the synchronization
	if (opposing_pass_idx < rg->passes.size && opposing_pass_idx < pass_idx) {
		RenderPass& opposing_pass = rg->passes[opposing_pass_idx];
		wait_signals_buffer.insert(buffer->handle, BufferSyncDescriptor{
													   .src_access_flags = src_access_flags,
													   .dst_access_flags = dst_access_flags,
													   .opposing_pass_idx = opposing_pass.pass_idx,
												   });
		opposing_pass.set_signals_buffer.insert(buffer->handle, BufferSyncDescriptor{
																	.src_access_flags = src_access_flags,
																	.dst_access_flags = dst_access_flags,
																	.opposing_pass_idx = pass_idx,
																});
	} else {
		if (flags == BufferSyncFlags::BUFFER_COPY || flags == BufferSyncFlags::BUFFER_AS_BUILD) {
			// Resource copies happens after the pass execution
			post_execution_buffer_barriers.push_back({buffer->handle, src_access_flags, dst_access_flags});
		} else {
			if (flags == BufferSyncFlags::BUFFER_ZERO) {
				// This case happens when there are no dependencies to the buffer being cleared inside the render
				// graph in a frame Yet we have to ensure syncronization because there are multiple command buffers
				// in flight
				LUMEN_ASSERT(dst_access_flags == VK_ACCESS_TRANSFER_WRITE_BIT, "Invalid buffer zero flags");
				prefill_buffer_barriers.push_back({buffer->handle, src_access_flags, dst_access_flags});
			} else {
				// TODO: Check
				// LUMEN_ASSERT(false, "Unreachable?");
				carryover_buffer_barriers.push_back({buffer->handle, src_access_flags, dst_access_flags});
			}
		}
	}
}

void RenderPass::register_dependencies(vk::Texture* tex, VkImageLayout dst_layout) {
	const bool has_storage_bit = (tex->usage_flags & VK_IMAGE_USAGE_STORAGE_BIT) == VK_IMAGE_USAGE_STORAGE_BIT;
	const bool eq_layouts = tex->layout == dst_layout;
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
	auto img_resource_entry = rg->img_resource_map.find(tex->handle);
	if (has_storage_bit && tex->layout == dst_layout && !img_resource_entry) {
		return;
	}
	if (tex->layout == VK_IMAGE_LAYOUT_UNDEFINED || !img_resource_entry) {
		layout_transitions.push_back({tex, tex->layout, dst_layout});
		tex->layout = dst_layout;
	} else if (img_resource_entry->value < rg->passes.size) {
		RenderPass& opposing_pass = rg->passes[img_resource_entry->value];
		if (opposing_pass.pass_idx < pass_idx) {
			wait_signals_img.insert(tex->handle, ImageSyncDescriptor{
													 .old_layout = tex->layout,
													 .new_layout = dst_layout,
													 .opposing_pass_idx = opposing_pass.pass_idx,
													 .image_aspect = tex->aspect_flags,
												 });

			opposing_pass.set_signals_img.insert(tex->handle, ImageSyncDescriptor{
																  .old_layout = tex->layout,
																  .new_layout = dst_layout,
																  .opposing_pass_idx = pass_idx,
																  .image_aspect = tex->aspect_flags,
															  });
			tex->layout = dst_layout;
		} else if (tex->layout != dst_layout) {
			// Means the opposing pass has already executed
			layout_transitions.push_back({tex, tex->layout, dst_layout});
			tex->layout = dst_layout;
		}
	} else if (tex->layout != dst_layout) {
		// Means the opposing pass has already executed
		layout_transitions.push_back({tex, tex->layout, dst_layout});
		tex->layout = dst_layout;
	}
}

void RenderPass::transition_resources() {
	for (const Resource& resource : resource_zeros) {
		if (resource.buf) {
			write_impl(resource.buf, VK_ACCESS_TRANSFER_WRITE_BIT, BufferSyncFlags::BUFFER_ZERO);
		} else {
			write_impl(resource.tex);
		}
	}

	if (rg->settings.shader_inference) {
		for (u64 i = 0; i < pipeline_storage->bound_resources.size; i++) {
			ResourceBinding& bound_resource = pipeline_storage->bound_resources[i];
			if (!bound_resource.active) {
				if (bound_resource.tex) {
					descriptor_infos[i] = vk::texture_descriptor(
						bound_resource.tex,
						vk::image_layout_from_descriptor_type(pipeline_storage->pipeline.descriptor_types[i]));
				} else {
					descriptor_infos[i] = pipeline_storage->bound_resources[i].get_descriptor_info();
				}
				continue;
			}
			if (bound_resource.write) {
				if (bound_resource.buf) {
					write_impl(bound_resource.buf, VK_ACCESS_SHADER_WRITE_BIT);
				} else {
					write_impl(bound_resource.tex);
				}
			} else if (bound_resource.read) {
				if (bound_resource.buf) {
					read_impl(bound_resource.buf);
				} else {
					read_impl(bound_resource.tex);
				}
			}
			descriptor_infos[i] = pipeline_storage->bound_resources[i].get_descriptor_info();
		}
		for (const auto& entry : pipeline_storage->affected_buffer_pointers) {
			String buffer_str = entry.key;
			vk::BufferStatus status = entry.value;
			vk::Buffer* buffer = rg->registered_buffer_pointers.find(buffer_str)->value;
			if (status.write) {
				write_impl(buffer, VK_ACCESS_SHADER_WRITE_BIT);
			} else if (status.read) {
				read_impl(buffer);
			}
		}
	} else {
		for (vk::Buffer* buf : explicit_buffer_reads) {
			read_impl(buf);
		}
		for (vk::Buffer* buf : explicit_buffer_writes) {
			write_impl(buf, VK_ACCESS_SHADER_WRITE_BIT);
		}
		for (vk::Texture* tex : explicit_tex_reads) {
			read_impl(tex);
		}
		for (vk::Texture* tex : explicit_tex_writes) {
			write_impl(tex);
		}
		for (i32 i = 0; i < pipeline_storage->bound_resources.size; i++) {
			descriptor_infos[i] = pipeline_storage->bound_resources[i].get_descriptor_info();
		}
	}

	for (const vk::BVH& as : pipeline_storage->as_bindings) {
		if (as.buffer) {
			read_impl(as.buffer, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR, BufferSyncFlags::BUFFER_AS_BUILD);
		}
	}

	for (const auto& [src, dst] : resource_copies) {
		if (src.tex) {
			// TODO: Add source texture dependencies
			if (dst.buf) {
				write_impl(dst.buf, VK_ACCESS_TRANSFER_WRITE_BIT, BufferSyncFlags::BUFFER_COPY);
			} else {
				write_impl(dst.tex, VK_ACCESS_TRANSFER_WRITE_BIT);
			}
		} else {  // buffer
			read_impl(src.buf, VK_ACCESS_TRANSFER_READ_BIT, BufferSyncFlags::BUFFER_COPY);
			if (dst.buf) {
				write_impl(dst.buf, VK_ACCESS_TRANSFER_WRITE_BIT, BufferSyncFlags::BUFFER_COPY);
			} else {
				write_impl(dst.tex, VK_ACCESS_TRANSFER_WRITE_BIT);
			}
		}
	}

	if (blas_build_data.is_valid()) {
		for (vk::Buffer* buf : blas_build_data.source_buffers) {
			read_impl(buf, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_SHADER_READ_BIT,
					  BufferSyncFlags::BUFFER_AS_BUILD);
		}
	}
}

static void process_bindless_resources(RenderPass* pass, vk::Shader& shader) {
	if (!pass->rg->settings.shader_inference) {
		return;
	}
	for (const auto& entry : shader.buffer_status_map) {
		pass->pipeline_storage->affected_buffer_pointers.insert(entry.key, entry.value);
	}
}

static void process_bindings(RenderPass* pass, vk::Shader& shader) {
	for (const auto& entry : shader.resource_binding_map) {
		assert(entry.key < pass->pipeline_storage->bound_resources.size);
		pass->pipeline_storage->bound_resources[entry.key].active = entry.value.active;
		pass->pipeline_storage->bound_resources[entry.key].read = entry.value.read;
		pass->pipeline_storage->bound_resources[entry.key].write = entry.value.write;
	}
}

static void build_shaders(RenderPass* pass, const lm::FixedArray<vk::Shader*>& active_shaders) {
	// TODO: make resource processing in order
	switch (pass->type) {
		case vk::PassType::RT:
		case vk::PassType::Graphics: {
			lm::SmallArray<std::future<vk::Shader*>, vk::MAX_SHADERS_PER_PASS> shader_tasks;
			for (auto& shader : active_shaders) {
				pass->rg->shader_map_mutex.lock();
				auto shader_entry = pass->rg->shader_cache.find(shader->name_with_macros);
				pass->rg->shader_map_mutex.unlock();
				if (shader_entry) {
					*shader = shader_entry->value;
				} else {
					shader_tasks.push_back_move(ThreadPool::submit(
						[pass](vk::Shader* shader) {
							shader->compile(pass);
							return shader;
						},
						shader));
				}
				// shader->compile(pass);
			}
			for (auto& task : shader_tasks) {
				auto shader = task.get();
				{
					std::lock_guard<std::mutex> lock(pass->rg->shader_map_mutex);
					pass->rg->shader_cache.insert(shader->name_with_macros, *shader);
				}
			}
			for (auto& shader : active_shaders) {
				process_bindless_resources(pass, *shader);
				process_bindings(pass, *shader);
			}
		} break;
		case vk::PassType::Compute: {
			for (auto& shader : active_shaders) {
				pass->rg->shader_map_mutex.lock();
				auto shader_entry = pass->rg->shader_cache.find(shader->name_with_macros);
				pass->rg->shader_map_mutex.unlock();
				if (shader_entry) {
					*shader = shader_entry->value;
				} else {
					shader->compile(pass);
					{
						std::lock_guard<std::mutex> lock(pass->rg->shader_map_mutex);
						pass->rg->shader_cache.insert(shader->name_with_macros, *shader);
					}
				}
				// shader->compile(pass);
				pass->pipeline_storage->affected_buffer_pointers = shader->buffer_status_map;
				process_bindings(pass, *shader);
			}
		} break;
		default:
			break;
	}
}

RenderGraph::RenderGraph() {}

RenderPass& RenderGraph::current_pass() { return passes[passes.size - 1]; }

RenderPass& RenderGraph::add_rt(const lm::String& name, const vk::RTPassSettings& settings) {
	bool cached = false;
	lm::String name_with_macros;
	lm::String macro_string;
	PipelineStorage* pipeline_storage = add_pass_impl_common(name, settings.macros, settings.specialization_data,
															 cached, name_with_macros, macro_string);
	vk::PassType type = vk::PassType::RT;
	RenderPass& pass = passes.push();
	render_pass_init_rt(pass, type, name_with_macros, this, (u32)passes.size - 1, settings, macro_string,
						pipeline_storage, cached);
	return pass;
}

RenderPass& RenderGraph::add_gfx(const lm::String& name, const vk::GraphicsPassSettings& settings) {
	bool cached = false;
	lm::String name_with_macros;
	lm::String macro_string;
	PipelineStorage* pipeline_storage = add_pass_impl_common(name, settings.macros, settings.specialization_data,
															 cached, name_with_macros, macro_string);
	vk::PassType type = vk::PassType::Graphics;
	RenderPass& pass = passes.push();
	render_pass_init_gfx(pass, type, name_with_macros, this, (u32)passes.size - 1, settings, macro_string,
						 pipeline_storage, cached);
	return pass;
}

RenderPass& RenderGraph::add_compute(const lm::String& name, const vk::ComputePassSettings& settings) {
	bool cached = false;
	lm::String name_with_macros;
	lm::String macro_string;
	PipelineStorage* pipeline_storage = add_pass_impl_common(name, settings.macros, settings.specialization_data,
															 cached, name_with_macros, macro_string);

	vk::PassType type = vk::PassType::Compute;
	RenderPass& pass = passes.push();
	render_pass_init_compute(pass, type, name_with_macros, this, (u32)passes.size - 1, settings, macro_string,
							 pipeline_storage, cached);
	return pass;
}

void RenderPass::init() {
	if (resources_initialized) {
		return;
	}
	set_signals_buffer = lm::hash_map_create<VkBuffer, BufferSyncDescriptor>(_arena_rendergraph, MAX_BUFFER_BARRIERS);
	wait_signals_buffer = lm::hash_map_create<VkBuffer, BufferSyncDescriptor>(_arena_rendergraph, MAX_BUFFER_BARRIERS);
	set_signals_img = lm::hash_map_create<VkImage, ImageSyncDescriptor>(_arena_rendergraph, MAX_IMG_BARRIERS);
	wait_signals_img = lm::hash_map_create<VkImage, ImageSyncDescriptor>(_arena_rendergraph, MAX_IMG_BARRIERS);
	resources_initialized = true;
}

RenderPass& RenderPass::bind(const ResourceBinding& binding) {
	if (next_binding_idx >= pipeline_storage->bound_resources.size) {
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
RenderPass& RenderPass::bind_texture_with_sampler(vk::Texture* tex, VkSampler sampler) {
	if (next_binding_idx >= pipeline_storage->bound_resources.size) {
		pipeline_storage->bound_resources.emplace_back(tex, sampler);
		descriptor_counts.push_back(1);
	} else {
		pipeline_storage->bound_resources[next_binding_idx].replace(tex, sampler);
	}
	next_binding_idx++;
	return *this;
}

RenderPass& RenderPass::bind_texture_array(lm::FixedArray<vk::Texture*> textures, bool force_update) {
	if (next_binding_idx >= pipeline_storage->bound_resources.size) {
		for (auto& texture : textures) {
			pipeline_storage->bound_resources.emplace_back(texture);
		}
		descriptor_counts.push_back((u32)textures.size);
	} else {
		for (auto i = 0; i < textures.size; i++) {
			pipeline_storage->bound_resources[next_binding_idx + i].replace(textures[i]);
		}
	}
	return *this;
}

RenderPass& RenderPass::bind_buffer_array(std::span<vk::Buffer*> buffers, bool force_update) {
	if (next_binding_idx >= pipeline_storage->bound_resources.size) {
		for (auto& buffer : buffers) {
			pipeline_storage->bound_resources.emplace_back(buffer);
		}
		descriptor_counts.push_back((u32)buffers.size());
	} else {
		for (auto i = 0; i < buffers.size(); i++) {
			pipeline_storage->bound_resources[next_binding_idx + i].replace(buffers[i]);
		}
	}
	return *this;
}

RenderPass& RenderPass::bind_tlas(const vk::BVH& tlas) {
	LUMEN_ASSERT(type == vk::PassType::RT, "TLAS can only be bound to RT pipelines");
	LUMEN_ASSERT(pipeline_storage->as_bindings.size <= vk::MAX_AS_BINDING_COUNT &&
					 next_as_binding_idx < vk::MAX_AS_BINDING_COUNT,
				 "Only two TLAS bindings are supported for now");
	if (next_as_binding_idx >= pipeline_storage->as_bindings.size) {
		pipeline_storage->as_bindings.push_back(tlas);
	} else {
		if (pipeline_storage->as_bindings[next_as_binding_idx].accel != tlas.accel) {
			pipeline_storage->update_as_descriptor = true;
		}
		pipeline_storage->as_bindings[next_as_binding_idx] = tlas;
	}
	next_as_binding_idx++;
	return *this;
}

RenderPass& RenderPass::read(vk::Buffer* buffer) {
	explicit_buffer_reads.push_back(buffer);
	return *this;
}

RenderPass& RenderPass::read(vk::Texture* tex) {
	explicit_tex_reads.push_back(tex);
	return *this;
}

RenderPass& RenderPass::read(std::initializer_list<vk::Texture*> texes) {
	for (vk::Texture* tex : texes) {
		read(tex);
	}
	return *this;
}

RenderPass& RenderPass::read(std::initializer_list<vk::Buffer*> buffers) {
	for (vk::Buffer* buff : buffers) {
		read(buff);
	}
	return *this;
}

RenderPass& RenderPass::read(ResourceBinding& resource) {
	if (resource.tex) {
		read(resource.tex);
	} else {
		read(resource.buf);
	}
	return *this;
}

RenderPass& RenderPass::write(vk::Buffer* buffer) {
	explicit_buffer_writes.push_back(buffer);
	return *this;
}

RenderPass& RenderPass::write(vk::Texture* tex) {
	explicit_tex_writes.push_back(tex);
	return *this;
}

RenderPass& RenderPass::write(std::initializer_list<vk::Buffer*> buffers) {
	for (vk::Buffer* buf : buffers) {
		write(buf);
	}
	return *this;
}

RenderPass& RenderPass::write(std::initializer_list<vk::Texture*> texes) {
	for (vk::Texture* tex : texes) {
		write(tex);
	}
	return *this;
}

RenderPass& RenderPass::write(ResourceBinding& resource) {
	if (resource.tex) {
		write(resource.tex);
	} else {
		write(resource.buf);
	}
	return *this;
}

RenderPass& RenderPass::skip_execution(bool condition) {
	disable_execution = condition;
	return *this;
}

RenderPass& RenderPass::push_constants(void* data, u64 size, u64 alignment) {
	push_constant_data = _arena_per_frame->allocate(size, alignment, nullptr, /*zero_initialize=*/false);
	memcpy(push_constant_data, data, size);
	return *this;
}

RenderPass& RenderPass::zero(const Resource& resource) {
	if (resource.tex) {
		LUMEN_ERROR("Unimplemented: Image zeroing");
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

RenderPass& RenderPass::zero(std::initializer_list<vk::Buffer*> buffers) {
	for (vk::Buffer* buf : buffers) {
		zero(buf);
	}
	return *this;
}

RenderPass& RenderPass::zero(std::initializer_list<vk::Texture*> textures) {
	for (vk::Texture* tex : textures) {
		zero(tex);
	}
	return *this;
}

RenderPass& RenderPass::copy(const Resource& src, const Resource& dst) {
	resource_copies.push_back({src, dst});
	return *this;
}

RenderPass& RenderPass::blas_build(util::Slice<vk::BVH> blases, util::Slice<vk::BlasInput> blas_inputs,
								   VkBuildAccelerationStructureFlagsKHR flags, util::Slice<vk::Buffer*> source_buffers,
								   vk::Buffer** scratch_buffer_ref) {
	LUMEN_ASSERT(!blas_build_data.is_valid(), "Only one BLAS build per pass is supported");
	LUMEN_ASSERT(blases.size == blas_inputs.size, "BLASes and inputs must have the same size");
	// TODO: Need to assert that scratch_buffer_ref == nullptr in certain cases
	blas_build_data.blases = blases;
	blas_build_data.blas_inputs = blas_inputs;
	blas_build_data.flags = flags;
	blas_build_data.source_buffers = source_buffers;
	blas_build_data.scratch_buffer_ref = scratch_buffer_ref;
	return *this;
}

RenderPass& RenderPass::tlas_build(vk::BVH& tlas, vk::Buffer* instances_buf, u32 instance_count,
								   VkBuildAccelerationStructureFlagsKHR flags, vk::Buffer** scratch_buffer_ref,
								   bool build_tlas_after_blas, bool update_tlas) {
	LUMEN_ASSERT(!tlas_build_data.is_valid(), "Only one TLAS build per pass is supported");

	tlas_build_data.tlas = &tlas;
	tlas_build_data.instance_count = instance_count;
	tlas_build_data.flags = flags;
	tlas_build_data.instances_buf = instances_buf;
	tlas_build_data.scratch_buffer_ref = scratch_buffer_ref;
	tlas_build_data.build_tlas_after_blas = build_tlas_after_blas;
	tlas_build_data.update_tlas = update_tlas;
	return *this;
}

void RenderPass::finalize() {
	// Create pipelines/push descriptor templates

	auto update_rt_descriptors = [this]() {
		VkAccelerationStructureKHR accels[vk::MAX_AS_BINDING_COUNT];
		u32 num_accels = 0;
		for (u32 i = 0; i < pipeline_storage->as_bindings.size; i++) {
			if (!pipeline_storage->as_bindings[i].accel) {
				LUMEN_INFO("Using null TLAS inside %s", name.data);
			}
			accels[i] = pipeline_storage->as_bindings[i].accel;
			++num_accels;
		}
		pipeline_storage->pipeline.tlas_info = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
		pipeline_storage->pipeline.tlas_info.accelerationStructureCount = num_accels;
		pipeline_storage->pipeline.tlas_info.pAccelerationStructures = accels;
		auto descriptor_write = vk::write_descriptor_set(pipeline_storage->pipeline.tlas_descriptor_set,
														 VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 0,
														 &pipeline_storage->pipeline.tlas_info, num_accels);
		vkUpdateDescriptorSets(vk::context().device, 1, &descriptor_write, 0, nullptr);
		pipeline_storage->update_as_descriptor = false;
	};
	bool rebuild_tlas_descriptors = is_pipeline_cached && pipeline_storage->update_as_descriptor;
	if (!is_pipeline_cached) {
		switch (type) {
			case vk::PassType::Graphics: {
				auto func = [](RenderPass* pass) {
					pass->pipeline_storage->pipeline.create_gfx_pipeline(
						pass->settings, pass->descriptor_counts.to_slice());
				};
				if (rg->multithreaded_pipeline_compilation) {
					rg->pipeline_tasks.push_back({func, pass_idx});
				} else {
					func(this);
				}
				break;
			}
			case vk::PassType::RT: {
				auto func = [update_rt_descriptors](RenderPass* pass) {
					pass->pipeline_storage->pipeline.create_rt_pipeline(pass->settings,
																		pass->descriptor_counts.to_slice(),
																		u32(pass->pipeline_storage->as_bindings.size));
					update_rt_descriptors();
				};
				if (rg->multithreaded_pipeline_compilation) {
					rg->pipeline_tasks.push_back({func, pass_idx});
				} else {
					func(this);
				}
				break;
			}
			case vk::PassType::Compute: {
				auto func = [](RenderPass* pass) {
					pass->pipeline_storage->pipeline.create_compute_pipeline(pass->settings,
																			 pass->descriptor_counts.to_slice());
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
	} else if (rebuild_tlas_descriptors) {
		update_rt_descriptors();
	}
}

void RenderPass::write_impl(const vk::Buffer* buffer, VkAccessFlags access_flags, BufferSyncFlags flags) {
	register_dependencies(buffer, access_flags, flags);
	rg->buffer_resource_map.insert(buffer->handle, {pass_idx, access_flags});
}

void RenderPass::write_impl(vk::Texture* tex, VkAccessFlags access_flags) {
	VkImageLayout target_layout = vk::texture_to_image_layout(tex, access_flags);
	register_dependencies(tex, target_layout);
	rg->img_resource_map.insert(tex->handle, pass_idx);
}

void RenderPass::read_impl(const vk::Buffer* buffer, VkAccessFlags access_flags, BufferSyncFlags flags) {
	register_dependencies(buffer, access_flags, flags);
	rg->buffer_resource_map.insert(buffer->handle, {pass_idx, access_flags});
}

void RenderPass::read_impl(vk::Texture* tex) {
	VkImageLayout target_layout = vk::texture_to_image_layout(tex, VK_ACCESS_SHADER_READ_BIT);
	register_dependencies(tex, target_layout);
	rg->img_resource_map.insert(tex->handle, pass_idx);
}

void RenderPass::post_execution_barrier(vk::Buffer* buffer, VkAccessFlags access_flags) {
	auto src_access_flags = rg->buffer_resource_map.find(buffer->handle)->value.second;
	post_execution_buffer_barriers.push_back({buffer->handle, src_access_flags, access_flags});
}

void RenderPass::run(VkCommandBuffer cmd) {
	lm::SmallArray<VkEvent, MAX_BUFFER_BARRIERS + MAX_IMG_BARRIERS> wait_events;
	const bool use_events = rg->settings.use_events;
	vk::begin_region(vk::context().device, cmd, name.data, glm::vec4(1.0f, 0.78f, 0.05f, 1.0f));
	GPUQueryManager::begin(cmd, name.data);

	// Wait: Buffer
	auto& buffer_sync = buffer_sync_resources;
	auto& img_sync = image_sync_resources;
	for (const auto& entry : wait_signals_buffer) {
		VkBuffer buffer = entry.key;
		const BufferSyncDescriptor& v = entry.value;
		VkBufferMemoryBarrier2& buffer_barrier = buffer_sync.buffer_barriers.push();
		buffer_barrier =
			vk::buffer_barrier2(entry.key, v.src_access_flags, v.dst_access_flags,
								pipeline_stage_from_pass_type(rg->passes[v.opposing_pass_idx].type, v.src_access_flags),
								pipeline_stage_from_pass_type(type, v.dst_access_flags));
		buffer_sync.dependency_infos.push_back(vk::dependency_info(1, &buffer_barrier));
		if (use_events) {
			VkEvent event = rg->passes[v.opposing_pass_idx].set_signals_buffer.find(buffer)->value.event;
			LUMEN_ASSERT(event, "Event can't be null");
			wait_events.push_back(event);
		}
	}
	if (wait_events.size) {
		vkCmdWaitEvents2(cmd, (u32)wait_events.size, wait_events.data, buffer_sync.dependency_infos.data);
		for (i32 i = 0; i < wait_events.size; i++) {
			vkCmdResetEvent2(cmd, wait_events[i], buffer_sync.buffer_barriers[i].dstStageMask);
		}
	} else if (!use_events) {
		for (const auto& info : buffer_sync.dependency_infos) {
			vkCmdPipelineBarrier2(cmd, &info);
		}
	}

	// TODO: Combine barriers and signals

	// Prefill buffer barriers
	{
		lm::SmallArray<VkBufferMemoryBarrier2, MAX_RESOURCES_ZEROS> buffer_memory_barriers;
		if (!prefill_buffer_barriers.empty()) {
			for (auto& barrier : prefill_buffer_barriers) {
				auto curr_stage = pipeline_stage_from_pass_type(type, barrier.src_access_flags);
				auto dst_stage = pipeline_stage_from_pass_type(type, barrier.dst_access_flags);
				buffer_memory_barriers.push_back(vk::buffer_barrier2(barrier.buffer, barrier.src_access_flags,
																	 barrier.dst_access_flags, curr_stage, dst_stage));
			}
			auto dependency_info = vk::dependency_info((u32)buffer_memory_barriers.size, buffer_memory_barriers.data);
			vkCmdPipelineBarrier2(cmd, &dependency_info);
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
		lm::SmallArray<VkBufferMemoryBarrier2, MAX_RESOURCES_ZEROS> buffer_memory_barriers;
		if (!carryover_buffer_barriers.empty()) {
			for (auto& barrier : carryover_buffer_barriers) {
				auto curr_stage = pipeline_stage_from_pass_type(type, barrier.src_access_flags);
				auto dst_stage = pipeline_stage_from_pass_type(type, barrier.dst_access_flags);
				buffer_memory_barriers.push_back(vk::buffer_barrier2(barrier.buffer, barrier.src_access_flags,
																	 barrier.dst_access_flags, curr_stage, dst_stage));
			}
			auto dependency_info = vk::dependency_info((u32)buffer_memory_barriers.size, buffer_memory_barriers.data);
			vkCmdPipelineBarrier2(cmd, &dependency_info);
		}
	}

	// Wait: Images
	wait_events.clear();
	for (const auto& entry : wait_signals_img) {
		VkImage image = entry.key;
		const ImageSyncDescriptor& v = entry.value;
		if (use_events) {
			LUMEN_ASSERT(rg->passes[v.opposing_pass_idx].set_signals_img.find(image)->value.event,
						 "Event can't be null");
		}
		auto src_access_flags = vk::access_flags_for_img_layout(v.old_layout);
		auto dst_access_flags = vk::access_flags_for_img_layout(v.new_layout);
		auto src_stage = pipeline_stage_from_pass_type(rg->passes[v.opposing_pass_idx].type, src_access_flags);
		auto dst_stage = pipeline_stage_from_pass_type(type, dst_access_flags);
		VkImageMemoryBarrier2& img_barrier = img_sync.img_barriers.emplace_back(
			vk::image_barrier2(entry.key, src_access_flags, dst_access_flags, v.old_layout, v.new_layout,
							   v.image_aspect, src_stage, dst_stage, vk::context().queue_indices.gfx_family.value()));
		img_sync.dependency_infos.emplace_back(vk::dependency_info(1, &img_barrier));
		if (use_events) {
			wait_events.push_back(rg->passes[v.opposing_pass_idx].set_signals_img.find(image)->value.event);
		}
	}

	if (wait_events.size) {
		vkCmdWaitEvents2(cmd, (u32)wait_events.size, wait_events.data, img_sync.dependency_infos.data);
		for (i32 i = 0; i < wait_events.size; i++) {
			vkCmdResetEvent2(cmd, wait_events[i], img_sync.img_barriers[i].dstStageMask);
		}
	} else if (!use_events) {
		for (const auto& info : img_sync.dependency_infos) {
			vkCmdPipelineBarrier2(cmd, &info);
		}
	}

	// Transition layouts inside the pass
	for (auto& [tex, old_layout, dst_layout] : layout_transitions) {
		vk::texture_force_transition(tex, cmd, old_layout, dst_layout);
	}

	// Push descriptors
	if (pipeline_storage->bound_resources.size) {
		vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipeline_storage->pipeline.update_template,
											  pipeline_storage->pipeline.pipeline_layout, 0, descriptor_infos);
	}
	// Push constants
	if (pipeline_storage->pipeline.push_constant_size) {
		vkCmdPushConstants(cmd, pipeline_storage->pipeline.pipeline_layout, pipeline_storage->pipeline.pc_stages, 0,
						   pipeline_storage->pipeline.push_constant_size, push_constant_data);
	}
	// Run
	if (!disable_execution) {
		switch (type) {
			case vk::PassType::RT: {
				LUMEN_ASSERT(pipeline_storage->pipeline.tlas_descriptor_set, "TLAS descriptor set cannot be NULL!");
				// This doesnt work because we can't push TLAS descriptor with
				// template...
				// vkCmdPushDescriptorSetWithTemplateKHR(cmd,
				// pipeline->rt_update_template, pipeline->pipeline_layout, 0,
				// &tlas_buffer.descriptor);
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
										pipeline_storage->pipeline.pipeline_layout, 1, 1,
										&pipeline_storage->pipeline.tlas_descriptor_set, 0, nullptr);

				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline_storage->pipeline.handle);

				if (settings.pass_func) {
					settings.pass_func(cmd, *this);
				} else {
					lm::SmallArray<VkStridedDeviceAddressRegionKHR, vk::NUM_SBT_GROUPS> regions = pipeline_storage->pipeline.get_rt_regions();
					lm::dim3& dims = settings.dims;
					vkCmdTraceRaysKHR(cmd, &regions[0], &regions[1], &regions[2], &regions[3], dims.x, dims.y, dims.z);
				}
				break;
			}
			case vk::PassType::Compute: {
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_storage->pipeline.handle);

				if (settings.pass_func) {
					settings.pass_func(cmd, *this);
				} else {
					lm::dim3& dims = settings.dims;
					vkCmdDispatch(cmd, dims.x, dims.y, dims.z);
				}
				break;
			}
			case vk::PassType::Graphics: {
				lm::SmallArray<vk::Texture*, vk::MAX_COLOR_ATTACHMENTS>& color_outputs = settings.color_outputs;
				vk::Texture* depth_output = settings.depth_output;
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_storage->pipeline.handle);

				u32& width = settings.width;
				u32& height = settings.height;
				VkViewport viewport = vk::viewport((f32)width, (f32)height, 0.0f, 1.0f);
				VkRect2D scissor = vk::rect2D(width, height, 0, 0);
				vkCmdSetViewport(cmd, 0, 1, &viewport);
				vkCmdSetScissor(cmd, 0, 1, &scissor);

				if (!settings.vertex_buffers.empty()) {
					lm::SmallArray<VkDeviceSize, MAX_VERTEX_BUFFERS> offsets;
					lm::SmallArray<VkBuffer, MAX_VERTEX_BUFFERS> vert_buffers;
					for (vk::Buffer* buf : settings.vertex_buffers) {
						vert_buffers.push_back(buf->handle);
					}
					vkCmdBindVertexBuffers(cmd, 0, (u32)vert_buffers.size, vert_buffers.data, offsets.data);
				}

				if (settings.index_buffer) {
					vkCmdBindIndexBuffer(cmd, settings.index_buffer->handle, 0, settings.index_type);
				}
				constexpr u64 MAX_RENDERING_ATTACHMENTS = 4;
				lm::SmallArray<VkRenderingAttachmentInfo, MAX_RENDERING_ATTACHMENTS> rendering_attachments;
				for (vk::Texture* color_output : color_outputs) {
					vk::texture_transition(color_output, cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
					rendering_attachments.push_back(vk::rendering_attachment_info(
						color_output->view, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR,
						VK_ATTACHMENT_STORE_OP_STORE, settings.clear_color));
				}
				VkRenderingAttachmentInfo depth_stencil_attachment;
				if (depth_output) {
					vk::texture_transition(depth_output, cmd, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
					depth_stencil_attachment = vk::rendering_attachment_info(
						depth_output->view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR,
						VK_ATTACHMENT_STORE_OP_STORE, settings.clear_depth_stencil);
				}

				// Render
				{
					VkRenderingInfo render_info{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
												.renderArea = {{0, 0}, {settings.width, settings.height}},
												.layerCount = 1,
												.colorAttachmentCount = (u32)color_outputs.size,
												.pColorAttachments = rendering_attachments.data,
												.pDepthAttachment = depth_output ? &depth_stencil_attachment : nullptr};
					vkCmdBeginRendering(cmd, &render_info);
					settings.pass_func(cmd, *this);
					vkCmdEndRendering(cmd);
				}

				// Present
				for (vk::Texture* color_output : color_outputs) {
					// If the texture is a swapchain image, it should be presented
					bool should_present = color_output->allocation == VK_NULL_HANDLE;
					if (should_present) {
						vk::texture_transition(color_output, cmd, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
					}
				}
				break;
			}
			default:
				break;
		}
	}

	// Post execution buffer barriers
	{
		lm::SmallArray<VkBufferMemoryBarrier2, MAX_RESOURCES_COPIES> post_execution_buffer_memory_barriers;
		if (!post_execution_buffer_barriers.empty()) {
			for (auto& barrier : post_execution_buffer_barriers) {
				VkPipelineStageFlags curr_stage = pipeline_stage_from_pass_type(type, barrier.src_access_flags);
				VkPipelineStageFlags dst_stage = pipeline_stage_from_pass_type(type, barrier.dst_access_flags);
				post_execution_buffer_memory_barriers.push_back(vk::buffer_barrier2(
					barrier.buffer, barrier.src_access_flags, barrier.dst_access_flags, curr_stage, dst_stage));
			}
			auto dependency_info = vk::dependency_info((u32)post_execution_buffer_memory_barriers.size,
													   post_execution_buffer_memory_barriers.data);
			vkCmdPipelineBarrier2(cmd, &dependency_info);
		}
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
				region.imageExtent = src.tex->extent;
				VkImageLayout old_layout = src.tex->layout;
				vk::texture_transition(src.tex, cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
				vkCmdCopyImageToBuffer(cmd, src.tex->handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.buf->handle, 1,
									   &region);
				vk::texture_transition(src.tex, cmd, old_layout);
			} else {
				LUMEN_ASSERT(src.tex->aspect_flags == dst.tex->aspect_flags, "Aspect flags mismatch");
				VkImageCopy region = {};
				vk::texture_transition(src.tex, cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
				vk::texture_transition(dst.tex, cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
				region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.srcSubresource.layerCount = 1;
				region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.dstSubresource.layerCount = 1;
				region.extent = src.tex->extent;
				vkCmdCopyImage(cmd, src.tex->handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.tex->handle,
							   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
			}
		} else {  // buffer
			if (dst.buf) {
				VkBufferCopy copy_region = {.size = src.buf->size};
				vkCmdCopyBuffer(cmd, src.buf->handle, dst.buf->handle, 1, &copy_region);
			}
		}
	}

	if (blas_build_data.is_valid()) {
		GPUQueryManager::begin(cmd, "BLAS Build");
		ScratchArena scratch = _arena_per_frame;
		vk::blas_build(scratch, blas_build_data.blases, blas_build_data.blas_inputs, blas_build_data.flags, cmd,
					   blas_build_data.scratch_buffer_ref);
		GPUQueryManager::end(cmd);

		if (tlas_build_data.build_tlas_after_blas) {
			LUMEN_ASSERT(tlas_build_data.tlas, "TLAS reference is null");
			void* instance_data = vk::buffer_map(tlas_build_data.instances_buf);
			for (u64 i = 0; i < blas_build_data.blases.size; i++) {
				VkAccelerationStructureInstanceKHR* instance = (VkAccelerationStructureInstanceKHR*)instance_data + i;
				instance->accelerationStructureReference = blas_build_data.blases[i].device_address();
			}
			vk::buffer_unmap(tlas_build_data.instances_buf);
#if 0
			// TODO: Do we need a barrier here?
			std::vector<VkBufferMemoryBarrier2> buffer_memory_barriers;
			auto curr_stage = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
			auto dst_stage = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
			buffer_memory_barriers.push_back(vk::buffer_barrier2(
				blas_build_data.blases[0].buffer->handle, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
				VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR, curr_stage, dst_stage));
			auto dependency_info =
				vk::dependency_info((u32)buffer_memory_barriers.size(), buffer_memory_barriers.data());
			vkCmdPipelineBarrier2(cmd, &dependency_info);
#endif
		}
	}

	if (tlas_build_data.is_valid()) {
		GPUQueryManager::begin(cmd, "TLAS Build");
		vk::tlas_build(*tlas_build_data.tlas, tlas_build_data.instances_buf, tlas_build_data.instance_count,
					   tlas_build_data.flags, cmd, tlas_build_data.scratch_buffer_ref);
		write_impl(tlas_build_data.tlas->buffer, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR);
		GPUQueryManager::end(cmd);
	}

	// Set: Buffer
	for (auto& entry : set_signals_buffer) {
		BufferSyncDescriptor& buffer_sync = entry.value;
		VkBuffer buffer = entry.key;
		LUMEN_ASSERT(buffer_sync.event == nullptr, "VkEvent should be null in the setter");
		VkBufferMemoryBarrier2 mem_barrier =
			vk::buffer_barrier2(buffer, buffer_sync.src_access_flags, buffer_sync.dst_access_flags,
								pipeline_stage_from_pass_type(type, buffer_sync.src_access_flags),
								pipeline_stage_from_pass_type(rg->passes[buffer_sync.opposing_pass_idx].type,
															  buffer_sync.dst_access_flags));
		VkDependencyInfo dependency_info = vk::dependency_info(1, &mem_barrier);

		if (use_events) {
			buffer_sync.event = vk::event_pool::get_event(cmd);
			vkCmdSetEvent2(cmd, buffer_sync.event, &dependency_info);
		}
	}

	// Set: Images
	for (auto& entry : set_signals_img) {
		ImageSyncDescriptor& img_sync = entry.value;
		VkImage img = entry.key;
		LUMEN_ASSERT(img_sync.event == nullptr, "VkEvent should be null in the setter");
		auto src_access_flags = vk::access_flags_for_img_layout(img_sync.old_layout);
		auto dst_access_flags = vk::access_flags_for_img_layout(img_sync.new_layout);
		auto mem_barrier = vk::image_barrier2(
			img, vk::access_flags_for_img_layout(img_sync.old_layout),
			vk::access_flags_for_img_layout(img_sync.new_layout), img_sync.old_layout, img_sync.new_layout,
			img_sync.image_aspect, pipeline_stage_from_pass_type(type, src_access_flags),
			pipeline_stage_from_pass_type(rg->passes[img_sync.opposing_pass_idx].type, dst_access_flags),
			vk::context().queue_indices.gfx_family.value());

		VkDependencyInfo dependency_info = vk::dependency_info(1, &mem_barrier);
		if (use_events) {
			img_sync.event = vk::event_pool::get_event(cmd);
			vkCmdSetEvent2(cmd, img_sync.event, &dependency_info);
		}
	}
	vk::end_region(vk::context().device, cmd);
	GPUQueryManager::end(cmd);
}

void RenderGraph::init() {
	if (!_arena_rendergraph) {
		_arena_rendergraph = lm::arena_create(CSTR("Render Graph Arena (Persistent)"), GB(1), MB(32));
		_arena_per_frame = lm::arena_create(CSTR("Render Graph Arena (Per frame)"), MB(16), MB(1));
	}
	// Arrays
	passes = lm::fixed_array_create<RenderPass>(_arena_rendergraph, MAX_PASSES_PER_FRAME);
	pipeline_tasks = lm::fixed_array_create<std::pair<std::function<void(RenderPass*)>, u32>>(_arena_rendergraph,
																							  MAX_PIPELINE_TASKS);
	shader_tasks = lm::fixed_array_create<std::function<void(RenderPass*)>>(_arena_rendergraph, 4 * MAX_PIPELINE_TASKS);
	// Hashmaps
	// Sizes are reasonable upper bounds but not hard bounds unlike arrays
	pipeline_cache = lm::hash_map_create<u64, PipelineStorage>(_arena_rendergraph, MAX_PASSES_PER_FRAME);
	buffer_resource_map =
		lm::hash_map_create<VkBuffer, std::pair<u32, VkAccessFlags>>(_arena_rendergraph, 32 * MAX_PASSES_PER_FRAME);
	img_resource_map = lm::hash_map_create<VkImage, u32>(_arena_rendergraph, 32 * MAX_PASSES_PER_FRAME);
	registered_buffer_pointers =
		lm::hash_map_create<lm::String, vk::Buffer*>(_arena_rendergraph, 32 * MAX_PASSES_PER_FRAME);
	shader_cache = lm::hash_map_create<lm::String, vk::Shader>(_arena_rendergraph, 4 * MAX_PASSES_PER_FRAME);
}

static u64 shader_render_pass_hash(const std::pair<vk::Shader*, RenderPass*>& entry) {
	return default_hash(entry.first->name_with_macros);
}

static bool shader_render_pass_eq(const std::pair<vk::Shader*, RenderPass*>& a,
								  const std::pair<vk::Shader*, RenderPass*>& b) {
	return a.first->name_with_macros == b.first->name_with_macros;
}

void RenderGraph::run(VkCommandBuffer cmd) {
	// Compile shaders and process resources
	const bool recording_or_reload = dirty_pass_encountered || reload_shaders;
	if (recording_or_reload) {
		lm::ScratchArena scratch = _arena_per_frame;
		auto unique_shaders_set =
			lm::hash_set_create<std::pair<vk::Shader*, RenderPass*>, shader_render_pass_hash, shader_render_pass_eq>(
				scratch.arena, MAX_SHADER_COMPILATIONS_PER_FRAME);
		// TODO: Make these FixedArrays SmallArrays
		auto existing_shaders_map = lm::hash_map_create<RenderPass*, lm::FixedArray<vk::Shader*>>(
			scratch.arena, MAX_SHADER_COMPILATIONS_PER_FRAME);
		auto unique_shaders_map = lm::hash_map_create<RenderPass*, lm::FixedArray<vk::Shader*>>(
			scratch.arena, MAX_SHADER_COMPILATIONS_PER_FRAME);

		for (auto i = 0; i < passes.size; i++) {
			if (passes[i].is_pipeline_cached) {
				continue;
			}
			for (vk::Shader& shader : passes[i].settings.shaders) {
				bool entry_created = false;
				unique_shaders_set.get_or_create({&shader, &passes[i]}, &entry_created);
				if (!entry_created) {
					auto entry = existing_shaders_map.get_or_create(&passes[i]);
					if (!entry->value.initialized()) {
						entry->value = lm::fixed_array_create<vk::Shader*>(scratch.arena, vk::MAX_SHADERS_PER_PASS);
					}
					entry->value.push_back(&shader);
				}
			}
		}

		lm::SmallArray<std::future<void>, MAX_PIPELINE_TASKS * 4> futures;
		for (const auto& entry : unique_shaders_set) {
			vk::Shader* shader = entry.key.first;
			RenderPass* rp = entry.key.second;
			auto unique_shader_entry = unique_shaders_map.get_or_create(rp);
			if (!unique_shader_entry->value.initialized()) {
				unique_shader_entry->value =
					lm::fixed_array_create<vk::Shader*>(scratch.arena, MAX_SHADER_COMPILATIONS_PER_FRAME);
			}
			unique_shader_entry->value.push_back(shader);
		}
		// Compile and process resources for unique shaders
		for (const auto& [hash, pass, shaders] : unique_shaders_map) {
			futures.push_back_move(ThreadPool::submit(std::bind(&build_shaders, pass, shaders)));
		}
		for (auto& future : futures) {
			future.wait();
		}
		futures.clear();
		// Process resources for duplicate shaders
		for (const auto& [hash, pass, shaders] : existing_shaders_map) {
			futures.push_back_move(ThreadPool::submit(std::bind(&build_shaders, pass, shaders)));
		}
		for (auto& future : futures) {
			future.wait();
		}
	}

	for (auto i = 0; i < passes.size; i++) {
		passes[i].finalize();
	}

	if (pipeline_tasks.size) {
		lm::SmallArray<std::future<void>, MAX_PIPELINE_TASKS> futures;
		for (auto& [task, idx] : pipeline_tasks) {
			if (task) {
				futures.push_back_move(ThreadPool::submit(task, &passes[idx]));
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

	for (auto i = 0; i < passes.size; i++) {
		passes[i].transition_resources();
	}

	for (auto i = 0; i < passes.size; i++) {
		passes[i].run(cmd);
	}
}

void RenderGraph::reset() {
	// TODO: Adjust to new implementation
	vk::event_pool::reset_events();
	for (auto& pass : passes) {
		////////////////////////////
		// --- Reset pass resources ---
		pass.resource_zeros.clear();
		pass.prefill_buffer_barriers.clear();
		pass.carryover_buffer_barriers.clear();
		pass.resource_copies.clear();
		pass.buffer_sync_resources.buffer_barriers.clear();
		pass.buffer_sync_resources.dependency_infos.clear();
		pass.image_sync_resources.img_barriers.clear();
		pass.image_sync_resources.dependency_infos.clear();
		pass.post_execution_buffer_barriers.clear();
		pass.explicit_buffer_writes.clear();
		pass.explicit_buffer_reads.clear();
		pass.explicit_tex_writes.clear();
		pass.explicit_tex_reads.clear();
		pass.descriptor_counts.clear();
		pass.layout_transitions.clear();
		pass.set_signals_buffer.clear();
		pass.wait_signals_buffer.clear();
		pass.set_signals_img.clear();
		pass.wait_signals_img.clear();

		// TODO: blas/tlas_build_data resets after we adjust them
		pass.next_binding_idx = 0;
		pass.next_as_binding_idx = 0;
		pass.disable_execution = false;

		pass.blas_build_data = {};
		pass.tlas_build_data = {};
		pass.settings = {};
	}
	passes.clear();
	pipeline_tasks.clear();
	_arena_per_frame->clear();
}

void RenderGraph::submit(vk::CommandBuffer& cmd) {
	cmd.submit();
	// This flushes all the existing timestamps
	// TODO: Maybe add a tracking mechanism inbetween frames per pass
	// This entails adding a mapping between a pass and a timestamp
	// Which enables us to get aggregate results for each pass per frame
	GPUQueryManager::collect();
	// The reset is needed here because the next subsequent pass may reuse the old pass' memory
	reset();
	dirty_pass_encountered = false;
}

void RenderGraph::run_and_submit(vk::CommandBuffer& cmd) {
	run(cmd.handle);
	submit(cmd);
}

static void populate_macros(lm::Arena* arena, const vk::ShaderMacroArray& macros, lm::String& macro_string,
							bool& prev_nonempty) {
	for (u64 i = 0; i < macros.size; i++) {
		if (!macros[i].visible) {
			continue;
		}
		if (!macros[i].name.empty()) {
			if (prev_nonempty) {
				macro_string = lm::str_concat(arena, macro_string, ",");
			}
			macro_string = lm::str_concat(arena, macro_string, macros[i].name);
			prev_nonempty = true;
		}
		if (macros[i].has_val) {
			macro_string = lm::str_concat(arena, macro_string, "=");
			macro_string = lm::str_concat(arena, macro_string, lm::str_from_s64(arena, macros[i].val));
		}
	}
}

PipelineStorage* RenderGraph::add_pass_impl_common(const lm::String& name, const vk::ShaderMacroArray& macros,
												   const lm::SpecializationConstantArray& specialization_data,
												   bool& cached, lm::String& name_with_macros,
												   lm::String& macro_string) {
	assert(name.is_cstr());
	PipelineStorage* pipeline_storage;

	name_with_macros = lm::str_dup(_arena_per_frame, name);

	if (!macros.empty() || !global_macro_defines.empty()) {
		macro_string = lm::str_concat(_arena_per_frame, macro_string, "(");
	}

	bool prev_nonempty = false;
	populate_macros(_arena_per_frame, macros, macro_string, prev_nonempty);
	populate_macros(_arena_per_frame, global_macro_defines, macro_string, prev_nonempty);

	if (!macros.empty() || !global_macro_defines.empty()) {
		macro_string = lm::str_concat(_arena_per_frame, macro_string, ")");
	}
	if (macro_string == "()") {
		macro_string = "";
	}
	name_with_macros = lm::str_concat(_arena_per_frame, name_with_macros, macro_string, /*cstr=*/true);

	u64 hash = 0;
	hash = lm::fnv1a_hash((void*)name_with_macros.data, name_with_macros.size, lm::HASH_INIT);
	for (u32 spec_data : specialization_data) {
		util::hash_combine(hash, spec_data);
	}

	auto entry = pipeline_cache.find(hash);
	if (entry && (!reload_shaders || entry->value.reload_counter == reload_counter)) {
		pipeline_storage = &entry->value;
		cached = true;
	} else {
		dirty_pass_encountered = true;
		if (entry) {
			// reload shaders
			vkDeviceWaitIdle(vk::context().device);
			entry->value.pipeline.cleanup();
		}
		auto new_entry = pipeline_cache.insert(
			hash, PipelineStorage{.pipeline = vk::Pipeline(lm::str_dup(_arena_rendergraph, name_with_macros))});
		new_entry->value.reload_counter = reload_counter;
		pipeline_storage = &new_entry->value;
	}
	if (!pipeline_storage->affected_buffer_pointers.initialized()) {
		pipeline_storage->affected_buffer_pointers =
			lm::hash_map_create<lm::String, vk::BufferStatus>(_arena_rendergraph, MAX_BUFFER_BARRIERS);
	}
	return pipeline_storage;
}
void RenderGraph::destroy() {
	for (auto& entry : pipeline_cache) {
		entry.value.pipeline.cleanup();
	}
}

lm::Arena* RenderGraph::arena() {
	assert(_arena_rendergraph);
	return _arena_rendergraph;
}

void render_pass_init_gfx(RenderPass& pass, vk::PassType type, const lm::String& name, RenderGraph* rg, u32 pass_idx,
						  const vk::GraphicsPassSettings& gfx_settings, const lm::String& macro_string,
						  PipelineStorage* pipeline_storage, bool cached) {
	assert(name.is_cstr());
	pass.type = type;
	pass.rg = rg;
	pass.pass_idx = pass_idx;
	pass.pipeline_storage = pipeline_storage;
	pass.name = name;
	pass.is_pipeline_cached = cached;
	vk::PassSettings& settings = pass.settings;
	// Common
	settings.shaders = gfx_settings.shaders;
	settings.macros = gfx_settings.macros;
	settings.specialization_data = gfx_settings.specialization_data;
	settings.pass_func = gfx_settings.pass_func;
	// Graphics
	settings.width = gfx_settings.width;
	settings.height = gfx_settings.height;
	settings.clear_color = gfx_settings.clear_color;
	settings.clear_depth_stencil = gfx_settings.clear_depth_stencil;
	settings.cull_mode = gfx_settings.cull_mode;
	settings.vertex_buffers = gfx_settings.vertex_buffers;
	settings.index_buffer = gfx_settings.index_buffer;
	settings.blend_enables = gfx_settings.blend_enables;
	settings.front_face = gfx_settings.front_face;
	settings.topology = gfx_settings.topology;
	settings.polygon_mode = gfx_settings.polygon_mode;
	settings.sample_count = gfx_settings.sample_count;
	settings.index_type = gfx_settings.index_type;
	settings.line_width = gfx_settings.line_width;
	settings.color_outputs = gfx_settings.color_outputs;
	settings.depth_output = gfx_settings.depth_output;
	if (!pass.is_pipeline_cached) {
		for (vk::Shader& shader : settings.shaders) {
			shader.name_with_macros = lm::str_concat(_arena_rendergraph, shader.filename, macro_string, /*cstr=*/true);
		}
	}
	pass.init();
}

void render_pass_init_rt(RenderPass& pass, vk::PassType type, const lm::String& name, RenderGraph* rg, u32 pass_idx,
						 const vk::RTPassSettings& rt_settings, const lm::String& macro_string,
						 PipelineStorage* pipeline_storage, bool cached) {
	assert(name.is_cstr());
	pass.type = type;
	pass.rg = rg;
	pass.pass_idx = pass_idx;
	pass.pipeline_storage = pipeline_storage;
	pass.name = name;
	pass.is_pipeline_cached = cached;
	vk::PassSettings& settings = pass.settings;
	// Common
	settings.shaders = rt_settings.shaders;
	settings.macros = rt_settings.macros;
	settings.specialization_data = rt_settings.specialization_data;
	settings.dims = rt_settings.dims;
	settings.pass_func = rt_settings.pass_func;
	// RT
	settings.recursion_depth = rt_settings.recursion_depth;
	if (!pass.is_pipeline_cached) {
		for (vk::Shader& shader : settings.shaders) {
			shader.name_with_macros = lm::str_concat(_arena_rendergraph, shader.filename, macro_string, /*cstr=*/true);
		}
	}
	pass.init();
}

void render_pass_init_compute(RenderPass& pass, vk::PassType type, const lm::String& name, RenderGraph* rg,
							  u32 pass_idx, const vk::ComputePassSettings& compute_settings,
							  const lm::String& macro_string, PipelineStorage* pipeline_storage, bool cached) {
	assert(name.is_cstr());
	pass.type = type;
	pass.rg = rg;
	pass.pass_idx = pass_idx;
	pass.pipeline_storage = pipeline_storage;
	pass.name = name;
	pass.is_pipeline_cached = cached;
	vk::PassSettings& settings = pass.settings;
	// Common
	settings.shaders.push_back(compute_settings.shader);
	settings.macros = compute_settings.macros;
	settings.specialization_data = compute_settings.specialization_data;
	settings.dims = compute_settings.dims;
	settings.pass_func = compute_settings.pass_func;
	if (!pass.is_pipeline_cached) {
		settings.shaders[0].name_with_macros =
			lm::str_concat(_arena_rendergraph, compute_settings.shader.filename, macro_string, /*cstr=*/true);
	}
	pass.init();
}

}  // namespace lm
