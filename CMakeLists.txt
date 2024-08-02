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
    "libs/vma/include"
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
file(GLOB_RECURSE RT_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/RayTracer/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/RayTracer/*.h)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src/RayTracer PREFIX RayTracer FILES ${RT_SRC})
file(GLOB_RECURSE LIB_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libs/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/libs/*.h ${CMAKE_CURRENT_SOURCE_DIR}/libs/*.c ${CMAKE_CURRENT_SOURCE_DIR}/libs/*.hpp ${CMAKE_CURRENT_SOURCE_DIR}/libs/*.cc)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/libs PREFIX Libs FILES ${LIB_SRC})



add_compile_definitions(TINYEXR_USE_MINIZ)  # enables usage of miniz for opening exr images
add_compile_definitions(USE_SHADERC)        # enables shader compilation via shaderc
add_executable(Lumen ${src_files} ${shaders_src})

# Add precompiled headers
target_precompile_headers(Lumen PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/LumenPCH.h>")
set_source_files_properties(${LIB_SRC} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

target_include_directories(Lumen PRIVATE ${Vulkan_INCLUDE_DIR};${structures_INCLUDE_DIR})
if(WIN32)
    message("-- Adding vulkan library lib to search paths for the linker to find spirv libraries")
    get_filename_component(vulkan_lib_folder ${Vulkan_LIBRARIES} DIRECTORY)
    target_link_directories(Lumen PRIVATE ${vulkan_lib_folder})
    target_link_libraries(Lumen PRIVATE Vulkan::Vulkan glfw volk shaderc_shared glm GPUOpen::VulkanMemoryAllocator
    $<$<CONFIG:Debug>:spirv-cross-cored> $<$<NOT:$<CONFIG:Debug>>:spirv-cross-core>
    $<$<CONFIG:Debug>:spirv-cross-glsld> $<$<NOT:$<CONFIG:Debug>>:spirv-cross-glsl>)
    target_compile_definitions(Lumen PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_link_libraries(Lumen PRIVATE Vulkan::Vulkan glfw volk shaderc_shared spirv-cross-core spirv-cross-glsl glm GPUOpen::VulkanMemoryAllocator)
endif()

if(MSVC)
	target_compile_options(Lumen PRIVATE "/MP")
	set_target_properties(Lumen PROPERTIES
    				      VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Lumen)
endif()

target_compile_features(Lumen PRIVATE cxx_std_20)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
