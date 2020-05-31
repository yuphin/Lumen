#pragma once

#include "vk/VKBase.h"



class Lumen : public VKBase {
    public: 
    Lumen(int width, int height, bool fullscreen, bool debug);
    void run();
    void create_render_pass();
    void create_gfx_pipeline();
    void create_framebuffers();
    void create_command_buffers();

};
