#pragma once

#include "vk/VKBase.h"



class Lumen : public VKBase {
    public: 
    Lumen(int width, int height, bool fullscreen, bool debug);
    void run();
    virtual void create_render_pass();
    virtual void create_gfx_pipeline();
    virtual void create_framebuffers();
    virtual void create_command_buffers();

};
