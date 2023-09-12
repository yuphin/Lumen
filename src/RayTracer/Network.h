#include <cuda.h>
#include <cuda_runtime.h>
#include <cuda_fp16.h>
//#ifdef __NVCC__
//#define NRC_CALLABLE __host__ __device__
//#else
//#define NRC_CALLABLE
//#endif

enum class PositionEncoding {
	TriangleWave,
	HashGrid,
};

class NeuralRadianceCache {
	struct NetworkInternal;
	NetworkInternal* m = nullptr;
   public:
	NeuralRadianceCache();
	~NeuralRadianceCache();

	void initialize(PositionEncoding pos_encoding, uint32_t num_hidden_layers, float learning_rate);
	void finalize();

	void infer(CUstream stream, float* input, uint32_t num_data, float* prediction_data);
	void train(CUstream stream, float* input, float* target_data, uint32_t num_data, float* loss_on_cpu = nullptr);

};
