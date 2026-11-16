cmake_minimum_required(VERSION 3.20)
project(imanim LANGUAGES CXX)

find_package(imgui CONFIG REQUIRED)

add_library(imanim STATIC im_anim.cpp)
target_compile_features(imanim PRIVATE cxx_std_17)
target_link_libraries(imanim PRIVATE imgui::imgui)

if(MSVC)
    target_compile_options(imanim PRIVATE /utf-8)
    target_compile_definitions(imanim PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

install(TARGETS imanim ARCHIVE DESTINATION lib)
install(FILES im_anim.h DESTINATION include)
