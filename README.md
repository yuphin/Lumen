# Lumen
 [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

 Lumen is a Vulkan ray tracing framework that contains a variety of variance reduction techniques. Its aim is to accelerate some of the well-known rendering techniques in computer graphics literature with the help of the ray tracing hardware in modern GPUs. All the techniques Lumen implements run fully on the GPU.

## Features
 ### Rendering Techniques

 - Unidirectional Path Tracer with Multiple Importance Sampling (Path)
 - Bidirectional Path Tracer (BDPT)
 - Stochastic Progressive Photon Mapping (SPPM)
 - Vertex Connection and Merging (VCM)
 - Primary Sample Space Metropolis Light Transport (PSSMLT)
 - Combined VCM + MLT Integrator (VCMMLT)
 - ReSTIR
 - ReSTIR GI

### Engine
 - Lambertian, mirror, glass, glossy and optionally Disney BSDF
 - Spot (point), polygonal and directional lights
 - JSON-based scene system
 - Modified Mitsuba parser
 - A lightweight Vulkan abstraction layer from scratch
 - EXR output (F10)
 - On-the-fly RMSE computation


## Showcase
##### Caustics Glass (VCM)
![0](/media/GlassVCM.png?raw=true "Japanese Classroom")

##### Mitsuba Torus (VCM)
![1](/media/TorusVCM.png?raw=true "Japanese Classroom")

##### Japanese Classroom (Courtesy of [Benedikt Bitterli](https://benedikt-bitterli.me/resources/))
![2](/media/ClassroomPath.png?raw=true "Japanese Classroom")

##### ReSTIR 1 sample per pixel ([Video comparison](https://drive.google.com/file/d/1H2OWNuinCjOEpfb5OWKAA_yl25t9_Hol/view?usp=sharing))
![3](/media/ReSTIR1spp.png?raw=true "Japanese Classroom")
##### ReSTIR GI 1 sample per pixel ([Video comparison](https://drive.google.com/file/d/1UV1FpyMhtcX8cUo4CFIXFXhWI8UWr121/view?usp=sharing))
![4](/media/ReSTIRGI1spp.png?raw=true "Japanese Classroom")
## Building

To build Lumen, start cloning the repository with

```shell
git clone --recursive https://github.com/yuphin/Lumen.git
```
### Requirements
- Turing+ or RDNA2 GPU
- VS2022 or VS2019

Currently, Lumen only builds on Windows, however, there is no platform specific code in the codebase and it can be ported to Linux with ease.

## Usage
Some of the sample scenes can be found in the `scenes/` directory.
Sample scene files with various integrators can be found in the `scenes/cornell_box/` directory.



## References

 - [Physically Based Rendering: From Theory to Implementation](http://www.pbr-book.org/)
 - [Light Transport Simulation with Vertex Connection and Merging](https://cgg.mff.cuni.cz/~jaroslav/papers/2012-vcm/)
 - [Spatiotemporal Reservoir Resampling for Real-time Ray Tracing with Dynamic Direct Lighting](https://cs.dartmouth.edu/wjarosz/publications/bitterli20spatiotemporal.html)
 - [ReSTIR GI: Path Resampling for Real-Time Path Tracing](https://research.nvidia.com/publication/2021-06_restir-gi-path-resampling-real-time-path-tracing)
 - [Robust Monte Carlo Methods for Light Transport Simulation](https://dl.acm.org/doi/10.5555/927297)
 - [Robust Light Transport Simulation via Metropolised Bidirectional Estimators](https://dl.acm.org/doi/10.1145/2980179.2982411)