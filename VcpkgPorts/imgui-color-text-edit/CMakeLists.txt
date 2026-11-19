cmake_minimum_required(VERSION 3.20)
project(ImGuiColorTextEdit CXX)

find_package(imgui CONFIG REQUIRED)

add_library(imgui-color-text-edit STATIC TextEditor.cpp)
target_compile_features(imgui-color-text-edit PRIVATE cxx_std_20)
target_link_libraries(imgui-color-text-edit PRIVATE imgui::imgui)

if(MSVC)
    target_compile_options(imgui-color-text-edit PRIVATE /utf-8)
endif()

install(TARGETS imgui-color-text-edit ARCHIVE DESTINATION lib)
install(FILES TextEditor.h DESTINATION include/ImGuiColorTextEdit)
