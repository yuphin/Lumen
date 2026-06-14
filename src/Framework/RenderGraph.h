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
		lm::String key = #struct_type "_" #field_name;                                     \
		rg->registered_buffer_pointers.insert(key, buffer_ptr);                            \
	} while (0)

class RenderGraph;
class RenderPass;

struct PipelineTask {
	void (*procedure)(RenderPass*);
	u32 pass_idx;
};

struct PipelineStorage {
	vk::Pipeline pipeline;
	lm::SmallArray<ResourceBinding, MAX_DESCRIPTORS> bound_resources;
	lm::SmallArray<vk::BVH, vk::MAX_AS_BINDING_COUNT> as_bindings;
	lm::HashMap<lm::String, vk::BufferStatus> affected_buffer_pointers;
	u32 reload_counter;
	bool update_as_descriptor;
};

struct BufferSyncResources {
	lm::SmallArray<VkBufferMemoryBarrier2, MAX_BUFFER_BARRIERS> buffer_barriers;
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
	VkPipelineStageFlags src_stage = 0;
	VkPipelineStageFlags dst_stage = 0;
};

struct ImageBarrier {
	VkImage image;
	VkAccessFlags src_access_flags;
	VkAccessFlags dst_access_flags;
	VkImageLayout old_layout;
	VkImageLayout new_layout;
	VkImageAspectFlags image_aspect;
	VkPipelineStageFlags src_stage;
	VkPipelineStageFlags dst_stage;
};

struct BufferResourceState {
	u32 pass_idx;
	VkAccessFlags access_flags;
	VkPipelineStageFlags stage;
	bool event_eligible;
};

struct ImageResourceState {
	u32 pass_idx;
	VkAccessFlags access_flags;
	VkImageLayout layout;
	VkImageAspectFlags image_aspect;
	VkPipelineStageFlags stage;
	bool event_eligible;
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

struct LayoutTransitionData {
	vk::Texture* tex;
	VkImageLayout src_layout;
	VkImageLayout dst_layout;
};

template <typename T1, typename T2>
struct pair {
	T1 first;
	T2 second;

	bool operator!=(const pair& other) const {
		return first != other.first || second != other.second;
	}

	bool operator==(const pair& other) const { return first == other.first && second == other.second; }
};

class RenderGraph {
   public:
	RenderGraph();
	RenderPass& current_pass();
	RenderPass& add_rt(const lm::String& name, const vk::RTPassSettings& settings);
	RenderPass& add_gfx(const lm::String& name, const vk::GraphicsPassSettings& settings);
	RenderPass& add_compute(const lm::String& name, const vk::ComputePassSettings& settings);
	PipelineStorage* add_pass_impl_common(const lm::String& name, const vk::ShaderMacroArray& macros,
										  const lm::SpecializationConstantArray& specialization_data, bool& cached,
										  lm::String& name_with_macros, lm::String& macro_string);
	void init();
	void run(VkCommandBuffer cmd);
	void reset();
	void submit(vk::CommandBuffer& cmd);
	void run_and_submit(vk::CommandBuffer& cmd);
	void destroy();
	lm::Arena* arena();

	friend RenderPass;

	vk::ShaderMacroArray global_macro_defines;
	lm::FixedArray<PipelineTask> pipeline_tasks;
	lm::FixedArray<RenderPass> passes;

	// vk::Pipeline Name + Macro String + Specialization Constants -> vk::Pipeline
	lm::HashMap<u64, PipelineStorage> pipeline_cache;
	lm::HashMap<VkBuffer, BufferResourceState> buffer_resource_map;
	lm::HashMap<VkImage, ImageResourceState> img_resource_map;
	lm::HashMap<lm::String, vk::Buffer*> registered_buffer_pointers;
	// vk::Shader Name + Macro String -> vk::Shader
	lm::HashMap<lm::String, vk::Shader> shader_cache;

