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
