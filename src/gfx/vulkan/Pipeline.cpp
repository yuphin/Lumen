#include "lmhpch.h"
#include "Pipeline.h"


Pipeline::Pipeline(const VkDevice& device, std::vector<Shader>& shaders)
	: device(device), shaders(shaders), pipeline_CI{}, vert_shader_CI{}, frag_shader_CI{} {
	for (auto& file : shaders) {
		const std::filesystem::path path = file.filename;
		paths[file.filename] = std::filesystem::last_write_time(path);
	}
	ThreadPool::submit([this] {
		track();
	});
}

void Pipeline::track() {
	std::chrono::duration<int, std::milli> delay = std::chrono::milliseconds(100);
	while (running) {
		std::this_thread::sleep_for(delay);
		for (auto& file : shaders) {
			const std::filesystem::path path = file.filename;
			auto last_write = std::filesystem::last_write_time(path);
			if (paths.find(file.filename) != paths.end() &&
				last_write != paths[file.filename]) {
				LUMEN_TRACE("Shader changed: {0}", file.filename);
				paths[file.filename] = last_write;
				if (!file.compile()) {
					pipeline_CI.flags |= VK_PIPELINE_CREATE_DERIVATIVE_BIT;
					pipeline_CI.basePipelineHandle = handle;
					pipeline_CI.basePipelineIndex = -1;
					EventHandler::obsolete_pipelines.push_back(handle);
					update_pipeline();
					EventHandler::set(LumenEvent::EVENT_SHADER_RELOAD);
				}
			}
		}
	}

}

void Pipeline::cleanup() {
	if (handle != VK_NULL_HANDLE) 	vkDestroyPipeline(device, handle, nullptr);
	if (pipeline_layout != VK_NULL_HANDLE)	vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
	running = false;
	tracker.join();
}
