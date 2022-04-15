# ffmpeg_wrapper
Convenience functions and classes in c++ for interfacing with FFMPEG libraries. 

## Motivation

I routinely use FFMPEG to decode video files and visualize the frames as part of some larger processing framework, or as an encoding mechanisms where data from a camera(s) is being acquired and needs to be written to disk. I work with high speed cameras (>500 fps), so performance can be critical. FFMPEG is great for this purpose, but I have typically been using the application with pipes, rather than interfacing its C/C++ libraries with my code. This seems far from optimal.

The methods included in this package are my attempt to make relatively simple encoding and decoding objects that an be incorporated into other frameworks.

## Video Decoder

## Video Encoder

## Resources

This project would not have been possible without the excellent talk given by Matt Szatmary:
https://www.youtube.com/watch?v=fk1bxHi6iSI
and graciously making his source code available which has been incorporated into part of this project:
https://github.com/szatmary/libavinc

The FFMPEG tutorial here is also very helpful:
https://github.com/leandromoreira/ffmpeg-libav-tutorial
