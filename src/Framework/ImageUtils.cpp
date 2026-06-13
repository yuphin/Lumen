#include "ImageUtils.h"
#include <tinyexr.h>

namespace ImageUtils {

f32* load_exr(const char* img_name, i32& width, i32& height) {
	// Load the ground truth image
	const char* err = nullptr;
	f32* data = nullptr;
	i32 ret = LoadEXR(&data, &width, &height, img_name, &err);
	if (ret != TINYEXR_SUCCESS) {
		if (err) {
			LUMEN_ERROR("EXR loading error", err);
			FreeEXRErrorMessage(err);
		}
	}
	return data;
}

void save_exr(const f32* rgb, i32 width, i32 height, const char* outfilename) {
	EXRHeader header;
	InitEXRHeader(&header);
	EXRImage image;
	InitEXRImage(&image);
	image.num_channels = 3;

	if (width <= 0 || height <= 0) {
		LUMEN_ERROR("Cannot save an EXR with invalid dimensions: %d x %d", width, height);
	}
	u64 pixel_count = static_cast<u64>(width) * static_cast<u64>(height);
	if (pixel_count > SIZE_MAX / (3 * sizeof(f32))) {
		LUMEN_ERROR("EXR dimensions are too large: %d x %d", width, height);
	}
	f32* channel_data = static_cast<f32*>(malloc(3 * pixel_count * sizeof(f32)));
	if (!channel_data) {
		LUMEN_ERROR("Could not allocate EXR channel storage");
	}
	f32* images[3] = {channel_data, channel_data + pixel_count, channel_data + 2 * pixel_count};

	// Split RGBRGBRGB... into R, G and B layer
	for (u64 i = 0; i < pixel_count; i++) {
		images[0][i] = rgb[4 * i + 0];
		images[1][i] = rgb[4 * i + 1];
		images[2][i] = rgb[4 * i + 2];
	}

	f32* image_ptr[3];
	image_ptr[0] = images[2];  // B
	image_ptr[1] = images[1];  // G
	image_ptr[2] = images[0];  // R

	image.images = (unsigned char**)image_ptr;
	image.width = width;
	image.height = height;

	header.num_channels = 3;
	header.channels = (EXRChannelInfo*)malloc(sizeof(EXRChannelInfo) * header.num_channels);
	// Must be (A)BGR order, since most of EXR viewers expect this channel
	// order.
#ifdef _MSC_VER
	strncpy_s(header.channels[0].name, "B", 255);
	header.channels[0].name[strlen("B")] = '\0';
	strncpy_s(header.channels[1].name, "G", 255);
	header.channels[1].name[strlen("G")] = '\0';
	strncpy_s(header.channels[2].name, "R", 255);
	header.channels[2].name[strlen("R")] = '\0';
#else
	strncpy(header.channels[0].name, "B", 255);
	header.channels[0].name[strlen("B")] = '\0';
	strncpy(header.channels[1].name, "G", 255);
	header.channels[1].name[strlen("G")] = '\0';
	strncpy(header.channels[2].name, "R", 255);
	header.channels[2].name[strlen("R")] = '\0';
#endif

	header.pixel_types = (i32*)malloc(sizeof(i32) * header.num_channels);
	header.requested_pixel_types = (i32*)malloc(sizeof(i32) * header.num_channels);
	for (i32 i = 0; i < header.num_channels; i++) {
		header.pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;		   // pixel type of input image
		header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_HALF;  // pixel type of output image to be stored
																   // in .EXR
	}

	const char* err = NULL;	 // or nullptr in C++11 or later.
	i32 ret = SaveEXRImageToFile(&image, &header, outfilename, &err);
	if (ret != TINYEXR_SUCCESS) {
		LUMEN_ERROR("Save EXR err: %s", err);
		FreeEXRErrorMessage(err);  // free's buffer for an error message
	}
	LUMEN_TRACE("Saved exr file. [ %s ]", outfilename);

	free(header.channels);
	free(header.pixel_types);
	free(header.requested_pixel_types);
	free(channel_data);
}
}  // namespace ImageUtils
