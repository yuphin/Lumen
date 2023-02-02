file(GLOB SRC
    *.cpp
    *.c
    tinyxml2/*.cpp
    mitsuba_parser/*.cpp
    imgui/*.cpp
    glslc/*.cc
)
set(Lumen_SRC ${Lumen_SRC} ${SRC} PARENT_SCOPE)

add_subdirectory(SPIRV-Cross)
add_subdirectory(SPIRV-Headers)
add_subdirectory(SPIRV-Tools)
add_subdirectory(glslang)
set(SHADERC_SKIP_INSTALL 1)
set(SHADERC_SKIP_TESTS 1)
set(SHADERC_SKIP_EXAMPLES 1)
set(SHADERC_SKIP_COPYRIGHT_CHECK 1)
add_subdirectory(shaderc)