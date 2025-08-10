#pragma once

namespace ImageUtils {
f32* load_exr(const char* img_name, i32& width, i32& height);
void save_exr(const f32* rgb, i32 width, i32 height, const char* outfilename);
}