	RenderGraphSettings settings;
	os::Mutex shader_map_mutex;
	const bool multithreaded_pipeline_compilation = true;
	static const u32 INVALID_PASS_IDX = UINT_MAX;
	bool dirty_pass_encountered = false;
	bool reload_shaders = false;
	u32 reload_counter = 0;
};

class RenderPass {
   public:
	RenderPass() = default;

	RenderPass& bind(const ResourceBinding& binding);
	RenderPass& bind_texture_with_sampler(vk::Texture* tex, VkSampler sampler);
	RenderPass& bind(std::initializer_list<ResourceBinding> bindings);
	RenderPass& bind_texture_array(lm::FixedArray<vk::Texture*> texes, bool force_update = false);
	RenderPass& bind_buffer_array(lm::FixedArray<vk::Buffer*> buffers, bool force_update = false);
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
	PipelineStorage* pipeline_storage = nullptr;
	lm::String name;
	bool is_pipeline_cached = false;
	vk::PassSettings settings;

   private:
	static void create_gfx_pipeline(RenderPass* pass);
	static void create_rt_pipeline(RenderPass* pass);
	static void create_compute_pipeline(RenderPass* pass);
	static void update_rt_descriptors(RenderPass* pass);

	lm::SmallArray<Resource, MAX_RESOURCES_ZEROS> resource_zeros;
	lm::SmallArray<BufferBarrier, MAX_RESOURCES_ZEROS> prefill_buffer_barriers;
	lm::SmallArray<lm::pair<Resource, Resource>, MAX_RESOURCES_COPIES> resource_copies;
	BufferSyncResources buffer_sync_resources;
	ImageSyncResources image_sync_resources;
	// TODO: Might be redundant?
	lm::SmallArray<BufferBarrier, MAX_RESOURCES_ZEROS> carryover_buffer_barriers;
	lm::SmallArray<ImageBarrier, MAX_IMG_BARRIERS> carryover_image_barriers;
	//
	lm::SmallArray<BufferBarrier, MAX_RESOURCES_COPIES> post_execution_buffer_barriers;
	lm::SmallArray<vk::Buffer*, MAX_EXPLICIT_BUFFER_READ_WRITES> explicit_buffer_writes;
	lm::SmallArray<vk::Buffer*, MAX_EXPLICIT_BUFFER_READ_WRITES> explicit_buffer_reads;
	lm::SmallArray<vk::Texture*, MAX_EXPLICIT_IMG_READ_WRITES> explicit_tex_writes;
	lm::SmallArray<vk::Texture*, MAX_EXPLICIT_IMG_READ_WRITES> explicit_tex_reads;
	lm::SmallArray<u32, MAX_DESCRIPTORS> descriptor_counts;
	lm::SmallArray<LayoutTransitionData, MAX_IMG_BARRIERS> layout_transitions;
	// Resource dependencies that must execute before this pass.
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
	vk::DescriptorInfo descriptor_infos[MAX_DESCRIPTORS] = {};

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
	bool register_dependencies(const vk::Buffer* buffer, VkAccessFlags dst_access_flags,
							   BufferSyncFlags flags = BufferSyncFlags::NONE, VkPipelineStageFlags dst_stage = 0);
	bool register_dependencies(vk::Texture* tex, VkAccessFlags dst_access_flags, VkImageLayout target_layout);
	void write_impl(const vk::Buffer* buffer, VkAccessFlags access_flags, BufferSyncFlags flags = BufferSyncFlags::NONE,
					VkPipelineStageFlags stage = 0);
	void write_impl(vk::Texture* tex, VkAccessFlags access_flags = VK_ACCESS_SHADER_WRITE_BIT);
	void read_impl(const vk::Buffer* buffer, VkAccessFlags access_flags = VK_ACCESS_SHADER_READ_BIT,
				   BufferSyncFlags flags = BufferSyncFlags::NONE, VkPipelineStageFlags stage = 0);
	void read_impl(vk::Texture* tex);

	void run(VkCommandBuffer cmd);
};

}  // namespace lm
