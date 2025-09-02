file(GLOB src_files 
    "external.cpp")

file(GLOB main_src
    "main.cpp"
    "main_tests.cpp"
)

file(GLOB framework_src
    "Framework/*.cpp"
    "Framework/*.h"
    "Framework/*.hpp"
    "Framework/*.inl"
    "Framework/Base/*.cpp"
    "Framework/Base/*.h"
    "Framework/Base/*.hpp"
    "Framework/Base/*.inl"
)

file(GLOB raytracing_src
    "RayTracer/*.cpp"
    "RayTracer/*.h"
    "RayTracer/*.hpp"
    "RayTracer/*.inl"
)

set(src_files "${src_files};${main_src};${framework_src};${raytracing_src}" PARENT_SCOPE)
