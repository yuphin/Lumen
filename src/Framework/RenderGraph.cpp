#include "Framework/RenderGraph.h"
#include "CommandBuffer.h"
#include "EventPool.h"
#include "VkUtils.h"
#include "GPUQueryManager.h"
#include "DynamicResourceManager.h"
#include "Framework/Base/OS.h"
#include "Framework/ThreadPool.h"

////////////////////////////
// --- Limits for fixed size arrays inside Render Graph ---
static constexpr u64 MAX_PASSES_PER_FRAME = 1024;
static constexpr u64 MAX_SYNC_ENTRIES_PER_PASS = 32;
////////////////////////////
// --- Limits for shader and pipeline compilation inside render graph ---
static constexpr u64 MAX_PIPELINE_TASKS = 128;
static constexpr u64 MAX_SHADER_COMPILATIONS_PER_FRAME = 1024;
constexpr u64 MAX_RENDERING_ATTACHMENTS = 4;
////////////////////////////
// --- Initial capacities for render graph hashmaps
static constexpr u64 INITIAL_BUFFER_RESOURCE_COUNT = 1024;
static constexpr u64 INITIAL_IMG_RESOURCE_COUNT = 256;
static constexpr u64 INITIAL_REGISTERED_BUFFER_POINTER_COUNT = 1024;
static constexpr u64 INITIAL_SHADER_CACHE_COUNT = 512;

using namespace lm;

namespace rg {

static lm::Arena* _arena_rendergraph = nullptr;
static lm::Arena* _arena_per_frame = nullptr;
static vk::ShaderMacroArray _global_macro_defines;
static lm::FixedArray<PipelineTask> _pipeline_tasks;
static lm::FixedArray<RenderPass> _passes;
static lm::HashMap<u64, PipelineStorage> _pipeline_cache;
static lm::HashMap<VkBuffer, BufferResourceState> _buffer_resource_map;
static lm::HashMap<VkImage, ImageResourceState> _img_resource_map;
static lm::HashMap<lm::String, vk::Buffer*> _registered_buffer_pointers;
static lm::HashMap<lm::String, vk::Shader> _shader_cache;
static RenderGraphSettings _settings;
static os::Mutex _shader_map_mutex;
static const bool _multithreaded_pipeline_compilation = true;
static bool _dirty_pass_encountered = false;
static bool _reload_shaders = false;
static u32 _reload_counter = 0;
static u64 _generation = 1;
static constexpr u32 INVALID_PASS_IDX = U32_MAX;

}  // namespace rg

using namespace rg;

// TODO: Investigate get_or_create behavior

static PipelineStorage* add_pass_impl_common(const lm::String& name, const vk::ShaderMacroArray& macros,
											 const lm::SpecializationConstantArray& specialization_data, bool& cached,
											 lm::String& macro_string);

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

static bool is_write_flag(VkAccessFlags flags) {
	return flags & (VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
					VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR);
}

static bool is_read_only(VkAccessFlags flags) { return flags != 0 && !is_write_flag(flags); }

static VkBufferMemoryBarrier2 buffer_barrier_from_sync(VkBuffer buffer, const BufferSyncDescriptor& sync) {
	return vk::buffer_barrier2(buffer, sync.src_access_flags, sync.dst_access_flags, sync.src_stage, sync.dst_stage);
}

static VkImageMemoryBarrier2 image_barrier_from_sync(VkImage image, const ImageSyncDescriptor& sync) {
	return vk::image_barrier2(image, sync.src_access_flags, sync.dst_access_flags, sync.old_layout, sync.new_layout,
							  sync.image_aspect, sync.src_stage, sync.dst_stage,
							  vk::context().queue_indices.gfx_family);
}

template <typename T>
static T* push_per_frame(const T& value) {
	return new (_arena_per_frame->allocate(sizeof(T), alignof(T), nullptr, /*zero_initialize=*/false)) T(value);
}

static void render_pass_init(RenderPass& pass, vk::PassType type, u32 pass_idx, const vk::PassSettings& settings,
							 const lm::String& macro_string, PipelineStorage* pipeline_storage, bool cached) {
	pass.type = type;
	pass.pass_idx = pass_idx;
	pass.pipeline_storage = pipeline_storage;
	pass.name = pipeline_storage->pipeline.name;
	pass.is_pipeline_cached = cached;
	pass.dims = settings.dims;
	pass.pass_func = settings.pass_func;
	pass.gfx = nullptr;
	pass.build = nullptr;
	if (!cached) {
		pass.build = push_per_frame(PipelineBuild{.settings = settings});
		if (type == vk::PassType::Compute) {
			LUMEN_ASSERT(settings.shaders.empty(), "Compute passes take a single shader via .shader");
			pass.build->shaders.push_back(vk::Shader(settings.shader));
		} else {
			for (const lm::String& filename : settings.shaders) {
				pass.build->shaders.push_back(vk::Shader(filename));
			}
		}
		for (vk::Shader& shader : pass.build->shaders) {
			shader.name_with_macros = lm::str_concat(_arena_rendergraph, shader.filename, macro_string, /*cstr=*/true);
		}
	}
	pass.init();
}

template <typename K, typename V>
static void sync_list_insert(SyncList<K, V>& list, K key, const V& value) {
	for (SyncEntry<K, V>& entry : list) {
		if (entry.key == key) {
			entry.value = value;
			return;
		}
	}
	if (list.size == list.capacity) {
		LUMEN_ERROR("Pass exceeded MAX_SYNC_ENTRIES_PER_PASS");
	}
	list.push_back({key, value});
}

template <typename K, typename V>
static const V& sync_list_find(const SyncList<K, V>& list, K key) {
	for (const SyncEntry<K, V>& entry : list) {
		if (entry.key == key) {
			return entry.value;
		}
	}
	LUMEN_ERROR("Missing sync entry");
	return list[0].value;
}

static void process_bindless_resources(RenderPass* pass, vk::Shader& shader) {
	if (!_settings.shader_inference) {
		return;
	}
	for (const auto& entry : shader.buffer_status_map) {
		vk::BufferStatus& status = pass->pipeline_storage->affected_buffer_pointers.get_or_create(entry.key)->value;
		status.read |= entry.value.read;
		status.write |= entry.value.write;
	}
}

static void process_bindings(RenderPass* pass, vk::Shader& shader) {
	for (const auto& entry : shader.resource_binding_map) {
		assert(entry.key < pass->pipeline_storage->bound_resources.size);
		ResourceBinding& binding = pass->pipeline_storage->bound_resources[entry.key];
		binding.active |= entry.value.active;
		binding.read |= entry.value.read;
		binding.write |= entry.value.write;
	}
}

struct ShaderCompileTask {
	RenderPass* pass;
	vk::Shader* shader;
	vk::Shader* result;
};

static void compile_shader(void* raw_task) {
	ShaderCompileTask* task = (ShaderCompileTask*)raw_task;
	task->result = task->shader->compile(task->pass) == 0 ? task->shader : nullptr;
}

struct BuildShadersTask {
	RenderPass* pass;
	lm::SmallArray<vk::Shader*, vk::MAX_SHADERS_PER_PASS> shaders;
};

struct PipelineRunTask {
	PipelineTask task;
	RenderPass* pass;
};

