cmake_minimum_required(VERSION 3.7.0)
project(Lumen VERSION 0.1.0)

include(CTest)
enable_testing()

find_package(Vulkan REQUIRED)
find_package(GLFW REQUIRED)

set(Lumen_SRC)
add_subdirectory(src)

add_executable(Lumen ${Lumen_SRC})

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
