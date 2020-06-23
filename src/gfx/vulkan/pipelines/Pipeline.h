#pragma once
#include <vulkan/vulkan.h>
#include "../../Shader.h"
#include "../VKStructs.h"
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <utility>
#include <filesystem>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <string>
#include <functional>
#include "../../../src/core/Event.h"

struct Pipeline {
	VkPipeline handle = VK_NULL_HANDLE;
	VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
	std::vector<Shader> shaders;
	VkDevice device;
	std::thread tracker;
	std::unordered_map<std::string, std::filesystem::file_time_type> paths;
	VkGraphicsPipelineCreateInfo pipeline_CI = {};
	bool running = true;


	Pipeline(const VkDevice& device, std::vector<Shader>& shaders)
		: device(device), shaders(shaders) {
		for (auto& file : shaders) {
			const std::filesystem::path path = file.filename;
			paths[file.filename] = std::filesystem::last_write_time(path);
		}
		tracker = std::thread([=] {
			track();
			});

	}
	void track() {
		std::chrono::duration<int, std::milli> delay = std::chrono::milliseconds(100);
		while (running) {
			std::this_thread::sleep_for(delay);
			for (auto& file : shaders) {
				const std::filesystem::path path = file.filename;
				auto last_write = std::filesystem::last_write_time(path);
				if (paths.find(file.filename) != paths.end() &&
					last_write != paths[file.filename]) {
					std::cout << "Shader changed: " << file.filename << std::endl;
					paths[file.filename] = last_write;
					file.compile();
					create_pipeline_with_shaders(pipeline_CI);
					EventHandler::set(LumenEvent::EVENT_SHADER_RELOAD);
				}

			}
		}


	}


	virtual void create_pipeline_with_shaders(VkGraphicsPipelineCreateInfo& ci) = 0;
	void cleanup() {
		if (handle != VK_NULL_HANDLE) 	vkDestroyPipeline(device, handle, nullptr);
		if (pipeline_layout != VK_NULL_HANDLE)	vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
		running = false;
		tracker.join();
	}




};
