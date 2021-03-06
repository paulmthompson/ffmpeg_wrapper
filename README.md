# ffmpeg_wrapper
Convenience functions and classes in c++ for interfacing with FFMPEG libraries. 

## Motivation

I routinely use FFMPEG to decode video files and visualize the frames as part of some larger processing framework, or as an encoding mechanisms where data from a camera(s) is being acquired and needs to be written to disk. I work with high speed cameras (>500 fps), so performance can be critical. FFMPEG is great for this purpose, but I have typically been using the application with pipes, rather than interfacing its C/C++ libraries with my code. This seems far from optimal.

The methods included in this package are my attempt to make relatively simple encoding and decoding objects that an be incorporated into other frameworks.

## Video Decoder

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
