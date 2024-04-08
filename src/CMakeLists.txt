
#Create Library
set(Sources
        videodecoder.cpp
        videoencoder.cpp
)

set(headers
        headers/ffmpeg_wrapper/videodecoder.h
        headers/ffmpeg_wrapper/videoencoder.h
)

#[[
ffmpeg_wrapper is a shared library

I am also creating an alias with the format that has the same structure as the library when it is imported as a target from find_package
This is recommended practice so that the library can be linked to with ffmpeg_wrapper::ffmpeg_wrapper as an in-source build or
as a package installed with find_package.
Professional CMake 17.4
]]
add_library(ffmpeg_wrapper SHARED ${Sources} ${Headers})
add_library(ffmpeg_wrapper::ffmpeg_wrapper ALIAS ffmpeg_wrapper)

target_link_libraries(ffmpeg_wrapper PUBLIC libavinc)

#[[
Here I link the include directories for ffmpeg_wrapper.
I add both headers and headers/ffmpeg_wrapper so that they can be included with both ffmpeg_wrapper/video_encoder.h and video_encoder.h
The BUILD_INTERFACE is used to specify the location of in-source builds

Because we later install the target with the "INCLUDES DESTINATION" section, we do NOT need to specify the INSTALL_INTERFACE
generator expression here because it would be redundant
The include directories will be installed in CMAKE_INSTALL_INCLUDEDIR, so this is the location that will be used for installed builds
Professional CMake 26.10

We also want to include the libavinc include directory, also declared PUBLIC for reasons seen above for linking the library
]]
target_include_directories(ffmpeg_wrapper
        PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/headers/ffmpeg_wrapper>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/headers>"

        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/dependencies/libavinc/include>"
)

#[[
Public headers are installed with their relative paths removed
]]
set(Public_Headers
        headers/ffmpeg_wrapper/videodecoder.h
        headers/ffmpeg_wrapper/videoencoder.h
)
set_target_properties(ffmpeg_wrapper PROPERTIES PUBLIC_HEADER "${Public_Headers}")

#[[
In general, it is not good practice to export all symbols from a shared library
Specific class and functions can be exported using the __declspec(dllexport) keywords in the header file
]]
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
