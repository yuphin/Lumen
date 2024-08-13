#pragma once

namespace ImageUtils {
float* load_exr(const char* img_name, int& width, int& height);
void save_exr(const float* rgb, int width, int height, const char* outfilename);
}
