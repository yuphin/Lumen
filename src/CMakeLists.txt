file(GLOB main_src
    "main.cpp"
)

file(GLOB framework_src
    "Framework/*.cpp"
    "Framework/*.h"
    "Framework/*.hpp"
)

file(GLOB raytracing_src
    "RayTracer/*.cpp"
    "RayTracer/*.h"
    "RayTracer/*.hpp"
)
file(GLOB network_src
    "RayTracer/*.cu"
)

set(src_files "${src_files};${main_src};${framework_src};${raytracing_src}" PARENT_SCOPE)

set(cuda_src "${cuda_src};${network_src}" PARENT_SCOPE)
