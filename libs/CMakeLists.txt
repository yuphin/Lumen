file(GLOB glfw_src
    "glfw/src/*.c"
)

file(GLOB imgui_src
    "imgui/*.cpp"
)

file(GLOB mitsuba_src
    "mitsuba_parser/*.cpp"
)

#file(GLOB extension_src
#    "*.cpp"
#    "*.c"
#)

file(GLOB tinyxml_src
    "tinyxml2/tinyxml2.cpp"
)

include(CMakeDependentOption)
cmake_dependent_option(GLFW_BUILD_WIN32 "Build support for Win32" ON "WIN32" OFF)
cmake_dependent_option(GLFW_BUILD_COCOA "Build support for Cocoa" ON "APPLE" OFF)
cmake_dependent_option(GLFW_BUILD_X11 "Build support for X11" ON "UNIX;NOT APPLE" OFF)
cmake_dependent_option(GLFW_BUILD_WAYLAND "Build support for Wayland"
    "${GLFW_USE_WAYLAND}" "UNIX;NOT APPLE" OFF)
add_subdirectory(glfw)
set(src_files "${src_files};${imgui_src};${mitsuba_src};${extension_src};${tinyxml_src}" PARENT_SCOPE)
