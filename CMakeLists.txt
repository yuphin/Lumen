cmake_minimum_required(VERSION 3.4.0)
project(Lumen VERSION 0.1.0)

include(CTest)
enable_testing()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    message("Enabling vulkan validation layers")
    add_compile_definitions(USEVKVALIDATIONLAYER _DEBUG)
endif()

find_package(Vulkan REQUIRED)
find_package(Threads REQUIRED)

set(src_files)
add_subdirectory(src)
add_subdirectory(libs)

set(structures_INCLUDE_DIR
    "libs"
    "libs/spdlog/include"
    "libs/glfw/include"
    "libs/gli"
    "src"
)

include_directories(${Vulkan_INCLUDE_DIR};${structures_INCLUDE_DIR})
if(WIN32)
    message("-- Adding vulkan library lib to search paths for the linker to find spirv libraries")
    get_filename_component(vulkan_lib_folder ${Vulkan_LIBRARIES} DIRECTORY)
    link_directories(${vulkan_lib_folder})
endif()
add_compile_definitions(TINYEXR_USE_MINIZ)  # enables usage of miniz for opening exr images
add_compile_definitions(USE_SHADERC)        # enables shader compilation via shaderc
add_executable(Lumen ${src_files})
if(WIN32)
    target_link_libraries(Lumen PRIVATE Vulkan::Vulkan glfw volk shaderc_shared glm
    $<$<CONFIG:Debug>:spirv-cross-cored> $<$<NOT:$<CONFIG:Debug>>:spirv-cross-core>
    $<$<CONFIG:Debug>:spirv-cross-glsld> $<$<NOT:$<CONFIG:Debug>>:spirv-cross-glsl>)
else()
    target_link_libraries(Lumen PRIVATE Vulkan::Vulkan glfw volk shaderc_shared spirv-cross-core spirv-cross-glsl glm)
endif()

target_compile_features(Lumen PRIVATE cxx_std_20)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
