cmake_minimum_required(VERSION 3.4.0)
project(Lumen VERSION 0.1.0)

include(CTest)
enable_testing()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    message("Enabling vulkan validation layers")
    add_compile_definitions(USEVKVALIDATIONLAYER)
endif()

find_package(Threads REQUIRED)
find_package(Vulkan REQUIRED)
find_package(spirv_cross_core REQUIRED)
find_package(spirv_cross_glsl REQUIRED)
find_package(spirv_cross_cpp REQUIRED)

set(src_files)
add_subdirectory(src)
add_subdirectory(libs)

set(structures_INCLUDE_DIR
    "libs"
    "libs/spdlog/include"
    "libs/glfw/include"
    "libs/shaderc/include"
    "src"
)
include_directories(${Vulkan_INCLUDE_DIR};${GLSLang_INCLUDE_DIR};${netCDF_INCLUDE_DIR};${structures_INCLUDE_DIR})
add_compile_definitions(TINYEXR_USE_MINIZ)
add_executable(Lumen ${src_files})

target_link_libraries(Lumen PRIVATE Vulkan::Vulkan glfw spirv-cross-core spirv-cross-cpp spirv-cross-glsl)
target_compile_features(Lumen PRIVATE cxx_std_20)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
