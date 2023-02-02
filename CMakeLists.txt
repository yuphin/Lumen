cmake_minimum_required(VERSION 3.7.0)
project(Lumen VERSION 0.1.0)

include(CTest)
enable_testing()

set(CMAKE_CXX_STANDARD 20)
add_compile_definitions(TINYGLTF_NO_STB_IMAGE_WRITE TINYEXR_USE_MINIZ)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

set(Lumen_SRC)
add_subdirectory(src)
add_subdirectory(libs)
find_package(spirv_cross_cpp REQUIRED)

include_directories("libs/" "libs/gli" "src/" ${Vulkan_INCLUDE_DIR})
add_executable(Lumen ${Lumen_SRC})
target_link_libraries(Lumen PRIVATE Vulkan::Vulkan glfw spirv-cross-cpp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