static void build_shaders(RenderPass* pass,
						  const lm::SmallArray<vk::Shader*, vk::MAX_SHADERS_PER_PASS>& active_shaders) {
	// TODO: make resource processing in order
	switch (pass->type) {
		case vk::PassType::RT:
		case vk::PassType::Graphics: {
			lm::SmallArray<ShaderCompileTask, vk::MAX_SHADERS_PER_PASS> shader_tasks;
			tp::JobCounter shader_counter;
			for (vk::Shader* shader : active_shaders) {
				bool shader_cached = false;
				{
					os::ScopedLock lock(_shader_map_mutex);
					auto* shader_entry = _shader_cache.find(shader->name_with_macros);
					if (shader_entry) {
						*shader = shader_entry->value;
						shader_cached = true;
					}
				}
				if (!shader_cached) {
					ShaderCompileTask& task = shader_tasks.push();
					task = {.pass = pass, .shader = shader, .result = nullptr};
					tp::submit({compile_shader, &task}, shader_counter);
				}
			}
			tp::wait(shader_counter);
			for (ShaderCompileTask& task : shader_tasks) {
				if (!task.result) {
					LUMEN_ERROR("Shader compilation failed");
				}
				{
					os::ScopedLock lock(_shader_map_mutex);
					_shader_cache.insert(task.result->name_with_macros, *task.result);
				}
			}
			for (vk::Shader* shader : active_shaders) {
				process_bindless_resources(pass, *shader);
				process_bindings(pass, *shader);
			}
		} break;
		case vk::PassType::Compute: {
			for (vk::Shader* shader : active_shaders) {
				bool shader_cached = false;
				{
					os::ScopedLock lock(_shader_map_mutex);
					auto* shader_entry = _shader_cache.find(shader->name_with_macros);
					if (shader_entry) {
						*shader = shader_entry->value;
						shader_cached = true;
					}
				}
				if (!shader_cached) {
					if (shader->compile(pass) != 0) {
						LUMEN_ERROR("Shader compilation failed");
					}
					{
						os::ScopedLock lock(_shader_map_mutex);
						_shader_cache.insert(shader->name_with_macros, *shader);
					}
				}
				process_bindless_resources(pass, *shader);
				process_bindings(pass, *shader);
			}
		} break;
		default:
			break;
	}
}

static void build_shaders_job(void* raw_task) {
	BuildShadersTask* task = (BuildShadersTask*)raw_task;
	build_shaders(task->pass, task->shaders);
}

static void run_pipeline_job(void* raw_task) {
	PipelineRunTask* task = (PipelineRunTask*)raw_task;
	task->task.procedure(task->pass);
}

bool RenderPass::register_dependencies(const vk::Buffer* buffer, VkAccessFlags dst_access_flags, BufferSyncFlags flags,
									   VkPipelineStageFlags dst_stage) {
	// Invariant : Pass with lower index should be the setter before the cmd buffer submission
	auto* entry = _buffer_resource_map.find(buffer->handle);

	if (!entry) {
		return true;
	}
	u32 opposing_pass_idx = entry->value.pass_idx;
	// If we're crossing frame, this dependency is not event elligible
	const bool cross_frame_history = entry->value.generation != _generation;
	if (is_read_only(dst_access_flags) && is_read_only(entry->value.access_flags)) {
		return entry->value.event_eligible && !cross_frame_history;
	}
	VkAccessFlags src_access_flags = entry->value.access_flags;
	VkPipelineStageFlags src_stage = entry->value.stage;
	if (!dst_stage) {
		dst_stage = pipeline_stage_from_pass_type(type, dst_access_flags);
	}
	// If this condition fails, that means the cmd buffer is already submitted last frame
	// For single cmd buffer setup, ignoring the else condition would be fine
	// However when multiple cmd buffers are in flight, we must still ensure the synchronization
	if (!cross_frame_history && opposing_pass_idx < pass_idx) {
		RenderPass& opposing_pass = _passes[opposing_pass_idx];
		sync_list_insert(wait_signals_buffer, buffer->handle,
						 BufferSyncDescriptor{
							 .src_access_flags = src_access_flags,
							 .dst_access_flags = dst_access_flags,
							 .src_stage = src_stage,
							 .dst_stage = dst_stage,
							 .opposing_pass_idx = opposing_pass.pass_idx,
							 .event_eligible = entry->value.event_eligible,
						 });
		sync_list_insert(opposing_pass.set_signals_buffer, buffer->handle,
						 BufferSyncDescriptor{
							 .src_access_flags = src_access_flags,
							 .dst_access_flags = dst_access_flags,
							 .src_stage = src_stage,
							 .dst_stage = dst_stage,
							 .opposing_pass_idx = pass_idx,
							 .event_eligible = entry->value.event_eligible,
						 });
	} else {
		if (flags == BufferSyncFlags::BUFFER_COPY || flags == BufferSyncFlags::BUFFER_AS_BUILD) {
			// Resource copies happens after the pass execution
			frame.post_execution_buffer_barriers.push_back(
				{buffer->handle, src_access_flags, dst_access_flags, src_stage, dst_stage});
		} else {
			if (flags == BufferSyncFlags::BUFFER_ZERO) {
				LUMEN_ASSERT(dst_access_flags == VK_ACCESS_TRANSFER_WRITE_BIT, "Invalid buffer zero flags");
				// TODO: Do we need this?
				// This case happens when there are no dependencies to the buffer being cleared inside the render
				// graph in a frame Yet we have to ensure syncronization because there are multiple command buffers
				// in flight
				frame.prefill_buffer_barriers.push_back(
					{buffer->handle, src_access_flags, dst_access_flags, src_stage, dst_stage});
			} else {
				frame.carryover_buffer_barriers.push_back(
					{buffer->handle, src_access_flags, dst_access_flags, src_stage, dst_stage});
			}
		}
	}
	return entry->value.event_eligible && !cross_frame_history;
}

bool RenderPass::register_dependencies(vk::Texture* tex, VkAccessFlags dst_access_flags, VkImageLayout dst_layout) {
	const auto* entry = _img_resource_map.find(tex->handle);
	if (!entry) {
		if (tex->layout == dst_layout) {
			return true;
		}
		frame.layout_transitions.push_back({tex, tex->layout, dst_layout});
		tex->layout = dst_layout;
		return true;
	}

	const ImageResourceState& image_state = entry->value;
	const bool cross_frame_history = image_state.generation != _generation;
	const bool read_after_read = is_read_only(image_state.access_flags) && is_read_only(dst_access_flags);
	if (read_after_read && image_state.layout == dst_layout) {
		tex->layout = dst_layout;
		return image_state.event_eligible && !cross_frame_history;
	}
	const VkAccessFlags src_access_flags = image_state.access_flags;
	const VkPipelineStageFlags src_stage = image_state.stage;

	if (!cross_frame_history) {
		RenderPass& opposing_pass = _passes[image_state.pass_idx];
		if (opposing_pass.pass_idx < pass_idx) {
			const VkPipelineStageFlags dst_stage = pipeline_stage_from_pass_type(type, dst_access_flags);
			sync_list_insert(wait_signals_img, tex->handle,
							 ImageSyncDescriptor{
								 .old_layout = image_state.layout,
								 .new_layout = dst_layout,
								 .src_access_flags = src_access_flags,
								 .dst_access_flags = dst_access_flags,
								 .src_stage = src_stage,
								 .dst_stage = dst_stage,
								 .image_aspect = tex->aspect_flags,
								 .opposing_pass_idx = opposing_pass.pass_idx,
								 .event_eligible = image_state.event_eligible,
							 });
			sync_list_insert(opposing_pass.set_signals_img, tex->handle,
							 ImageSyncDescriptor{
								 .old_layout = image_state.layout,
								 .new_layout = dst_layout,
								 .src_access_flags = src_access_flags,
								 .dst_access_flags = dst_access_flags,
								 .src_stage = src_stage,
								 .dst_stage = dst_stage,
								 .image_aspect = tex->aspect_flags,
								 .opposing_pass_idx = pass_idx,
								 .event_eligible = image_state.event_eligible,
							 });

			tex->layout = dst_layout;
		} else if (image_state.layout != dst_layout) {
			// Multiple accesses inside one pass cannot be ordered by the graph.
			frame.layout_transitions.push_back({tex, image_state.layout, dst_layout});
			tex->layout = dst_layout;
		}
	} else {
		frame.carryover_image_barriers.push_back({
			.image = tex->handle,
			.src_access_flags = src_access_flags,
			.dst_access_flags = dst_access_flags,
			.old_layout = image_state.layout,
			.new_layout = dst_layout,
			.image_aspect = image_state.image_aspect,
			.src_stage = src_stage,
			.dst_stage = pipeline_stage_from_pass_type(type, dst_access_flags),
		});
		tex->layout = dst_layout;
	}
	return image_state.event_eligible && !cross_frame_history;
}

