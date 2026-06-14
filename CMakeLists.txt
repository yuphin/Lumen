cmake_minimum_required(VERSION 3.15)
project(Lumen VERSION 0.1.0)

include(CTest)
enable_testing()

find_package(Vulkan REQUIRED)
find_package(Threads REQUIRED)

set(VOLK_HEADERS_ONLY ON CACHE BOOL "Use volk as headers-only")

set(src_files)
add_subdirectory(src)
add_subdirectory(libs)

set(structures_INCLUDE_DIR
    "libs"
    "libs/vma/include"
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


add_executable(Lumen ${src_files} ${shaders_src})

target_compile_definitions(Lumen PRIVATE
    TINYEXR_USE_MINIZ
    $<$<CONFIG:Debug>:USE_VALIDATION_LAYERS>
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:_RELEASE>
)

# Add precompiled headers
target_precompile_headers(Lumen PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/LumenPCH.h>")
set_source_files_properties(${LIB_SRC} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/src/external.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

target_include_directories(Lumen PRIVATE ${Vulkan_INCLUDE_DIR};${structures_INCLUDE_DIR})
if(WIN32)
    message("-- Adding vulkan library lib to search paths for the linker to find spirv libraries")
    get_filename_component(vulkan_lib_folder ${Vulkan_LIBRARIES} DIRECTORY)
    get_filename_component(vulkan_sdk_folder ${vulkan_lib_folder} DIRECTORY)
    target_link_directories(Lumen PRIVATE ${vulkan_lib_folder})
    target_link_libraries(Lumen PRIVATE Vulkan::Vulkan volk shaderc_shared GPUOpen::VulkanMemoryAllocator user32
        $<$<CONFIG:Debug>:spirv-cross-c-sharedd> $<$<NOT:$<CONFIG:Debug>>:spirv-cross-c-shared>)
    add_custom_command(TARGET Lumen POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<IF:$<CONFIG:Debug>,${vulkan_sdk_folder}/Bin/spirv-cross-c-sharedd.dll,${vulkan_sdk_folder}/Bin/spirv-cross-c-shared.dll>"
            "$<TARGET_FILE_DIR:Lumen>"
        VERBATIM)
    target_compile_definitions(Lumen PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    find_package(X11 REQUIRED)
    target_link_libraries(Lumen PRIVATE Vulkan::Vulkan volk shaderc_shared spirv-cross-c-shared GPUOpen::VulkanMemoryAllocator X11::X11)
endif()
target_link_libraries(Lumen PRIVATE Threads::Threads)
target_compile_definitions(Lumen PRIVATE
    $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
    $<$<NOT:$<PLATFORM_ID:Windows>>:VK_USE_PLATFORM_XLIB_KHR>
)
# target_compile_options(Lumen PRIVATE
#     $<$<CXX_COMPILER_ID:MSVC>:/W4>
#     $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

if(MSVC)
	target_compile_options(Lumen PRIVATE "/MP")
    # Vulkan SDK debug SPIRV-Cross libraries reference compilation PDBs that
    # are not distributed with the SDK.
    target_link_options(Lumen PRIVATE "$<$<CONFIG:Debug>:/IGNORE:4099>")
    # For MSVC compilation diagnostics
    # target_compile_options(Lumen PRIVATE /d1reportTime)
    # For struct layouts
    # target_compile_options(Lumen PRIVATE /d1reportAllClassLayout)
	set_target_properties(Lumen PROPERTIES
    				      VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Lumen)
endif()

target_compile_features(Lumen PRIVATE cxx_std_20)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
