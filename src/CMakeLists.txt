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

set(src_files "${src_files};${main_src};${framework_src};${raytracing_src}" PARENT_SCOPE)
