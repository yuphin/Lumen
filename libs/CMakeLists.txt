file(GLOB glslc_src
    "glslc/file_includer.cc"
)

file(GLOB libshaderc_util
    "libshaderc_util/file_finder.cc"
    "libshaderc_util/io_shaderc.cc"
)

file(GLOB imgui_src
    "imgui/*.cpp"
)

file(GLOB mitsuba_src
    "mitsuba_parser/*.cpp"
)

file(GLOB tinyxml_src
    "tinyxml2/tinyxml2.cpp"
)
file(GLOB miniz_src
    "miniz.c"
)

include(CMakeDependentOption)
cmake_dependent_option(GLFW_BUILD_WIN32 "Build support for Win32" ON "WIN32" OFF)
cmake_dependent_option(GLFW_BUILD_COCOA "Build support for Cocoa" ON "APPLE" OFF)
cmake_dependent_option(GLFW_BUILD_X11 "Build support for X11" ON "UNIX;NOT APPLE" OFF)
cmake_dependent_option(GLFW_BUILD_WAYLAND "Build support for Wayland"
    "${GLFW_USE_WAYLAND}" "UNIX;NOT APPLE" OFF)
add_subdirectory(glfw)
add_subdirectory(volk SYSTEM)
add_subdirectory(glm)

set(src_files "${src_files};${glslc_src};${libshaderc_util};${imgui_src};${mitsuba_src};${tinyxml_src};${miniz_src}" PARENT_SCOPE)
