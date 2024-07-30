#pragma once
#include <memory>
#include "../LumenPCH.h"
#include "CommandBuffer.h"
#include "Framework/RenderGraphTypes.h"
#include "Framework/VulkanStructs.h"
#include "Pipeline.h"
#include "Shader.h"
#include "Texture.h"
#include "EventPool.h"
#include "RenderGraphTypes.h"
#include "AccelerationStructure.h"

namespace lumen {

namespace detail {
inline void hash_combine(std::size_t& seed) {}

template <typename T, typename... Rest>
inline void hash_combine(std::size_t& seed, const T& v, Rest... rest) {
	std::hash<T> hasher;
	seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	hash_combine(seed, rest...);
}
}  // namespace detail

#define TO_STR(V) (#V)

#define REGISTER_BUFFER_WITH_ADDRESS(struct_type, struct_name, field_name, buffer_ptr, rg) \
	do {                                                                                   \
		auto key = std::string(#struct_type) + '_' + std::string(#field_name);             \
		rg->registered_buffer_pointers[key] = buffer_ptr;                                  \
	} while (0)
#define REGISTER_BUFFER(X, Y) ((X) < (Y) ? (X) : (Y))
#define REGISTER_IMAGE(X, Y) ((X) < (Y) ? (X) : (Y))


class RenderGraph;
class RenderPass;

struct PipelineStorage {
	std::unique_ptr<Pipeline> pipeline;
	std::vector<ResourceBinding> bound_resources;
	std::unordered_map<std::string, BufferStatus> affected_buffer_pointers;
	bool dirty = false;
};

class RenderGraph {
   public:
	RenderGraph();
	RenderPass& current_pass();
	RenderPass& add_rt(const std::string& name, const RTPassSettings& settings);
	RenderPass& add_gfx(const std::string& name, const GraphicsPassSettings& settings);
	RenderPass& add_compute(const std::string& name, const ComputePassSettings& settings);
	void run(VkCommandBuffer cmd);
	void reset();
	void submit(CommandBuffer& cmd);
	void run_and_submit(CommandBuffer& cmd);
	void destroy();
	void set_pipelines_dirty();
	friend RenderPass;
	bool reload_shaders = false;
	vk::EventPool event_pool;
	std::unordered_map<std::string, Buffer*> registered_buffer_pointers;
	// Shader Name + Macro String -> Shader
	std::unordered_map<std::string, Shader> shader_cache;
	RenderGraphSettings settings;
	std::mutex shader_map_mutex;
	std::vector<ShaderMacro> global_macro_defines;

   private:
	struct BufferSyncResources {
		std::vector<VkBufferMemoryBarrier2> buffer_bariers;
		std::vector<VkDependencyInfo> dependency_infos;
	};
	struct ImageSyncResources {
		std::vector<VkImageMemoryBarrier2> img_barriers;
		std::vector<VkDependencyInfo> dependency_infos;
	};

	std::vector<RenderPass> passes;

	// Pipeline Name + Macro String + Specialization Constants + Timestamp -> Pipeline
	std::unordered_map<size_t, PipelineStorage> pipeline_cache;
	std::vector<std::pair<std::function<void(RenderPass*)>, uint32_t>> pipeline_tasks;
	std::vector<std::function<void(RenderPass*)>> shader_tasks;
	// Sync related data
	std::vector<BufferSyncResources> buffer_sync_resources;
	std::vector<ImageSyncResources> img_sync_resources;
	std::unordered_map<VkBuffer, std::pair<uint32_t, VkAccessFlags>>
		buffer_resource_map;								 // Buffer handle - { Write Pass Idx, Access Type }
	std::unordered_map<VkImage, uint32_t> img_resource_map;	 // Tex2D handle - Pass Idx
	const bool multithreaded_pipeline_compilation = true;
	static const uint32_t INVALID_PASS_IDX = UINT_MAX;

	template <typename Settings>
	RenderPass& add_pass_impl(const std::string& name, const Settings& settings);

   private:
	bool dirty_pass_encountered = false;
};

class RenderPass {
   public:
	RenderPass(PassType type, const std::string& name, RenderGraph* rg, uint32_t pass_idx,
			   const GraphicsPassSettings& gfx_settings, const std::string& macro_string,
			   PipelineStorage* pipeline_storage, bool cached = false)
		: type(type),
		  name(name),
		  rg(rg),
		  pass_idx(pass_idx),
		  gfx_settings(std::make_unique<GraphicsPassSettings>(gfx_settings)),
		  macro_defines(gfx_settings.macros),
		  pipeline_storage(pipeline_storage),
		  is_pipeline_cached(cached) {
		for (auto& shader : this->gfx_settings->shaders) {
			shader.name_with_macros = shader.filename + macro_string;
		}
	}

	RenderPass(PassType type, const std::string& name, RenderGraph* rg, uint32_t pass_idx,
			   const RTPassSettings& rt_settings, const std::string& macro_string, PipelineStorage* pipeline_storage,
			   bool cached = false)
		: type(type),
		  name(name),
		  rg(rg),
		  pass_idx(pass_idx),
		  rt_settings(std::make_unique<RTPassSettings>(rt_settings)),
		  macro_defines(rt_settings.macros),
		  pipeline_storage(pipeline_storage),
		  is_pipeline_cached(cached) {
		for (auto& shader : this->rt_settings->shaders) {
			shader.name_with_macros = shader.filename + macro_string;
		}
	}

	RenderPass(PassType type, const std::string& name, RenderGraph* rg, uint32_t pass_idx,
			   const ComputePassSettings& compute_settings, const std::string& macro_string,
			   PipelineStorage* pipeline_storage, bool cached = false)
		: type(type),
		  name(name),
		  rg(rg),
		  pass_idx(pass_idx),
		  compute_settings(std::make_unique<ComputePassSettings>(compute_settings)),
		  macro_defines(compute_settings.macros),
		  pipeline_storage(pipeline_storage),
		  is_pipeline_cached(cached) {
		this->compute_settings->shader.name_with_macros = compute_settings.shader.filename + macro_string;
	}

	RenderPass& bind(const ResourceBinding& binding);
	RenderPass& bind_texture_with_sampler(Texture2D& tex, VkSampler sampler);
	RenderPass& bind(std::initializer_list<ResourceBinding> bindings);
	RenderPass& bind_texture_array(std::vector<Texture2D>& texes, bool force_update = false);
	RenderPass& bind_buffer_array(std::vector<Buffer>& buffers, bool force_update = false);
	RenderPass& bind_tlas(const vk::AccelKHR& tlas);

	RenderPass& write(Texture2D& tex);
	RenderPass& write(Buffer& buffer);
	RenderPass& read(Texture2D& tex);
	RenderPass& read(Buffer& buffer);
	RenderPass& read(ResourceBinding& resource);
	RenderPass& read(std::initializer_list<std::reference_wrapper<Buffer>> buffers);
	RenderPass& read(std::initializer_list<std::reference_wrapper<Texture2D>> texes);
	RenderPass& write(ResourceBinding& resource);
	RenderPass& write(std::initializer_list<std::reference_wrapper<Buffer>> buffers);
	RenderPass& write(std::initializer_list<std::reference_wrapper<Texture2D>> texes);

	RenderPass& skip_execution(bool condition = true);
	template <typename T>
	RenderPass& push_constants(T* data);
	RenderPass& zero(const Resource& resource);
	RenderPass& zero(std::initializer_list<std::reference_wrapper<Buffer>> buffers);
	RenderPass& zero(std::initializer_list<std::reference_wrapper<Texture2D>> textures);
	RenderPass& zero(const Resource& resource, bool cond);
	RenderPass& copy(const Resource& src, const Resource& dst);
	void finalize();
	friend RenderGraph;

	RenderGraph* rg;
	std::vector<ShaderMacro> macro_defines;
	std::unique_ptr<GraphicsPassSettings> gfx_settings = nullptr;
	std::unique_ptr<RTPassSettings> rt_settings = nullptr;
	std::unique_ptr<ComputePassSettings> compute_settings = nullptr;
	PassType type;
	uint32_t pass_idx;
	PipelineStorage* pipeline_storage = nullptr;

   private:
	// When the automatic inference isn't used
	std::vector<Buffer*> explicit_buffer_writes;
	std::vector<Buffer*> explicit_buffer_reads;
	std::vector<Texture2D*> explicit_tex_writes;
	std::vector<Texture2D*> explicit_tex_reads;

	void write_impl(Buffer& buffer, VkAccessFlags access_flags);
	void write_impl(Texture2D& tex, VkAccessFlags access_flags = VK_ACCESS_SHADER_WRITE_BIT);
	void read_impl(Buffer& buffer);
	void read_impl(Buffer& buffer, VkAccessFlags access_flags);
	void read_impl(Texture2D& tex);
	void post_execution_barrier(Buffer& buffer, VkAccessFlags access_flags);

	void run(VkCommandBuffer cmd);
	void register_dependencies(Buffer& buffer, VkAccessFlags dst_access_flags);
	void register_dependencies(Texture2D& tex, VkImageLayout target_layout);
	void transition_resources();

	std::string name;
	int next_binding_idx = 0;
	std::vector<uint32_t> descriptor_counts;
	void* push_constant_data = nullptr;
	bool is_pipeline_cached = false;
	bool record_override = true;
	/*
		Note:
		The assumption is that a SyncDescriptor is unique to a pass (either via
		Buffer or Image). Which is reasonable because each pass is comprised of a
		single shader dispatch
	*/
	std::unordered_map<VkBuffer, BufferSyncDescriptor> set_signals_buffer;
	std::unordered_map<VkBuffer, BufferSyncDescriptor> wait_signals_buffer;

	std::unordered_map<VkImage, ImageSyncDescriptor> set_signals_img;
	std::unordered_map<VkImage, ImageSyncDescriptor> wait_signals_img;

	vk::DescriptorInfo descriptor_infos[32] = {};

	std::vector<std::tuple<Texture2D*, VkImageLayout, VkImageLayout>> layout_transitions;

	struct BufferBarrier {
		VkBuffer buffer;
		VkAccessFlags src_access_flags = VK_ACCESS_SHADER_WRITE_BIT;
		VkAccessFlags dst_access_flags = VK_ACCESS_SHADER_READ_BIT;
	};
	std::vector<Resource> resource_zeros;
	std::vector<std::pair<Resource, Resource>> resource_copies;
	std::vector<BufferBarrier> buffer_barriers;
	std::vector<BufferBarrier> post_execution_buffer_barriers;
	bool disable_execution = false;
};

template <typename Settings>
inline RenderPass& RenderGraph::add_pass_impl(const std::string& name, const Settings& settings) {
	PipelineStorage* pipeline_storage;
	bool cached = false;

	std::string name_with_macros = name;
	std::string macro_string;

	std::vector<ShaderMacro> combined_macros;
	if (!settings.macros.empty() || !global_macro_defines.empty()) {
		macro_string += '(';
	}

	auto populate_macros = [](const std::vector<ShaderMacro>& macros, std::string& macro_string, bool& prev_nonempty) {
		for (size_t i = 0; i < macros.size(); i++) {
			if (!macros[i].visible) {
				continue;
			}
			if (!macros[i].name.empty()) {
				if (prev_nonempty) {
					macro_string += ",";
				}
				macro_string += macros[i].name;
				prev_nonempty = true;
			}
			if (macros[i].has_val) {
				macro_string += "=" + std::to_string(macros[i].val);
			}
		}
	};
	bool prev_nonempty = false;
	populate_macros(settings.macros, macro_string, prev_nonempty);
	populate_macros(global_macro_defines, macro_string, prev_nonempty);

	if (!settings.macros.empty() || !global_macro_defines.empty()) {
		macro_string += ')';
	}
	if (macro_string == "()") {
		macro_string.clear();
	}
	name_with_macros += macro_string;

	size_t hash = 0;
	detail::hash_combine(hash, name_with_macros);
	for (uint32_t spec_data : settings.specialization_data) {
		detail::hash_combine(hash, spec_data);
	}

	if (auto cache_it = pipeline_cache.find(hash); cache_it != pipeline_cache.end() && !reload_shaders) {
		pipeline_storage = &cache_it->second;
		cached = true;
	} else {
		dirty_pass_encountered = true;
		if (cache_it != pipeline_cache.end()) {
			vkDeviceWaitIdle(vk::context().device);
			cache_it->second.pipeline->cleanup();
		}
		pipeline_cache[hash] = PipelineStorage(std::make_unique<Pipeline>(name_with_macros));
		pipeline_storage = &pipeline_cache[hash];
	}
	PassType type;
	if constexpr (std::is_same_v<ComputePassSettings, Settings>) {
		type = PassType::Compute;
	} else if constexpr (std::is_same_v<GraphicsPassSettings, Settings>) {
		type = PassType::Graphics;
	} else {
		type = PassType::RT;
	}
	return passes.emplace_back(type, name_with_macros, this, passes.size(), settings, macro_string, pipeline_storage,
							   cached);
}

template <typename T>
inline RenderPass& RenderPass::push_constants(T* data) {
	if (!push_constant_data) {
		push_constant_data = malloc(sizeof(T));
	}
	memcpy(push_constant_data, data, sizeof(T));
	return *this;
}

}  // namespace lumen
