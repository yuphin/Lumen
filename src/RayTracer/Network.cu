#ifndef __NVCC__
#define __NVCC__
#endif
#include "Network.h"

#include <curand.h>
#include <tiny-cuda-nn/config.h>
#include <tiny-cuda-nn/common.h>
#include <tiny-cuda-nn/encoding.h>

using precision_t = tcnn::network_precision_t;
// cuda related
cudaStream_t inference_stream;
cudaStream_t training_stream;
curandGenerator_t rng;

// Position: 3
// Scattered Direction: 2
// Normal: 2
// Roughness: 1
// Diffuse Reflectance: 3
// Specular Reflectance: 3
constexpr static uint32_t NUM_INPUT_DIMS = 14;
// RGB Radiance: 3
constexpr static uint32_t NUM_OUTPUT_DIMS = 3;

struct NeuralRadianceCache::NetworkInternal {
	std::shared_ptr<tcnn::Loss<precision_t>> loss = nullptr;
	std::shared_ptr<tcnn::Optimizer<precision_t>> optimizer = nullptr;
	std::shared_ptr<tcnn::NetworkWithInputEncoding<precision_t>> network = nullptr;
	std::shared_ptr<tcnn::Trainer<float, precision_t, precision_t>> trainer = nullptr;
};

NeuralRadianceCache::NeuralRadianceCache() { m = new NetworkInternal(); }

NeuralRadianceCache::~NeuralRadianceCache() { delete m; }

void NeuralRadianceCache::initialize(PositionEncoding pos_encoding, uint32_t num_hidden_layers, float learning_rate) {
	tcnn::json config = {{"loss", {{"otype", "RelativeL2Luminance"}}},
						 {"optimizer",
						  {{"otype", "EMA"},
						   {"decay", 0.99f},
						   {"nested",
							{
								{"otype", "Adam"},
								{"learning_rate", learning_rate},
								{"beta1", 0.9f},
								{"beta2", 0.99f},
								{"l2_reg", 1e-6f},
							}}}},
						 {"network",
						  {
							  {"otype", "FullyFusedMLP"},
							  {"n_neurons", 64},
							  {"n_hidden_layers", num_hidden_layers},
							  {"activation", "ReLU"},
							  {"output_activation", "None"},
						  }}};
	if (pos_encoding == PositionEncoding::TriangleWave) {
		// config["encoding"] = { {"otype", "NRC"} };
		config["encoding"] = {{"otype", "Composite"},
							  {"nested",
							   {
								   {
									   {"n_dims_to_encode", 3},
									   {"otype", "TriangleWave"},
									   {"n_frequencies", 12},
								   },
								   {
									   {"n_dims_to_encode", 5},
									   {"otype", "OneBlob"},
									   {"n_bins", 4},
								   },
								   {{"n_dims_to_encode", 6}, {"otype", "Identity"}},
							   }}};
		config["optimizer"]["nested"]["epsilon"] = 1e-8f;
	} else if (pos_encoding == PositionEncoding::HashGrid) {
		config["encoding"] = {{"otype", "Composite"},
							  {"nested",
							   {
								   {
									   {"n_dims_to_encode", 3},
									   {"otype", "HashGrid"},
									   {"per_level_scale", 2.0f},
									   {"log2_hashmap_size", 15},
									   {"base_resolution", 16},
									   {"n_levels", 16},
									   {"n_features_per_level", 2},
								   },
								   {
									   {"n_dims_to_encode", 5},
									   {"otype", "OneBlob"},
									   {"n_bins", 4},
								   },
								   {{"n_dims_to_encode", 6}, {"otype", "Identity"}},
							   }}};
		config["optimizer"]["nested"]["epsilon"] = 1e-15f;
	}

	m->loss.reset(tcnn::create_loss<precision_t>(config.value("loss", tcnn::json::object())));
	m->optimizer.reset(tcnn::create_optimizer<precision_t>(config.value("optimizer", tcnn::json::object())));
	m->network = std::make_shared<tcnn::NetworkWithInputEncoding<precision_t>>(
		NUM_INPUT_DIMS, NUM_OUTPUT_DIMS, config.value("encoding", tcnn::json::object()),
		config.value("network", tcnn::json::object()));

	m->trainer = std::make_shared<tcnn::Trainer<float, precision_t, precision_t>>(m->network, m->optimizer, m->loss);

}

void NeuralRadianceCache::finalize() {
	m->trainer = nullptr;
	m->network = nullptr;
	m->optimizer = nullptr;
	m->loss = nullptr;
}

void NeuralRadianceCache::infer(CUstream stream, float* input, uint32_t num_data, float* prediction_data) {
	assert((num_data & 0x7F) == 0);
	tcnn::GPUMatrix<float> inputs(input, NUM_INPUT_DIMS, num_data);
	tcnn::GPUMatrix<float> predictions(prediction_data, NUM_OUTPUT_DIMS, num_data);
	m->network->inference(stream, inputs, predictions);
}

void NeuralRadianceCache::train(CUstream stream, float* input, float* target_data, uint32_t num_data,
								float* loss_on_cpu) {
	assert((num_data & 0x7F) == 0);
	tcnn::GPUMatrix<float> inputs(input, NUM_INPUT_DIMS, num_data);
	tcnn::GPUMatrix<float> targets(target_data, NUM_OUTPUT_DIMS, num_data);
	auto context = m->trainer->training_step(stream, inputs, targets);
	if (loss_on_cpu) *loss_on_cpu = m->trainer->loss(stream, *context);
}
