cmake_minimum_required(VERSION 3.24)

project(InSourceBuild)

#[[
Manifest mode from VCPKG will make all dependancies specified in vcpkg.json be associated and installed
with this particular project
]]
set(VCPKG_MANIFEST_MODE ON)

#[[
We are going to add ffmpeg_wrapper as a subdirectory and link to an executable that uses it

]]
set(BUILD_TESTING OFF CACHE INTERNAL "")
add_subdirectory(${ffmpeg_wrapper_source}
    ${CMAKE_CURRENT_BINARY_DIR}/ffmpeg_wrapper)

add_executable(InSourceBuild main.cpp)  

target_link_libraries(InSourceBuild PRIVATE ffmpeg_wrapper::ffmpeg_wrapper)

