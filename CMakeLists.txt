cmake_minimum_required(VERSION 3.7.0)
project(Lumen VERSION 0.1.0)

include(CTest)
enable_testing()

set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

set(Lumen_SRC)
add_subdirectory(src)

include_directories("libs/" "src/" ${Vulkan_INCLUDE_DIR})
add_executable(Lumen ${Lumen_SRC})
target_link_libraries(Lumen Vulkan::Vulkan glfw3)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
