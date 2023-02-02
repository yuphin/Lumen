file(GLOB_RECURSE SRC
    *.cpp
    Framework/*.cpp
    RayTracer/*.cpp
)
set(Lumen_SRC ${Lumen_SRC} ${SRC} PARENT_SCOPE)