#include <iostream>

#include "libavinc.hpp"
#include "videodecoder.h"

#if defined _WIN32 || defined __CYGWIN__
	#define DLLOPT __declspec(dllexport)
#else
	#define DLLOPT __attribute__((visibility("default")))
#endif


void DLLOPT say_hello(){
    std::cout << "Hello, from ffmpeg_wrapper!\n";
}
