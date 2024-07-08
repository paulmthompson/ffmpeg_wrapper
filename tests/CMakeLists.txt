cmake_minimum_required(VERSION 3.24)

#[[
ffmpeg_wrapper is often going to be included as a part of other projects. We want to make sure that it builds with some of these different ways of inclusion.
First we can test an in-source build, meaning that ffmpeg_wrapper source code will be included in that project.
]]
add_subdirectory(in-source-build)

Include(FetchContent)

FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.4 # or a later release
)

FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

include(Catch)

#[[
Then we can test an out-of-source build, meaning that ffmpeg_wrapper source code will be included in that project.
]]
add_subdirectory(video-encoder-tests)

add_subdirectory(video-decoder-tests)