void RenderPass::transition_resources() {
	for (const Resource& resource : frame.resource_zeros) {
		if (resource.buf) {
			write_impl(resource.buf, VK_ACCESS_TRANSFER_WRITE_BIT, BufferSyncFlags::BUFFER_ZERO);
		} else {
			write_impl(resource.tex);
		}
	}

	if (_settings.shader_inference) {
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
			const VkAccessFlags access_flags = (bound_resource.read ? VK_ACCESS_SHADER_READ_BIT : 0) |
											   (bound_resource.write ? VK_ACCESS_SHADER_WRITE_BIT : 0);
			if (bound_resource.write) {
				if (bound_resource.buf) {
					write_impl(bound_resource.buf, access_flags);
				} else {
					write_impl(bound_resource.tex, access_flags);
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
			lm::String buffer_str = entry.key;
			assert(!buffer_str.empty());
			vk::BufferStatus status = entry.value;
			auto buffer_ptr = _registered_buffer_pointers.find(buffer_str);
			assert(buffer_ptr);
			vk::Buffer* buffer = buffer_ptr->value;
			const VkAccessFlags access_flags =
				(status.read ? VK_ACCESS_SHADER_READ_BIT : 0) | (status.write ? VK_ACCESS_SHADER_WRITE_BIT : 0);
			if (status.write) {
				write_impl(buffer, access_flags);
			} else if (status.read) {
				read_impl(buffer);
			}
		}
	} else {
		for (i32 i = 0; i < pipeline_storage->bound_resources.size; i++) {
			descriptor_infos[i] = pipeline_storage->bound_resources[i].get_descriptor_info();
		}
	}

	for (vk::Buffer* buffer : frame.explicit_buffer_reads) {
		read_impl(buffer);
	}
	for (vk::Buffer* buffer : frame.explicit_buffer_writes) {
		write_impl(buffer, VK_ACCESS_SHADER_WRITE_BIT);
	}
	for (vk::Texture* texture : frame.explicit_tex_reads) {
		read_impl(texture);
	}
	for (vk::Texture* texture : frame.explicit_tex_writes) {
		write_impl(texture);
	}

	if (type == vk::PassType::Graphics) {
		for (vk::Buffer* buffer : gfx->vertex_buffers) {
			read_impl(buffer, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, BufferSyncFlags::NONE,
					  VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
		}
		if (gfx->index_buffer) {
			read_impl(gfx->index_buffer, VK_ACCESS_INDEX_READ_BIT, BufferSyncFlags::NONE,
					  VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
		}
		for (u64 i = 0; i < gfx->color_outputs.size; ++i) {
			VkAccessFlags access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			const VkAttachmentLoadOp load_op =
				i < gfx->color_load_ops.size ? gfx->color_load_ops[i] : VK_ATTACHMENT_LOAD_OP_CLEAR;
			if (load_op == VK_ATTACHMENT_LOAD_OP_LOAD) {
				access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
			}
			write_impl(gfx->color_outputs[i], access, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		}
		if (gfx->depth_output) {
			VkAccessFlags access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			if (gfx->depth_load_op == VK_ATTACHMENT_LOAD_OP_LOAD) {
				access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			}
			write_impl(gfx->depth_output, access, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
		}
	}

	for (const vk::BVH& as : pipeline_storage->as_bindings) {
		if (as.buffer) {
			read_impl(as.buffer, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR, BufferSyncFlags::NONE,
					  VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
		}
	}

	for (const auto& [src, dst] : frame.resource_copies) {
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

	if (frame.blas_build_data.is_valid()) {
		for (vk::Buffer* buf : frame.blas_build_data.source_buffers) {
			read_impl(buf, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_SHADER_READ_BIT,
					  BufferSyncFlags::BUFFER_AS_BUILD, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
		}
		for (vk::BVH& blas : frame.blas_build_data.blases) {
			if (blas.buffer) {
				write_impl(blas.buffer, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
						   BufferSyncFlags::BUFFER_AS_BUILD, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
			}
		}
	}

	if (frame.tlas_build_data.is_valid()) {
		read_impl(frame.tlas_build_data.instances_buf, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
				  BufferSyncFlags::BUFFER_AS_BUILD, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
		if (frame.tlas_build_data.tlas->buffer) {
			write_impl(frame.tlas_build_data.tlas->buffer, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
					   BufferSyncFlags::BUFFER_AS_BUILD, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
		}
	}
}

RenderPass& rg::current_pass() { return _passes[_passes.size - 1]; }

static RenderPass& add_pass(vk::PassType type, const lm::String& name, const vk::PassSettings& settings) {
	bool cached = false;
	lm::String macro_string;
	PipelineStorage* pipeline_storage =
		add_pass_impl_common(name, settings.macros, settings.specialization_data, cached, macro_string);
	RenderPass& pass = _passes.push();
	render_pass_init(pass, type, (u32)_passes.size - 1, settings, macro_string, pipeline_storage, cached);
	return pass;
}

RenderPass& rg::add_rt(const lm::String& name, const vk::RTPassSettings& rt_settings) {
	return add_pass(vk::PassType::RT, name, rt_settings);
}

RenderPass& rg::add_gfx(const lm::String& name, const vk::PassSettings& settings,
						const vk::GraphicsPassSettings& gfx_settings) {
	RenderPass& pass = add_pass(vk::PassType::Graphics, name, settings);
	pass.gfx = push_per_frame(gfx_settings);
	return pass;
}

RenderPass& rg::add_compute(const lm::String& name, const vk::ComputePassSettings& compute_settings) {
	return add_pass(vk::PassType::Compute, name, compute_settings);
}

void RenderPass::init() {
	if (resources_initialized) {
		return;
	}
	using BufferSyncEntry = SyncEntry<VkBuffer, BufferSyncDescriptor>;
	using ImageSyncEntry = SyncEntry<VkImage, ImageSyncDescriptor>;
	set_signals_buffer = lm::fixed_array_create<BufferSyncEntry>(_arena_rendergraph, MAX_SYNC_ENTRIES_PER_PASS);
	wait_signals_buffer = lm::fixed_array_create<BufferSyncEntry>(_arena_rendergraph, MAX_SYNC_ENTRIES_PER_PASS);
	set_signals_img = lm::fixed_array_create<ImageSyncEntry>(_arena_rendergraph, MAX_SYNC_ENTRIES_PER_PASS);
	wait_signals_img = lm::fixed_array_create<ImageSyncEntry>(_arena_rendergraph, MAX_SYNC_ENTRIES_PER_PASS);
	resources_initialized = true;
}

RenderPass& RenderPass::bind(const ResourceBinding& binding) {
	if (frame.next_binding_idx >= pipeline_storage->bound_resources.size) {
		pipeline_storage->bound_resources.push_back(binding);
		frame.descriptor_counts.push_back(1);
	} else {
		pipeline_storage->bound_resources[frame.next_binding_idx].replace(binding);
	}
	frame.next_binding_idx++;
	return *this;
}

RenderPass& RenderPass::bind(std::initializer_list<ResourceBinding> bindings) {
	for (const ResourceBinding& binding : bindings) {
		bind(binding);
	}
	return *this;
}
RenderPass& RenderPass::bind_texture_with_sampler(vk::Texture* tex, VkSampler sampler) {
	if (frame.next_binding_idx >= pipeline_storage->bound_resources.size) {
		pipeline_storage->bound_resources.emplace_back(tex, sampler);
		frame.descriptor_counts.push_back(1);
	} else {
		pipeline_storage->bound_resources[frame.next_binding_idx].replace(tex, sampler);
	}
	frame.next_binding_idx++;
	return *this;
}

RenderPass& RenderPass::bind_texture_array(lm::FixedArray<vk::Texture*> textures, bool force_update) {
	if (frame.next_binding_idx >= pipeline_storage->bound_resources.size) {
		for (vk::Texture*& texture : textures) {
			pipeline_storage->bound_resources.emplace_back(texture);
		}
		frame.descriptor_counts.push_back((u32)textures.size);
	} else {
		for (u64 i = 0; i < textures.size; i++) {
			pipeline_storage->bound_resources[frame.next_binding_idx + i].replace(textures[i]);
		}
	}
	return *this;
}

RenderPass& RenderPass::bind_buffer_array(lm::FixedArray<vk::Buffer*> buffers, bool force_update) {
	if (frame.next_binding_idx >= pipeline_storage->bound_resources.size) {
		for (vk::Buffer*& buffer : buffers) {
			pipeline_storage->bound_resources.emplace_back(buffer);
		}
		frame.descriptor_counts.push_back((u32)buffers.size);
	} else {
		for (u64 i = 0; i < buffers.size; i++) {
			pipeline_storage->bound_resources[frame.next_binding_idx + i].replace(buffers[i]);
		}
	}
	return *this;
}

RenderPass& RenderPass::bind_tlas(const vk::BVH& tlas) {
	LUMEN_ASSERT(type == vk::PassType::RT, "TLAS can only be bound to RT pipelines");
	LUMEN_ASSERT(pipeline_storage->as_bindings.size <= vk::MAX_AS_BINDING_COUNT &&
					 frame.next_as_binding_idx < vk::MAX_AS_BINDING_COUNT,
				 "Only two TLAS bindings are supported for now");
	if (frame.next_as_binding_idx >= pipeline_storage->as_bindings.size) {
		pipeline_storage->as_bindings.push_back(tlas);
	} else {
		if (pipeline_storage->as_bindings[frame.next_as_binding_idx].accel != tlas.accel) {
			pipeline_storage->update_as_descriptor = true;
		}
		pipeline_storage->as_bindings[frame.next_as_binding_idx] = tlas;
	}
	frame.next_as_binding_idx++;
	return *this;
}

RenderPass& RenderPass::read(vk::Buffer* buffer) {
	frame.explicit_buffer_reads.push_back(buffer);
	return *this;
}

RenderPass& RenderPass::read(vk::Texture* tex) {
	frame.explicit_tex_reads.push_back(tex);
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
	frame.explicit_buffer_writes.push_back(buffer);
	return *this;
}

RenderPass& RenderPass::write(vk::Texture* tex) {
	frame.explicit_tex_writes.push_back(tex);
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
	frame.disable_execution = condition;
	return *this;
}

RenderPass& RenderPass::push_constants(void* data, u64 size, u64 alignment) {
	frame.push_constant_data = _arena_per_frame->allocate(size, alignment, nullptr, /*zero_initialize=*/false);
	memcpy(frame.push_constant_data, data, size);
	return *this;
}

void RenderPass::push_descriptors(VkCommandBuffer cmd, const vk::DescriptorInfo* descriptors) const {
	if (!pipeline_storage->pipeline.update_template) return;
	vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipeline_storage->pipeline.update_template,
										  pipeline_storage->pipeline.pipeline_layout, 0, descriptors);
}

void RenderPass::bind_graphics_state(VkCommandBuffer cmd) const {
	LUMEN_ASSERT(type == vk::PassType::Graphics, "Only graphics passes have graphics state");
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_storage->pipeline.handle);

	VkViewport viewport = vk::viewport((f32)gfx->width, (f32)gfx->height, 0.0f, 1.0f);
	VkRect2D scissor = vk::rect2D(gfx->width, gfx->height, 0, 0);
	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	if (!gfx->vertex_buffers.empty()) {
		lm::SmallArray<VkDeviceSize, MAX_VERTEX_BUFFERS> offsets;
		lm::SmallArray<VkBuffer, MAX_VERTEX_BUFFERS> vertex_buffers;
		for (vk::Buffer* buffer : gfx->vertex_buffers) {
			vertex_buffers.push_back(buffer->handle);
			offsets.push_back(0);
		}
		vkCmdBindVertexBuffers(cmd, 0, (u32)vertex_buffers.size, vertex_buffers.data, offsets.data);
	}
	if (gfx->index_buffer) {
		vkCmdBindIndexBuffer(cmd, gfx->index_buffer->handle, 0, gfx->index_type);
	}
	if (pipeline_storage->pipeline.push_constant_size) {
		vkCmdPushConstants(cmd, pipeline_storage->pipeline.pipeline_layout, pipeline_storage->pipeline.pc_stages, 0,
						   pipeline_storage->pipeline.push_constant_size, frame.push_constant_data);
	}
}

RenderPass& RenderPass::zero(const Resource& resource) {
	if (resource.tex) {
		LUMEN_ERROR("Unimplemented: Image zeroing");
	}
	frame.resource_zeros.push_back(resource);
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
	frame.resource_copies.push_back({src, dst});
	return *this;
}

RenderPass& RenderPass::blas_build(util::Slice<vk::BVH> blases, util::Slice<vk::BlasInput> blas_inputs,
								   VkBuildAccelerationStructureFlagsKHR flags, util::Slice<vk::Buffer*> source_buffers,
								   vk::Buffer** scratch_buffer_ref) {
	LUMEN_ASSERT(!frame.blas_build_data.is_valid(), "Only one BLAS build per pass is supported");
	LUMEN_ASSERT(blases.size == blas_inputs.size, "BLASes and inputs must have the same size");
	// TODO: Need to assert that scratch_buffer_ref == nullptr in certain cases
	frame.blas_build_data.blases = blases;
	frame.blas_build_data.blas_inputs = blas_inputs;
	frame.blas_build_data.flags = flags;
	frame.blas_build_data.source_buffers = source_buffers;
	frame.blas_build_data.scratch_buffer_ref = scratch_buffer_ref;
	return *this;
}

RenderPass& RenderPass::tlas_build(vk::BVH& tlas, vk::Buffer* instances_buf, u32 instance_count,
								   VkBuildAccelerationStructureFlagsKHR flags, vk::Buffer** scratch_buffer_ref,
								   bool build_tlas_after_blas, bool update_tlas) {
	LUMEN_ASSERT(!frame.tlas_build_data.is_valid(), "Only one TLAS build per pass is supported");

	frame.tlas_build_data.tlas = &tlas;
	frame.tlas_build_data.instance_count = instance_count;
	frame.tlas_build_data.flags = flags;
	frame.tlas_build_data.instances_buf = instances_buf;
	frame.tlas_build_data.scratch_buffer_ref = scratch_buffer_ref;
	frame.tlas_build_data.build_tlas_after_blas = build_tlas_after_blas;
	frame.tlas_build_data.update_tlas = update_tlas;
	return *this;
}

static void update_rt_descriptors(RenderPass* pass) {
	VkAccelerationStructureKHR accels[vk::MAX_AS_BINDING_COUNT];
	u32 num_accels = 0;
	for (u32 i = 0; i < pass->pipeline_storage->as_bindings.size; i++) {
		if (!pass->pipeline_storage->as_bindings[i].accel) {
			LUMEN_INFO("Using null TLAS inside %s", pass->name.data);
		}
		accels[i] = pass->pipeline_storage->as_bindings[i].accel;
		++num_accels;
	}
	pass->pipeline_storage->pipeline.tlas_info = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
	pass->pipeline_storage->pipeline.tlas_info.accelerationStructureCount = num_accels;
	pass->pipeline_storage->pipeline.tlas_info.pAccelerationStructures = accels;
	VkWriteDescriptorSet descriptor_write = vk::write_descriptor_set(
		pass->pipeline_storage->pipeline.tlas_descriptor_set, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 0,
		&pass->pipeline_storage->pipeline.tlas_info, num_accels);
	vkUpdateDescriptorSets(vk::context().device, 1, &descriptor_write, 0, nullptr);
	pass->pipeline_storage->update_as_descriptor = false;
}

static util::Slice<const vk::Shader> build_shaders_slice(RenderPass* pass) {
	return {pass->build->shaders.data, pass->build->shaders.size};
}

static void create_gfx_pipeline_proc(RenderPass* pass) {
	pass->pipeline_storage->pipeline.create_gfx_pipeline(build_shaders_slice(pass), pass->build->settings, *pass->gfx,
														 pass->frame.descriptor_counts.to_slice());
}

static void create_rt_pipeline_proc(RenderPass* pass) {
	pass->pipeline_storage->pipeline.create_rt_pipeline(build_shaders_slice(pass), pass->build->settings,
														pass->frame.descriptor_counts.to_slice(),
														u32(pass->pipeline_storage->as_bindings.size));
	update_rt_descriptors(pass);
}

static void create_compute_pipeline_proc(RenderPass* pass) {
	pass->pipeline_storage->pipeline.create_compute_pipeline(pass->build->shaders[0], pass->build->settings,
															 pass->frame.descriptor_counts.to_slice());
}

void RenderPass::finalize() {
	// Create pipelines/push descriptor templates
	bool rebuild_tlas_descriptors = is_pipeline_cached && pipeline_storage->update_as_descriptor;
	if (!is_pipeline_cached) {
		void (*procedure)(RenderPass*) = nullptr;
		switch (type) {
			case vk::PassType::Graphics:
				procedure = create_gfx_pipeline_proc;
				break;
			case vk::PassType::RT:
				procedure = create_rt_pipeline_proc;
				break;
			case vk::PassType::Compute:
				procedure = create_compute_pipeline_proc;
				break;
			default:
				break;
		}
		if (procedure) {
			if (_multithreaded_pipeline_compilation) {
				_pipeline_tasks.push_back({procedure, pass_idx});
			} else {
				procedure(this);
			}
		}
	} else if (rebuild_tlas_descriptors) {
		update_rt_descriptors(this);
	}
}

void RenderPass::write_impl(const vk::Buffer* buffer, VkAccessFlags access_flags, BufferSyncFlags flags,
							VkPipelineStageFlags stage) {
	if (!stage) {
		stage = pipeline_stage_from_pass_type(type, access_flags);
	}
	const bool event_eligible = register_dependencies(buffer, access_flags, flags, stage);
	_buffer_resource_map.insert(buffer->handle, {pass_idx, access_flags, stage, _generation, event_eligible});
}

void RenderPass::write_impl(vk::Texture* tex, VkAccessFlags access_flags, VkImageLayout target_layout) {
	if (target_layout == VK_IMAGE_LAYOUT_UNDEFINED) {
		target_layout = vk::texture_to_image_layout(tex, access_flags);
	}
	const VkPipelineStageFlags stage = pipeline_stage_from_pass_type(type, access_flags);
	const bool event_eligible = register_dependencies(tex, access_flags, target_layout);
	_img_resource_map.insert(tex->handle, {.pass_idx = pass_idx,
										   .access_flags = access_flags,
										   .layout = target_layout,
										   .image_aspect = tex->aspect_flags,
										   .stage = stage,
										   .generation = _generation,
										   .event_eligible = event_eligible});
}

void RenderPass::read_impl(const vk::Buffer* buffer, VkAccessFlags access_flags, BufferSyncFlags flags,
						   VkPipelineStageFlags stage) {
	if (!stage) {
		stage = pipeline_stage_from_pass_type(type, access_flags);
	}
	const bool event_eligible = register_dependencies(buffer, access_flags, flags, stage);
	_buffer_resource_map.insert(buffer->handle, {pass_idx, access_flags, stage, _generation, event_eligible});
}

void RenderPass::read_impl(vk::Texture* tex) {
	constexpr VkAccessFlags access_flags = VK_ACCESS_SHADER_READ_BIT;
	VkImageLayout target_layout = vk::texture_to_image_layout(tex, access_flags);
	VkPipelineStageFlags stage = pipeline_stage_from_pass_type(type, access_flags);
	const bool event_eligible = register_dependencies(tex, access_flags, target_layout);
	_img_resource_map.insert(tex->handle, {.pass_idx = pass_idx,
										   .access_flags = access_flags,
										   .layout = target_layout,
										   .image_aspect = tex->aspect_flags,
										   .stage = stage,
										   .generation = _generation,
										   .event_eligible = event_eligible});
}

void RenderPass::post_execution_barrier(vk::Buffer* buffer, VkAccessFlags access_flags) {
	VkAccessFlags src_access_flags = _buffer_resource_map.find(buffer->handle)->value.access_flags;
	frame.post_execution_buffer_barriers.push_back({buffer->handle, src_access_flags, access_flags});
}

void RenderPass::run(VkCommandBuffer cmd) {
	vk::begin_region(vk::context().device, cmd, name.data, lm::vec4(1.0f, 0.78f, 0.05f, 1.0f));
	GPUQueryManager::begin(cmd, name);
	const bool use_events = _settings.use_events;

	// Barriers required before zeroing resources
	lm::SmallArray<VkBufferMemoryBarrier2, MAX_BUFFER_BARRIERS + MAX_RESOURCES_ZEROS> pre_zero_buffer_barriers;
	lm::SmallArray<VkBufferMemoryBarrier2, MAX_BUFFER_BARRIERS + MAX_RESOURCES_ZEROS> pre_execution_buffer_barriers;
	for (const auto& entry : wait_signals_buffer) {
		VkBuffer buffer = entry.key;
		const BufferSyncDescriptor& v = entry.value;
		VkBufferMemoryBarrier2 buffer_barrier = buffer_barrier_from_sync(entry.key, v);
		VkDependencyInfo dependency_info = vk::dependency_info(1, &buffer_barrier);
		if (use_events && v.event_eligible) {
			VkEvent event = sync_list_find(_passes[v.opposing_pass_idx].set_signals_buffer, buffer).event;
			LUMEN_ASSERT(event, "Event can't be null");
			vkCmdWaitEvents2(cmd, 1, &event, &dependency_info);
			vkCmdResetEvent2(cmd, event, buffer_barrier.dstStageMask);
		} else {
			lm::SmallArray<VkBufferMemoryBarrier2, MAX_BUFFER_BARRIERS + MAX_RESOURCES_ZEROS>& destination =
				frame.resource_zeros.empty() ? pre_execution_buffer_barriers : pre_zero_buffer_barriers;
			destination.push_back(buffer_barrier);
		}
	}
	for (BufferBarrier& barrier : frame.prefill_buffer_barriers) {
		VkPipelineStageFlags curr_stage =
			barrier.src_stage ? barrier.src_stage : pipeline_stage_from_pass_type(type, barrier.src_access_flags);
		VkPipelineStageFlags dst_stage =
			barrier.dst_stage ? barrier.dst_stage : pipeline_stage_from_pass_type(type, barrier.dst_access_flags);
		pre_zero_buffer_barriers.push_back(vk::buffer_barrier2(barrier.buffer, barrier.src_access_flags,
															   barrier.dst_access_flags, curr_stage, dst_stage));
	}
	if (!pre_zero_buffer_barriers.empty()) {
		VkDependencyInfo dependency_info =
			vk::dependency_info((u32)pre_zero_buffer_barriers.size, pre_zero_buffer_barriers.data);
		vkCmdPipelineBarrier2(cmd, &dependency_info);
	}

	// Zero out resources
	for (const Resource& resource : frame.resource_zeros) {
		if (resource.buf) {
			vkCmdFillBuffer(cmd, resource.buf->handle, 0, resource.buf->size, 0);
		}
	}

	// Barriers required after zeroing and before pass execution
	for (BufferBarrier& barrier : frame.carryover_buffer_barriers) {
		VkPipelineStageFlags curr_stage =
			barrier.src_stage ? barrier.src_stage : pipeline_stage_from_pass_type(type, barrier.src_access_flags);
		VkPipelineStageFlags dst_stage =
			barrier.dst_stage ? barrier.dst_stage : pipeline_stage_from_pass_type(type, barrier.dst_access_flags);
		pre_execution_buffer_barriers.push_back(vk::buffer_barrier2(barrier.buffer, barrier.src_access_flags,
																	barrier.dst_access_flags, curr_stage, dst_stage));
	}

	// Wait: Images
	lm::SmallArray<VkImageMemoryBarrier2, MAX_IMG_BARRIERS * 2> pre_execution_image_barriers;
	for (const auto& entry : wait_signals_img) {
		VkImage image = entry.key;
		const ImageSyncDescriptor& v = entry.value;
		VkImageMemoryBarrier2 img_barrier = image_barrier_from_sync(entry.key, v);
		VkDependencyInfo dependency_info = vk::dependency_info(1, &img_barrier);
		if (use_events && v.event_eligible) {
			VkEvent event = sync_list_find(_passes[v.opposing_pass_idx].set_signals_img, image).event;
			LUMEN_ASSERT(event, "Event can't be null");
			vkCmdWaitEvents2(cmd, 1, &event, &dependency_info);
			vkCmdResetEvent2(cmd, event, img_barrier.dstStageMask);
		} else {
			pre_execution_image_barriers.push_back(img_barrier);
		}
	}

	// Cross-frame image dependencies
	for (const ImageBarrier& barrier : frame.carryover_image_barriers) {
		pre_execution_image_barriers.push_back(
			vk::image_barrier2(barrier.image, barrier.src_access_flags, barrier.dst_access_flags, barrier.old_layout,
							   barrier.new_layout, barrier.image_aspect, barrier.src_stage, barrier.dst_stage));
	}

	if (!pre_execution_buffer_barriers.empty() || !pre_execution_image_barriers.empty()) {
		VkDependencyInfo dependency_info = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
		dependency_info.bufferMemoryBarrierCount = (u32)pre_execution_buffer_barriers.size;
		dependency_info.pBufferMemoryBarriers = pre_execution_buffer_barriers.data;
		dependency_info.imageMemoryBarrierCount = (u32)pre_execution_image_barriers.size;
		dependency_info.pImageMemoryBarriers = pre_execution_image_barriers.data;
		vkCmdPipelineBarrier2(cmd, &dependency_info);
	}

	// Transition layouts inside the pass
	for (auto& [tex, old_layout, dst_layout] : frame.layout_transitions) {
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
						   pipeline_storage->pipeline.push_constant_size, frame.push_constant_data);
	}
	// Run
	if (!frame.disable_execution) {
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

				if (pass_func) {
					pass_func(cmd, *this);
				} else {
					lm::SmallArray<VkStridedDeviceAddressRegionKHR, vk::NUM_SBT_GROUPS> regions =
						pipeline_storage->pipeline.get_rt_regions();
					vkCmdTraceRaysKHR(cmd, &regions[0], &regions[1], &regions[2], &regions[3], dims.x, dims.y, dims.z);
				}
				break;
			}
			case vk::PassType::Compute: {
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_storage->pipeline.handle);

				if (pass_func) {
					pass_func(cmd, *this);
				} else {
					vkCmdDispatch(cmd, dims.x, dims.y, dims.z);
				}
				break;
			}
			case vk::PassType::Graphics: {
				const lm::SmallArray<vk::Texture*, vk::MAX_COLOR_ATTACHMENTS>& color_outputs = gfx->color_outputs;
				vk::Texture* depth_output = gfx->depth_output;
				bind_graphics_state(cmd);
				lm::SmallArray<VkRenderingAttachmentInfo, MAX_RENDERING_ATTACHMENTS> rendering_attachments;
				for (u64 i = 0; i < color_outputs.size; ++i) {
					vk::Texture* color_output = color_outputs[i];
					vk::texture_transition(color_output, cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
					const VkAttachmentLoadOp load_op =
						i < gfx->color_load_ops.size ? gfx->color_load_ops[i] : VK_ATTACHMENT_LOAD_OP_CLEAR;
					rendering_attachments.push_back(
						vk::rendering_attachment_info(color_output->view, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
													  load_op, VK_ATTACHMENT_STORE_OP_STORE, gfx->clear_color));
				}
				VkRenderingAttachmentInfo depth_stencil_attachment;
				if (depth_output) {
					vk::texture_transition(depth_output, cmd, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
					depth_stencil_attachment = vk::rendering_attachment_info(
						depth_output->view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, gfx->depth_load_op,
						VK_ATTACHMENT_STORE_OP_STORE, gfx->clear_depth_stencil);
				}

				// Render
				{
					VkRenderingInfo render_info{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
												.renderArea = {{0, 0}, {gfx->width, gfx->height}},
												.layerCount = 1,
												.colorAttachmentCount = (u32)color_outputs.size,
												.pColorAttachments = rendering_attachments.data,
												.pDepthAttachment = depth_output ? &depth_stencil_attachment : nullptr};
					vkCmdBeginRendering(cmd, &render_info);
					pass_func(cmd, *this);
					vkCmdEndRendering(cmd);
				}

				break;
			}
			default:
				break;
		}
	}

	// Post execution buffer barriers
	{
		lm::SmallArray<VkBufferMemoryBarrier2, MAX_RESOURCES_COPIES + MAX_BUFFER_BARRIERS>
			post_execution_buffer_memory_barriers;
		for (BufferBarrier& barrier : frame.post_execution_buffer_barriers) {
			VkPipelineStageFlags curr_stage =
				barrier.src_stage ? barrier.src_stage : pipeline_stage_from_pass_type(type, barrier.src_access_flags);
			VkPipelineStageFlags dst_stage =
				barrier.dst_stage ? barrier.dst_stage : pipeline_stage_from_pass_type(type, barrier.dst_access_flags);
			post_execution_buffer_memory_barriers.push_back(vk::buffer_barrier2(
				barrier.buffer, barrier.src_access_flags, barrier.dst_access_flags, curr_stage, dst_stage));
		}
		if (frame.blas_build_data.is_valid()) {
			for (vk::Buffer* source_buffer : frame.blas_build_data.source_buffers) {
				for (const Resource& zeroed_resource : frame.resource_zeros) {
					if (zeroed_resource.buf && zeroed_resource.buf->handle == source_buffer->handle) {
						// If a BLAS input was zeroed, make the fill visible to the AS build.
						post_execution_buffer_memory_barriers.push_back(vk::buffer_barrier2(
							source_buffer->handle, VK_ACCESS_TRANSFER_WRITE_BIT,
							VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
							VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR));
						break;
					}
				}
			}
		}

		if (!post_execution_buffer_memory_barriers.empty()) {
			VkDependencyInfo dependency_info = vk::dependency_info((u32)post_execution_buffer_memory_barriers.size,
																   post_execution_buffer_memory_barriers.data);
			vkCmdPipelineBarrier2(cmd, &dependency_info);
		}
	}

	for (const auto& [src, dst] : frame.resource_copies) {
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
				VkImageLayout old_src_layout = src.tex->layout;
				VkImageLayout old_dst_layout = dst.tex->layout;
				vk::texture_transition(src.tex, cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
				vk::texture_transition(dst.tex, cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
				region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.srcSubresource.layerCount = 1;
				region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.dstSubresource.layerCount = 1;
				region.extent = src.tex->extent;
				vkCmdCopyImage(cmd, src.tex->handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.tex->handle,
							   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
				vk::texture_transition(src.tex, cmd, old_src_layout);
				vk::texture_transition(dst.tex, cmd, old_dst_layout);
			}
		} else {  // buffer
			if (dst.buf) {
				VkBufferCopy copy_region = {.size = src.buf->size};
				vkCmdCopyBuffer(cmd, src.buf->handle, dst.buf->handle, 1, &copy_region);
			}
		}
	}

	if (frame.blas_build_data.is_valid()) {
		GPUQueryManager::begin(cmd, "BLAS Build");
		ScratchArena scratch = _arena_per_frame;
		vk::blas_build(scratch, frame.blas_build_data.blases, frame.blas_build_data.blas_inputs,
					   frame.blas_build_data.flags, cmd, frame.blas_build_data.scratch_buffer_ref);
		GPUQueryManager::end(cmd);

		if (frame.tlas_build_data.build_tlas_after_blas) {
			LUMEN_ASSERT(frame.tlas_build_data.tlas, "TLAS reference is null");
			void* instance_data = vk::buffer_map(frame.tlas_build_data.instances_buf);
			for (u64 i = 0; i < frame.blas_build_data.blases.size; i++) {
				VkAccelerationStructureInstanceKHR* instance = (VkAccelerationStructureInstanceKHR*)instance_data + i;
				instance->accelerationStructureReference = frame.blas_build_data.blases[i].device_address();
			}
			vk::buffer_unmap(frame.tlas_build_data.instances_buf);
			lm::SmallArray<VkBufferMemoryBarrier2, MAX_BUFFER_BARRIERS> buffer_memory_barriers;
			for (const vk::BVH& blas : frame.blas_build_data.blases) {
				buffer_memory_barriers.push_back(
					vk::buffer_barrier2(blas.buffer->handle, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
										VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
										VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
										VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR));
			}
			VkDependencyInfo dependency_info =
				vk::dependency_info((u32)buffer_memory_barriers.size, buffer_memory_barriers.data);
			vkCmdPipelineBarrier2(cmd, &dependency_info);
		}
	}

	if (frame.tlas_build_data.is_valid()) {
		GPUQueryManager::begin(cmd, "TLAS Build");
		vk::tlas_build(*frame.tlas_build_data.tlas, frame.tlas_build_data.instances_buf,
					   frame.tlas_build_data.instance_count, frame.tlas_build_data.flags, cmd,
					   frame.tlas_build_data.scratch_buffer_ref);
		GPUQueryManager::end(cmd);
	}

	// Set: Buffer
	for (auto& entry : set_signals_buffer) {
		BufferSyncDescriptor& buffer_sync = entry.value;
		VkBuffer buffer = entry.key;
		LUMEN_ASSERT(buffer_sync.event == nullptr, "VkEvent should be null in the setter");
		VkBufferMemoryBarrier2 mem_barrier = buffer_barrier_from_sync(buffer, buffer_sync);
		VkDependencyInfo dependency_info = vk::dependency_info(1, &mem_barrier);

		if (use_events && buffer_sync.event_eligible) {
			buffer_sync.event = vk::event_pool::get_event(cmd);
			vkCmdSetEvent2(cmd, buffer_sync.event, &dependency_info);
		}
	}

	// Set: Images
	for (auto& entry : set_signals_img) {
		ImageSyncDescriptor& img_sync = entry.value;
		VkImage img = entry.key;
		LUMEN_ASSERT(img_sync.event == nullptr, "VkEvent should be null in the setter");
		VkImageMemoryBarrier2 mem_barrier = image_barrier_from_sync(img, img_sync);

		VkDependencyInfo dependency_info = vk::dependency_info(1, &mem_barrier);
		if (use_events && img_sync.event_eligible) {
			img_sync.event = vk::event_pool::get_event(cmd);
			vkCmdSetEvent2(cmd, img_sync.event, &dependency_info);
		}
	}

	vk::end_region(vk::context().device, cmd);
	GPUQueryManager::end(cmd);
}

void rg::init() {
	if (!_arena_rendergraph) {
		_arena_rendergraph = lm::arena_create(CSTR("Render Graph Arena (Persistent)"), GB(1), MB(32));
		_arena_per_frame = lm::arena_create(CSTR("Render Graph Arena (Per frame)"), MB(16), MB(1));
	}
	// Arrays
	_passes = lm::fixed_array_create<RenderPass>(_arena_rendergraph, MAX_PASSES_PER_FRAME);
	_pipeline_tasks = lm::fixed_array_create<PipelineTask>(_arena_rendergraph, MAX_PIPELINE_TASKS);
	// Hashmaps
	// Sizes are reasonable upper bounds but not hard bounds unlike arrays
	_pipeline_cache = lm::hash_map_create<u64, PipelineStorage>(_arena_rendergraph, MAX_PASSES_PER_FRAME);
	_buffer_resource_map =
		lm::hash_map_create<VkBuffer, BufferResourceState>(_arena_rendergraph, INITIAL_BUFFER_RESOURCE_COUNT);
	_img_resource_map =
		lm::hash_map_create<VkImage, ImageResourceState>(_arena_rendergraph, INITIAL_IMG_RESOURCE_COUNT);
	_registered_buffer_pointers =
		lm::hash_map_create<lm::String, vk::Buffer*>(_arena_rendergraph, INITIAL_REGISTERED_BUFFER_POINTER_COUNT);
	_shader_cache = lm::hash_map_create<lm::String, vk::Shader>(_arena_rendergraph, INITIAL_SHADER_CACHE_COUNT);
}

static u64 shader_render_pass_hash(const lm::pair<vk::Shader*, RenderPass*>& entry) {
	return lm::hash(entry.first->name_with_macros);
}

static bool shader_render_pass_eq(const lm::pair<vk::Shader*, RenderPass*>& a,
								  const lm::pair<vk::Shader*, RenderPass*>& b) {
	return a.first->name_with_macros == b.first->name_with_macros;
}

void rg::run(VkCommandBuffer cmd) {
	// Compile shaders and process resources
	const bool recording_or_reload = _dirty_pass_encountered || _reload_shaders;
	if (recording_or_reload) {
		lm::ScratchArena scratch = _arena_per_frame;
		auto unique_shaders_set =
			lm::hash_set_create<lm::pair<vk::Shader*, RenderPass*>, shader_render_pass_hash, shader_render_pass_eq>(
				scratch.arena, MAX_SHADER_COMPILATIONS_PER_FRAME);
		auto existing_shaders_map =
			lm::hash_map_create<RenderPass*, lm::SmallArray<vk::Shader*, vk::MAX_SHADERS_PER_PASS>>(
				scratch.arena, MAX_SHADER_COMPILATIONS_PER_FRAME);
		auto unique_shaders_map =
			lm::hash_map_create<RenderPass*, lm::SmallArray<vk::Shader*, vk::MAX_SHADERS_PER_PASS>>(
				scratch.arena, MAX_SHADER_COMPILATIONS_PER_FRAME);

		for (u64 i = 0; i < _passes.size; i++) {
			if (_passes[i].is_pipeline_cached) {
				continue;
			}
			for (vk::Shader& shader : _passes[i].build->shaders) {
				bool entry_created = false;
				unique_shaders_set.get_or_create({&shader, &_passes[i]}, &entry_created);
				if (!entry_created) {
					auto* entry = existing_shaders_map.get_or_create(&_passes[i]);
					entry->value.push_back(&shader);
				}
			}
		}

		lm::SmallArray<BuildShadersTask, MAX_PIPELINE_TASKS * 4> shader_tasks;
		tp::JobCounter shader_counter;
		for (const auto& entry : unique_shaders_set) {
			vk::Shader* shader = entry.key.first;
			RenderPass* rp = entry.key.second;
			auto* unique_shader_entry = unique_shaders_map.get_or_create(rp);
			unique_shader_entry->value.push_back(shader);
		}
		// Compile and process resources for unique shaders
		for (const auto& [hash, pass, shaders] : unique_shaders_map) {
			BuildShadersTask& task = shader_tasks.push();
			task = {.pass = pass, .shaders = shaders};
			tp::submit({build_shaders_job, &task}, shader_counter);
		}
		tp::wait(shader_counter);
		shader_tasks.clear();
		// Process resources for duplicate shaders
		for (const auto& [hash, pass, shaders] : existing_shaders_map) {
			BuildShadersTask& task = shader_tasks.push();
			task = {.pass = pass, .shaders = shaders};
			tp::submit({build_shaders_job, &task}, shader_counter);
		}
		tp::wait(shader_counter);
	}

	for (u64 i = 0; i < _passes.size; i++) {
		_passes[i].finalize();
	}

	if (_pipeline_tasks.size) {
		lm::SmallArray<PipelineRunTask, MAX_PIPELINE_TASKS> tasks;
		tp::JobCounter pipeline_counter;
		for (PipelineTask& pipeline_task : _pipeline_tasks) {
			if (pipeline_task.procedure) {
				PipelineRunTask& task = tasks.push();
				task = {.task = pipeline_task, .pass = &_passes[pipeline_task.pass_idx]};
				tp::submit({run_pipeline_job, &task}, pipeline_counter);
			}
		}
		tp::wait(pipeline_counter);
		// for (auto& [_, idx] : pipeline_tasks) {
		// 	passes[idx].transition_resources();
		// }
		_pipeline_tasks.clear();
	}

	for (u64 i = 0; i < _passes.size; i++) {
		_passes[i].transition_resources();
	}

	for (u64 i = 0; i < _passes.size; i++) {
		_passes[i].run(cmd);
	}

	// Transition output images to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	for (RenderPass& pass : _passes) {
		if (!pass.gfx) {
			continue;
		}
		for (vk::Texture* output : pass.gfx->color_outputs) {
			if (output->allocation != VK_NULL_HANDLE || output->layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) continue;
			vk::texture_transition(output, cmd, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

			// This transition is not owned by any pass, so it must always take the
			// cross frame carryover path in register_dependencies
			_img_resource_map.insert(output->handle, {.pass_idx = INVALID_PASS_IDX,
													  .access_flags = 0,
													  .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
													  .image_aspect = output->aspect_flags,
													  .stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
													  .generation = _generation - 1,
													  .event_eligible = false});
		}
	}
}

void rg::reset_frame() {
	vk::event_pool::reset_events();
	// Entries in the resource maps stamped with older generations become cross-frame history
	_generation++;
	for (RenderPass& pass : _passes) {
		////////////////////////////
		// --- Reset pass resources ---
		pass.frame.reset();
		pass.set_signals_buffer.clear();
		pass.wait_signals_buffer.clear();
		pass.set_signals_img.clear();
		pass.wait_signals_img.clear();
	}
	_passes.clear();
	_pipeline_tasks.clear();
	_arena_per_frame->clear();
	_reload_shaders = false;
	_dirty_pass_encountered = false;
}

void rg::submit(vk::CommandBuffer& cmd) {
	cmd.submit();
	// The reset is needed here because the next subsequent pass may reuse the old pass' memory
	reset_frame();
}

void rg::run_and_submit(vk::CommandBuffer& cmd) {
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

static u64 hash_macros(const vk::ShaderMacroArray& macros, u64 hash) {
	for (const vk::ShaderMacro& macro : macros) {
		if (!macro.visible) {
			continue;
		}
		hash = lm::fnv1a_hash((void*)macro.name.data, macro.name.size, hash);
		hash = lm::fnv1a_hash(u64(macro.has_val), hash);
		if (macro.has_val) {
			hash = lm::fnv1a_hash(u64(u32(macro.val)), hash);
		}
	}
	return hash;
}

static lm::String build_macro_string(const vk::ShaderMacroArray& macros) {
	lm::String macro_string;
	if (!macros.empty() || !_global_macro_defines.empty()) {
		macro_string = lm::str_concat(_arena_per_frame, macro_string, "(");
	}

	bool prev_nonempty = false;
	populate_macros(_arena_per_frame, macros, macro_string, prev_nonempty);
	populate_macros(_arena_per_frame, _global_macro_defines, macro_string, prev_nonempty);

	if (!macros.empty() || !_global_macro_defines.empty()) {
		macro_string = lm::str_concat(_arena_per_frame, macro_string, ")");
	}
	if (macro_string == "()") {
		macro_string = "";
	}
	return macro_string;
}

static PipelineStorage* add_pass_impl_common(const lm::String& name, const vk::ShaderMacroArray& macros,
											 const lm::SpecializationConstantArray& specialization_data, bool& cached,
											 lm::String& macro_string) {
	assert(name.is_cstr());
	PipelineStorage* pipeline_storage;

	u64 hash = lm::fnv1a_hash((void*)name.data, name.size, lm::HASH_INIT);
	hash = hash_macros(macros, hash);
	hash = hash_macros(_global_macro_defines, hash);
	for (u32 spec_data : specialization_data) {
		lm::hash_combine(hash, spec_data);
	}

	auto* entry = _pipeline_cache.find(hash);
	if (entry && entry->value.reload_counter == _reload_counter) {
		pipeline_storage = &entry->value;
		cached = true;
	} else {
		_dirty_pass_encountered = true;
		if (entry) {
			// reload shaders
			vkDeviceWaitIdle(vk::context().device);
			entry->value.pipeline.cleanup();
		}
		macro_string = build_macro_string(macros);
		lm::String name_with_macros = lm::str_concat(_arena_rendergraph, name, macro_string, /*cstr=*/true);
		auto* new_entry = _pipeline_cache.insert(hash, PipelineStorage{.pipeline = vk::Pipeline(name_with_macros)});
		new_entry->value.reload_counter = _reload_counter;
		pipeline_storage = &new_entry->value;
	}
	if (!pipeline_storage->affected_buffer_pointers.initialized()) {
		pipeline_storage->affected_buffer_pointers =
			lm::hash_map_create<lm::String, vk::BufferStatus>(_arena_rendergraph, MAX_BUFFER_BARRIERS);
	}
	return pipeline_storage;
}

RenderGraphSettings& rg::settings() { return _settings; }

const vk::ShaderMacroArray& rg::global_macro_defines() { return _global_macro_defines; }

void rg::add_global_macro(const vk::ShaderMacro& macro) { _global_macro_defines.push_back(macro); }

void rg::register_buffer_pointer(const lm::String& name, vk::Buffer* buffer) {
	_registered_buffer_pointers.insert(name, buffer);
}

bool rg::is_buffer_registered(const lm::String& name) { return _registered_buffer_pointers.find(name) != nullptr; }

void rg::request_shader_reload() {
	_reload_shaders = true;
	_shader_cache.clear();
	_reload_counter++;
}

void rg::destroy() {
	for (auto& entry : _pipeline_cache) {
		entry.value.pipeline.cleanup();
	}
}

lm::Arena* rg::arena() {
	assert(_arena_rendergraph);
	return _arena_rendergraph;
}
