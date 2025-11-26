#pragma once
#include "CommandBuffer.h"
#include "Framework/RenderGraphTypes.h"
#include "Framework/VulkanStructs.h"
#include "Pipeline.h"
#include "Shader.h"
#include "Texture.h"
#include "EventPool.h"
#include "AccelerationStructure.h"
#include "Framework/Buffer.h"
#include "Framework/Texture.h"
#include "Framework/Camera.h"
#include "Framework/Base/SmallArray.h"

namespace lm {

////////////////////////////
// --- Limits for resources in a Render Pass  ---
static constexpr u64 MAX_RESOURCES_ZEROS = 16;
static constexpr u64 MAX_RESOURCES_COPIES = 16;
static constexpr u64 MAX_BUFFER_BARRIERS = 16;
static constexpr u64 MAX_IMG_BARRIERS = 16;
static constexpr u64 MAX_EXPLICIT_BUFFER_READ_WRITES = 16;
static constexpr u64 MAX_EXPLICIT_IMG_READ_WRITES = 16;
static constexpr u64 MAX_DESCRIPTORS = 32;

#define TO_STR(V) (#V)

#define REGISTER_BUFFER_WITH_ADDRESS(struct_type, struct_name, field_name, buffer_ptr, rg) \
	do {                                                                                   \
		auto key = std::string(#struct_type) + '_' + std::string(#field_name);             \
		rg->registered_buffer_pointers.insert(lm::str_from_cpp_str(key), buffer_ptr);      \
	} while (0)

class RenderGraph;
class RenderPass;

struct PipelineStorage {
	vk::Pipeline pipeline;
	lm::FixedArray<ResourceBinding> bound_resources;
	lm::FixedArray<vk::BVH> as_bindings;
	lm::HashMap<lm::String, vk::BufferStatus> affected_buffer_pointers;
	bool update_as_descriptor;
};

struct BufferSyncResources {
	lm::SmallArray<VkBufferMemoryBarrier2, MAX_BUFFER_BARRIERS> buffer_bariers;
	lm::SmallArray<VkDependencyInfo, MAX_BUFFER_BARRIERS> dependency_infos;
};
struct ImageSyncResources {
	lm::SmallArray<VkImageMemoryBarrier2, MAX_IMG_BARRIERS> img_barriers;
	lm::SmallArray<VkDependencyInfo, MAX_IMG_BARRIERS> dependency_infos;
};

struct BufferBarrier {
	VkBuffer buffer;
	VkAccessFlags src_access_flags = VK_ACCESS_SHADER_WRITE_BIT;
	VkAccessFlags dst_access_flags = VK_ACCESS_SHADER_READ_BIT;
};

// For now, there is only one set of BLASes to build per pass
struct BlasBuildData {
	util::Slice<vk::BVH> blases;
	util::Slice<vk::BlasInput> blas_inputs;
	// For barrier placement
	util::Slice<vk::Buffer*> source_buffers;
	//
	VkBuildAccelerationStructureFlagsKHR flags;
	// The owner is the caller of the blas_build(...) function
	vk::Buffer** scratch_buffer_ref = nullptr;
	inline bool is_valid() { return !blases.empty(); }
};

struct TlasBuildData {
	vk::BVH* tlas = nullptr;
	vk::Buffer* instances_buf = nullptr;
	vk::Buffer** scratch_buffer_ref = nullptr;
	u32 instance_count = 0;
	bool build_tlas_after_blas = false;
	bool update_tlas = false;
	VkBuildAccelerationStructureFlagsKHR flags;
	inline bool is_valid() { return instance_count != 0; }
};

class RenderGraph {
   public:
	RenderGraph();
	RenderPass& current_pass();
	RenderPass& add_rt(const lm::String& name, const vk::RTPassSettings& settings);
	RenderPass& add_gfx(const lm::String& name, const vk::GraphicsPassSettings& settings);
	RenderPass& add_compute(const lm::String& name, const vk::ComputePassSettings& settings);
	PipelineStorage* add_pass_impl_common(const lm::String& name, const lm::FixedArray<vk::ShaderMacro>& macros,
										  const lm::SpecializationConstantArray& specialization_data, bool& cached,
										  lm::String& name_with_macros, lm::String& macro_string);
	void init();
	void run(VkCommandBuffer cmd);
	void reset();
	void submit(vk::CommandBuffer& cmd);
	void run_and_submit(vk::CommandBuffer& cmd);
	void destroy();
	friend RenderPass;

	lm::FixedArray<vk::ShaderMacro> global_macro_defines;
	lm::FixedArray<std::pair<std::function<void(RenderPass*)>, u32>> pipeline_tasks;
	lm::FixedArray<std::function<void(RenderPass*)>> shader_tasks;
	lm::FixedArray<RenderPass> passes;

	// vk::Pipeline Name + Macro String + Specialization Constants -> vk::Pipeline
	lm::HashMap<u64, PipelineStorage> pipeline_cache;
	lm::HashMap<VkBuffer, std::pair<u32, VkAccessFlags>>
		buffer_resource_map;					 // Buffer handle - { Write Pass Idx, Access Type }
	lm::HashMap<VkImage, u32> img_resource_map;	 // Tex2D handle - Pass Idx
	lm::HashMap<lm::String, vk::Buffer*> registered_buffer_pointers;
	// vk::Shader Name + Macro String -> vk::Shader
	lm::HashMap<lm::String, vk::Shader> shader_cache;

	RenderGraphSettings settings;
	std::mutex shader_map_mutex;
	const bool multithreaded_pipeline_compilation = true;
	static const u32 INVALID_PASS_IDX = UINT_MAX;
	bool dirty_pass_encountered = false;
	bool reload_shaders = false;
};

class RenderPass {
   public:
	RenderPass() = default;

	RenderPass& bind(const ResourceBinding& binding);
	RenderPass& bind_texture_with_sampler(vk::Texture* tex, VkSampler sampler);
	RenderPass& bind(std::initializer_list<ResourceBinding> bindings);
	RenderPass& bind_texture_array(lm::FixedArray<vk::Texture*> texes, bool force_update = false);
	RenderPass& bind_buffer_array(std::span<vk::Buffer*> buffers, bool force_update = false);
	RenderPass& bind_tlas(const vk::BVH& tlas);

	RenderPass& read(std::initializer_list<vk::Buffer*> buffers);
	RenderPass& read(std::initializer_list<vk::Texture*> texes);
	RenderPass& read(ResourceBinding& resource);

	RenderPass& write(std::initializer_list<vk::Buffer*> buffers);
	RenderPass& write(std::initializer_list<vk::Texture*> texes);
	RenderPass& write(ResourceBinding& resource);

	RenderPass& skip_execution(bool condition = true);
	RenderPass& push_constants(void* data, u64 size, u64 alignment);
	template <typename T>
	inline RenderPass& push_constants(T* data) {
		return push_constants((void*)data, sizeof(T), alignof(T));
	}

	// Zero-ing happens before the pass runs
	RenderPass& zero(const Resource& resource);
	RenderPass& zero(std::initializer_list<vk::Buffer*> buffers);
	RenderPass& zero(std::initializer_list<vk::Texture*> textures);
	RenderPass& zero(const Resource& resource, bool cond);

	// Copy happens after the pass runs
	RenderPass& copy(const Resource& src, const Resource& dst);

	// BLAS building happens after the pass runs
	RenderPass& blas_build(util::Slice<vk::BVH> blases, util::Slice<vk::BlasInput> blas_inputs,
						   VkBuildAccelerationStructureFlagsKHR flags, util::Slice<vk::Buffer*> source_buffers,
						   vk::Buffer** scratch_buffer_ref);
	RenderPass& tlas_build(vk::BVH& tlas, vk::Buffer* instances_buf, u32 instance_count,
						   VkBuildAccelerationStructureFlagsKHR flags, vk::Buffer** scratch_buffer_ref,
						   bool build_tlas_after_blas = false, bool update_blas = false);
	void init();
	void finalize();
	friend RenderGraph;

	vk::PassType type;
	RenderGraph* rg;
	u32 pass_idx;
	std::unique_ptr<vk::GraphicsPassSettings> gfx_settings = nullptr;
	std::unique_ptr<vk::RTPassSettings> rt_settings = nullptr;
	std::unique_ptr<vk::ComputePassSettings> compute_settings = nullptr;
	lm::FixedArray<vk::ShaderMacro> macro_defines;
	PipelineStorage* pipeline_storage = nullptr;
	lm::String name;
	bool is_pipeline_cached = false;

   private:
	lm::SmallArray<Resource, MAX_RESOURCES_ZEROS> resource_zeros;
	lm::SmallArray<BufferBarrier, MAX_RESOURCES_ZEROS> prefill_buffer_barriers;
	lm::SmallArray<std::pair<Resource, Resource>, MAX_RESOURCES_COPIES> resource_copies;
	BufferSyncResources buffer_sync_resources;
	ImageSyncResources image_sync_resources;
	// TODO: Might be redundant?
	lm::SmallArray<BufferBarrier, MAX_RESOURCES_ZEROS> carryover_buffer_barriers;
	//
	lm::SmallArray<BufferBarrier, MAX_RESOURCES_COPIES> post_execution_buffer_barriers;
	lm::SmallArray<vk::Buffer*, MAX_EXPLICIT_BUFFER_READ_WRITES> explicit_buffer_writes;
	lm::SmallArray<vk::Buffer*, MAX_EXPLICIT_BUFFER_READ_WRITES> explicit_buffer_reads;
	lm::SmallArray<vk::Texture*, MAX_EXPLICIT_IMG_READ_WRITES> explicit_tex_writes;
	lm::SmallArray<vk::Texture*, MAX_EXPLICIT_IMG_READ_WRITES> explicit_tex_reads;
	lm::SmallArray<u32, MAX_DESCRIPTORS> descriptor_counts;
	lm::SmallArray<std::tuple<vk::Texture*, VkImageLayout, VkImageLayout>, MAX_IMG_BARRIERS> layout_transitions;
	/*
	Note:
	The assumption is that a SyncDescriptor is unique to a pass (either via
	Buffer or Image). Which is reasonable because each pass is comprised of a
	single shader dispatch
	*/
	lm::HashMap<VkBuffer, BufferSyncDescriptor> set_signals_buffer;
	lm::HashMap<VkBuffer, BufferSyncDescriptor> wait_signals_buffer;
	lm::HashMap<VkImage, ImageSyncDescriptor> set_signals_img;
	lm::HashMap<VkImage, ImageSyncDescriptor> wait_signals_img;
	BlasBuildData blas_build_data;
	TlasBuildData tlas_build_data;

	i32 next_binding_idx = 0;
	i32 next_as_binding_idx = 0;
	void* push_constant_data = nullptr;
	bool disable_execution = false;
	bool resources_initialized = false;
	vk::DescriptorInfo descriptor_infos[32] = {};

	RenderPass& read(vk::Texture* tex);
	RenderPass& read(vk::Buffer* buffer);

	RenderPass& write(vk::Texture* tex);
	RenderPass& write(vk::Buffer* buffer);

	void transition_resources();
	void post_execution_barrier(vk::Buffer* buffer, VkAccessFlags access_flags);
	enum class BufferSyncFlags {
		NONE = 0x0,
		BUFFER_ZERO = 0x1,
		BUFFER_COPY = 0x2,
		BUFFER_AS_BUILD = 0x4,
	};
	void register_dependencies(const vk::Buffer* buffer, VkAccessFlags dst_access_flags,
							   BufferSyncFlags flags = BufferSyncFlags::NONE);
	void register_dependencies(vk::Texture* tex, VkImageLayout target_layout);
	void write_impl(const vk::Buffer* buffer, VkAccessFlags access_flags,
					BufferSyncFlags flags = BufferSyncFlags::NONE);
	void write_impl(vk::Texture* tex, VkAccessFlags access_flags = VK_ACCESS_SHADER_WRITE_BIT);
	void read_impl(const vk::Buffer* buffer, VkAccessFlags access_flags = VK_ACCESS_SHADER_READ_BIT,
				   BufferSyncFlags flags = BufferSyncFlags::NONE);
	void read_impl(vk::Texture* tex);

	void run(VkCommandBuffer cmd);
};

void render_pass_init_gfx(RenderPass& pass, vk::PassType type, const lm::String& name, RenderGraph* rg, u32 pass_idx,
						  const vk::GraphicsPassSettings& gfx_settings, const lm::String& macro_string,
						  PipelineStorage* pipeline_storage, bool cached = false);

void render_pass_init_rt(RenderPass& pass, vk::PassType type, const lm::String& name, RenderGraph* rg, u32 pass_idx,
						 const vk::RTPassSettings& rt_settings, const lm::String& macro_string,
						 PipelineStorage* pipeline_storage, bool cached = false);

void render_pass_init_compute(RenderPass&, vk::PassType type, const lm::String& name, RenderGraph* rg, u32 pass_idx,
							  const vk::ComputePassSettings& compute_settings, const lm::String& macro_string,
							  PipelineStorage* pipeline_storage, bool cached = false);

}  // namespace lm
