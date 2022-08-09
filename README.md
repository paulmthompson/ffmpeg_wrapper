# ffmpeg_wrapper
Convenience functions and classes in c++ for interfacing with FFMPEG libraries. 

## Motivation

I routinely use FFMPEG to decode video files and visualize the frames as part of some larger processing framework, or as an encoding mechanisms where data from a camera(s) is being acquired and needs to be written to disk. I work with high speed cameras (>500 fps), so performance can be critical. FFMPEG is great for this purpose, but I have typically been using the application with pipes, rather than interfacing its C/C++ libraries with my code. This seems far from optimal.

The methods included in this package are my attempt to make relatively simple encoding and decoding objects that an be incorporated into other frameworks.

## Installation

I have compiled on both Linux and Windows. The only dependancy is a version of ffmpeg with nvidia CUDA support. I use vcpkg (https://vcpkg.io/en/index.html) to install the package **ffmpeg[nvcodec]**. 
  
On linux, you must use a version of ffmpeg that is a dynamic library. Follow this guide here to compile a custom triplet:  
https://vcpkg.readthedocs.io/en/latest/examples/overlay-triplets-linux-dynamic/

## How to use in a project

You will need to make sure that the dependacy for ffmpeg is met as above. Set the cmake variable ffmpeg_wrapper_DIR to the install path. Then import and link the target with
```cmake
find_package(ffmpeg_wrapper CONFIG)

target_link_libraries(myproject PRIVATE ffmpeg_wrapper::ffmpeg_wrapper)
```

## Video Decoder

The VideoDecoder class is expected to be used as part of a larger project to quickly grab frames from compressed videos. I have used this as an interface to load and display frames from mp4 videos in a GUI. 

## Video Encoder

The VideoEncoder class is meant to facilitate encoding and saving raw camera data on the fly. It currently uses the h264_nvenc codec to save mp4 files. An implementation of this with a GUI for visualization can be found here: <br>
https://github.com/paulmthompson/CameraViewer

My current profiling suggests that during 500 fps white noise video acquisition, about ~50% of the available processing window is utilized. Interestingly, the rate limiting step appears to be the CPU conversion of raw camera data to the NV12 format expected by the GPU encoder, and this is about a factor of 2-4x more time consuming than the video encoding and saving itself. My current TODO list includes:

- [ ] Helper functions to set video parameters like save path, height, width etc
- [ ] Producer / Consumer threading for the scaling step

## Resources

This project would not have been possible without the excellent talk given by Matt Szatmary:
https://www.youtube.com/watch?v=fk1bxHi6iSI
and graciously making his source code available which has been incorporated into part of this project:
https://github.com/szatmary/libavinc

The FFMPEG tutorial here is also very helpful:
https://github.com/leandromoreira/ffmpeg-libav-tutorial
