cmake_minimum_required(VERSION 3.24)

#[[
Then we can test an out-of-source build, meaning that ffmpeg_wrapper source code will be included in that project.
]]
add_subdirectory(video-encoder-tests)

#[[
ffmpeg_wrapper is often going to be included as a part of other projects. We want to make sure that it builds with some of these different ways of inclusion.
First we can test an in-source build, meaning that ffmpeg_wrapper source code will be included in that project.
]]
add_subdirectory(in-source-build)
