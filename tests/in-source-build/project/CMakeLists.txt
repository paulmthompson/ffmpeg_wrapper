cmake_minimum_required(VERSION 3.24)

project(InSourceBuild)

set(BUILD_TESTING OFF CACHE INTERNAL "")
add_subdirectory(${ffmpeg_wrapper_source})

add_executable(InSourceBuild main.cpp)  

target_link_libraries(InSourceBuild PRIVATE ffmpeg_wrapper::ffmpeg_wrapper)

