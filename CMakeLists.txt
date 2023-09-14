cmake_minimum_required(VERSION 3.4.0)
project(Lumen VERSION 0.1.0 LANGUAGES CXX CUDA)

include(CTest)
enable_testing()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    message("Enabling vulkan validation layers")
    add_compile_definitions(USEVKVALIDATIONLAYER _DEBUG)
endif()

find_package(Vulkan REQUIRED)
find_package(Threads REQUIRED)
find_package(CUDA REQUIRED)

set(TCNN_CUDA_ARCHITECTURES "86")
set(CUDA_ARCHITECTURES "86")
set(CMAKE_CUDA_ARCHITECTURES "86")

if (WIN32)
   set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
endif()

set(src_files)
set(cuda_src)
add_subdirectory(src)
add_subdirectory(libs)

###############################################################################
# CUDA compiler setup
###############################################################################

# Figure out CUDA version
if(CMAKE_CUDA_COMPILER_LOADED)
	if (CMAKE_CUDA_COMPILER_ID STREQUAL "NVIDIA" AND CMAKE_CUDA_COMPILER_VERSION MATCHES "^([0-9]+\\.[0-9]+)")
		set(CUDA_VERSION "${CMAKE_MATCH_1}")
	endif()
endif()

set(CMAKE_CUDA_STANDARD 14)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_EXTENSIONS OFF)
set(CUDA_LINK_LIBRARIES_KEYWORD PUBLIC)

set(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS};--expt-extended-lambda;-lineinfo;")

# If the CUDA version does not support the chosen architecture, target
# the latest supported one instead.
if (CUDA_VERSION VERSION_LESS 11.0)
	set(LATEST_SUPPORTED_CUDA_ARCHITECTURE 75)
elseif (CUDA_VERSION VERSION_LESS 11.1)
	set(LATEST_SUPPORTED_CUDA_ARCHITECTURE 80)
elseif (CUDA_VERSION VERSION_LESS 11.8)
	set(LATEST_SUPPORTED_CUDA_ARCHITECTURE 86)
else()
	set(LATEST_SUPPORTED_CUDA_ARCHITECTURE 90)
endif()

if (CUDA_VERSION VERSION_GREATER_EQUAL 12.0)
	set(EARLIEST_SUPPORTED_CUDA_ARCHITECTURE 50)
else()
	set(EARLIEST_SUPPORTED_CUDA_ARCHITECTURE 20)
endif()

foreach (CUDA_CC IN LISTS CMAKE_CUDA_ARCHITECTURES)
	if (CUDA_CC GREATER ${LATEST_SUPPORTED_CUDA_ARCHITECTURE})
		message(WARNING "CUDA version ${CUDA_VERSION} is too low for detected architecture ${CUDA_CC}. Targeting the highest supported architecture ${LATEST_SUPPORTED_CUDA_ARCHITECTURE} instead.")
		list(REMOVE_ITEM CMAKE_CUDA_ARCHITECTURES ${CUDA_CC})
		if (NOT CMAKE_CUDA_ARCHITECTURES)
			list(APPEND CMAKE_CUDA_ARCHITECTURES ${LATEST_SUPPORTED_CUDA_ARCHITECTURE})
		endif()
	endif()

	if (CUDA_CC LESS ${EARLIEST_SUPPORTED_CUDA_ARCHITECTURE})
		message(ERROR "CUDA version ${CUDA_VERSION} no longer supports detected architecture ${CUDA_CC}. Targeting the lowest supported architecture ${EARLIEST_SUPPORTED_CUDA_ARCHITECTURE} instead.")
		list(REMOVE_ITEM CMAKE_CUDA_ARCHITECTURES ${CUDA_CC})
		if (NOT CMAKE_CUDA_ARCHITECTURES)
			list(APPEND CMAKE_CUDA_ARCHITECTURES ${EARLIEST_SUPPORTED_CUDA_ARCHITECTURE})
		endif()
	endif()
endforeach(CUDA_CC)

