set(imgui_src
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_demo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_draw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_tables.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_widgets.cpp"
)

file(GLOB tinyxml_src
    "tinyxml2/tinyxml2.cpp"
)
file(GLOB offsetAllocator_src
    "offsetAllocator/offsetAllocator.cpp"
)
file(GLOB miniz_src
    "miniz.c"
)

add_subdirectory(volk SYSTEM)
add_subdirectory(vma)

set(src_files "${src_files};${imgui_src};${mitsuba_src};${tinyxml_src};${miniz_src};${offsetAllocator_src}" PARENT_SCOPE)
