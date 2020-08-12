#pragma once

#include "gfx/vulkan/Base.h"
#include "gfx/Shader.h"
#include "gfx/vulkan/pipelines/DefaultPipeline.h"
#include "lmhpch.h"


class Lumen : public VulkanBase {
    public: 
    Lumen(int width, int height, bool fullscreen, bool debug);
    void run();
    void create_render_pass() override;
    void create_gfx_pipeline() override;
    void prepare_vertex_buffers();
    void prepare_render();
    void setup_vertex_descriptions();
    void build_command_buffers() override;
    ~Lumen();
    std::vector<Shader> shaders;

};