if (NOT CMAKE_CUDA_ARCHITECTURES)
	list(APPEND CMAKE_CUDA_ARCHITECTURES ${LATEST_SUPPORTED_CUDA_ARCHITECTURE})
endif()

# Sort the list to obtain lowest architecture that must be compiled for.
list(SORT CMAKE_CUDA_ARCHITECTURES COMPARE NATURAL ORDER ASCENDING)
list(GET CMAKE_CUDA_ARCHITECTURES 0 MIN_GPU_ARCH)

string(REPLACE "-virtual" "" MIN_GPU_ARCH "${MIN_GPU_ARCH}")

message(STATUS "Targeting CUDA architectures: ${CMAKE_CUDA_ARCHITECTURES}")

find_library(CUDART_LIBRARY cudart ${CMAKE_CUDA_IMPLICIT_LINK_DIRECTORIES})



set(structures_INCLUDE_DIR
    "libs"
    "libs/spdlog/include"
    "libs/glfw/include"
    "libs/gli"
    "src"
)

file(GLOB_RECURSE shaders_src CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.glsl  
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.comp  
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.rgen  
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.rahit  
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.rchit  
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.rmiss  
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.vert  
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.frag  
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.h  
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.rcall
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.rint
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders PREFIX Shaders FILES ${shaders_src})
file(GLOB_RECURSE FRAMEWORK_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/Framework/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/Framework/*.h ${CMAKE_CURRENT_SOURCE_DIR}/src/Framework/*.hpp)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src/Framework PREFIX Framework FILES ${FRAMEWORK_SRC})
file(GLOB_RECURSE RT_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/RayTracer/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/RayTracer/*.h ${CMAKE_CURRENT_SOURCE_DIR}/src/RayTracer/*.cu)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src/RayTracer PREFIX RayTracer FILES ${RT_SRC})
file(GLOB_RECURSE LIB_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libs/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/libs/*.h ${CMAKE_CURRENT_SOURCE_DIR}/libs/*.c ${CMAKE_CURRENT_SOURCE_DIR}/libs/*.hpp ${CMAKE_CURRENT_SOURCE_DIR}/libs/*.cc)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/libs PREFIX Libs FILES ${LIB_SRC})

include_directories(${Vulkan_INCLUDE_DIR};${structures_INCLUDE_DIR}; ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})
if(WIN32)
    message("-- Adding vulkan library lib to search paths for the linker to find spirv libraries")
    get_filename_component(vulkan_lib_folder ${Vulkan_LIBRARIES} DIRECTORY)
    link_directories(${vulkan_lib_folder})
endif()

add_compile_definitions(TINYEXR_USE_MINIZ)  # enables usage of miniz for opening exr images
add_compile_definitions(USE_SHADERC)        # enables shader compilation via shaderc
# cuda_add_executable(Lumen ${src_files} ${shaders_src})
set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS};-gencode arch=compute_86,code=sm_86)
cuda_add_executable(Lumen ${src_files} ${shaders_src} ${cuda_src})
# cuda_add_executable(Network ${CMAKE_CURRENT_SOURCE_DIR}/src/RayTracer/Network.cu)
if(WIN32)
    target_link_libraries(Lumen PRIVATE Vulkan::Vulkan glfw volk shaderc_shared glm
    $<$<CONFIG:Debug>:spirv-cross-cored> $<$<NOT:$<CONFIG:Debug>>:spirv-cross-core>
    $<$<CONFIG:Debug>:spirv-cross-glsld> $<$<NOT:$<CONFIG:Debug>>:spirv-cross-glsl>
    cudart tiny-cuda-nn)
else()
    target_link_libraries(Lumen PRIVATE Vulkan::Vulkan glfw volk shaderc_shared spirv-cross-core spirv-cross-glsl glm cudart)
endif()

if(MSVC)
	target_compile_options(Lumen PRIVATE "/MP")
	set_target_properties(Lumen PROPERTIES
    				      VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                          CUDA_SEPARABLE_COMPILATION ON
                          CUDA_ARCHITECTURES 86)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Lumen)
endif()

target_compile_features(Lumen PRIVATE cxx_std_20)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
