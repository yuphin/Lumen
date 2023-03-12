#pragma once
#include "LumenPCH.h"
#include "Framework/CommandBuffer.h"
#include "Framework/Pipeline.h"
#include "Framework/Shader.h"
#include "Framework/Texture.h"
#include "Framework/EventPool.h"
#include "Framework/RenderGraphTypes.h"

#define TO_STR(V) (#V)

#define REGISTER_BUFFER_WITH_ADDRESS(struct_type, struct_name, field_name, buffer_ptr, rg) \
	do {                                                                                   \
		auto key = std::string(#struct_type) + '_' + std::string(#field_name);             \
		rg->registered_buffer_pointers[key] = buffer_ptr;                                  \
	} while (0)
#define REGISTER_BUFFER(X, Y) ((X) < (Y) ? (X) : (Y))
#define REGISTER_IMAGE(X, Y) ((X) < (Y) ? (X) : (Y))

struct AccelKHR {
	VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
	Buffer buffer;
};

class RenderGraph;
class RenderPass;

class RenderGraph {
public:
	RenderGraph(VulkanContext* ctx) : ctx(ctx) { pipeline_tasks.reserve(32); }
	RenderPass& current_pass() { return passes.back(); }

	RenderPass& add_rt(const std::string& name, const RTPassSettings& settings);
	RenderPass& add_gfx(const std::string& name, const GraphicsPassSettings& settings);
	RenderPass& add_compute(const std::string& name, const ComputePassSettings& settings);
	void run(VkCommandBuffer cmd);
	void reset(VkCommandBuffer cmd);
	void submit(CommandBuffer& cmd);
	void run_and_submit(CommandBuffer& cmd);
	void destroy();
	friend RenderPass;
	bool recording = true;
	bool reload_shaders = false;
	EventPool event_pool;
	std::unordered_map<std::string, Buffer*> registered_buffer_pointers;
	std::unordered_map<std::string, Shader> shader_cache;
	RenderGraphSettings settings;
	std::mutex shader_map_mutex;

private:
	struct BufferSyncResources {
		std::vector<VkBufferMemoryBarrier2> buffer_bariers;
		std::vector<VkDependencyInfo> dependency_infos;
	};
	struct ImageSyncResources {
		std::vector<VkImageMemoryBarrier2> img_barriers;
		std::vector<VkDependencyInfo> dependency_infos;
	};

	struct PipelineStorage {
		std::unique_ptr<Pipeline> pipeline;
		uint32_t offset_idx;
		std::vector<uint32_t> pass_idxs;
	};
	VulkanContext* ctx = nullptr;
	std::vector<RenderPass> passes;
	std::unordered_map<std::string, PipelineStorage> pipeline_cache;
	std::vector<std::pair<std::function<void(RenderPass*)>, uint32_t>> pipeline_tasks;
	std::vector<std::function<void(RenderPass*)>> shader_tasks;
	// Sync related data
	std::vector<BufferSyncResources> buffer_sync_resources;
	std::vector<ImageSyncResources> img_sync_resources;
	std::unordered_map<VkBuffer, std::pair<uint32_t, VkAccessFlags>>
		buffer_resource_map;								 // Buffer handle - { Write Pass Idx, Access Type }
	std::unordered_map<VkImage, uint32_t> img_resource_map;	 // Tex2D handle - Pass Idx
	uint32_t beginning_pass_idx = 0;
	uint32_t ending_pass_idx = 0;
	const bool multithreaded_pipeline_compilation = true;

	template <typename Settings>
	RenderPass& add_pass_impl(const std::string& name, const Settings& settings);
};

class RenderPass {
   public:
	RenderPass(PassType type, Pipeline* pipeline, const std::string& name, RenderGraph* rg, uint32_t pass_idx,
			   const GraphicsPassSettings& gfx_settings, bool cached = false)
		: type(type),
		  pipeline(pipeline),
		  name(name),
		  rg(rg),
		  pass_idx(pass_idx),
		  gfx_settings(std::make_unique<GraphicsPassSettings>(gfx_settings)),
		  is_pipeline_cached(cached) {}

	RenderPass(PassType type, Pipeline* pipeline, const std::string& name, RenderGraph* rg, uint32_t pass_idx,
			   const RTPassSettings& rt_settings, bool cached = false)
		: type(type),
		  pipeline(pipeline),
		  name(name),
		  rg(rg),
		  pass_idx(pass_idx),
		  rt_settings(std::make_unique<RTPassSettings>(rt_settings)),
		  is_pipeline_cached(cached) {}

	RenderPass(PassType type, Pipeline* pipeline, const std::string& name, RenderGraph* rg, uint32_t pass_idx,
			   const ComputePassSettings& compute_settings, bool cached = false)
		: type(type),
		  pipeline(pipeline),
		  name(name),
		  rg(rg),
		  pass_idx(pass_idx),
		  compute_settings(std::make_unique<ComputePassSettings>(compute_settings)),
		  is_pipeline_cached(cached) {}

	RenderPass& bind(const ResourceBinding& binding);
	RenderPass& bind(Texture2D& tex, VkSampler sampler);
	RenderPass& bind(std::initializer_list<ResourceBinding> bindings);
	RenderPass& bind_texture_array(std::vector<Texture2D>& texes);
	RenderPass& bind_buffer_array(std::vector<Buffer>& buffers);
	RenderPass& bind_tlas(const AccelKHR& tlas);

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

	RenderPass& skip_execution();
	template <typename T>
	RenderPass& push_constants(T* data);
	RenderPass& zero(const Resource& resource);
	RenderPass& zero(std::initializer_list<std::reference_wrapper<Buffer>> buffers);
	RenderPass& zero(std::initializer_list<std::reference_wrapper<Texture2D>> textures);
	RenderPass& zero(const Resource& resource, bool cond);
	RenderPass& copy(const Resource& src, const Resource& dst);
	void finalize();
	friend RenderGraph;
	std::vector<ResourceBinding> bound_resources;

	std::unordered_map<Buffer*, BufferStatus> affected_buffer_pointers;
	RenderGraph* rg;
	std::unique_ptr<GraphicsPassSettings> gfx_settings = nullptr;
	std::unique_ptr<RTPassSettings> rt_settings = nullptr;
	std::unique_ptr<ComputePassSettings> compute_settings = nullptr;
	PassType type;
	bool active = true;

   private:
	// When the automatic inference isn't used
	std::vector<Buffer*> explicit_buffer_writes;
	std::vector<Buffer*> explicit_buffer_reads;
	std::vector<Texture2D*> explicit_tex_writes;
	std::vector<Texture2D*> explicit_tex_reads;

	void write_impl(Buffer& buffer, VkAccessFlags access_flags);
	void write_impl(Texture2D& tex);
	void read_impl(Buffer& buffer);
	void read_impl(Buffer& buffer, VkAccessFlags access_flags);
	void read_impl(Texture2D& tex);
	void post_execution_barrier(Buffer& buffer, VkAccessFlags access_flags);


	void run(VkCommandBuffer cmd);
	void register_dependencies(Buffer& buffer, VkAccessFlags dst_access_flags);
	void register_dependencies(Texture2D& tex, VkImageLayout target_layout);
	void transition_resources();

	std::string name;
	Pipeline* pipeline;
	uint32_t pass_idx;
	int next_binding_idx = 0;
	std::vector<uint32_t> descriptor_counts;
	void* push_constant_data = nullptr;
	bool is_pipeline_cached;
	bool submitted = false;
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

	DescriptorInfo descriptor_infos[32] = {};

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

template<typename Settings>
inline RenderPass& RenderGraph::add_pass_impl(const std::string& name, const Settings& settings) {
	Pipeline* pipeline;
	uint32_t pass_idx = (uint32_t)passes.size();
	bool cached = false;
	ending_pass_idx++;
	if (pipeline_cache.find(name) != pipeline_cache.end()) {
		auto& storage = pipeline_cache[name];
		if (!recording && storage.pass_idxs.size()) {
			auto idx = storage.pass_idxs[storage.offset_idx];
			if constexpr (std::is_same_v<GraphicsPassSettings, Settings>) {
				auto& curr_pass = passes[idx];
				curr_pass.gfx_settings->color_outputs = settings.color_outputs;
				curr_pass.gfx_settings->depth_output = settings.depth_output;
			}
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
	PassType type;
	if constexpr (std::is_same_v<ComputePassSettings, Settings>) {
		type = PassType::Compute;
	} else if constexpr (std::is_same_v<GraphicsPassSettings, Settings>) {
		type = PassType::Graphics;
	} else {
		type = PassType::RT;
	}
	passes.emplace_back(type, pipeline, name, this, pass_idx, settings, cached);
	return passes.back();
}

template<typename T>
inline RenderPass& RenderPass::push_constants(T* data) {
	void* new_ptr = nullptr;
	if (rg->recording) {
		new_ptr = malloc(sizeof(T));
	} else {
		new_ptr = push_constant_data;
	}
	memcpy(new_ptr, data, sizeof(T));
	push_constant_data = new_ptr;
	return *this;
}
