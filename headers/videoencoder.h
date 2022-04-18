#ifndef VIDEOENCODER_H
#define VIDEOENCODER_H

#include "libavinc.hpp"
#include <string>
#include <vector>
#include <stdio.h>

#if defined _WIN32 || defined __CYGWIN__
	#define DLLOPT __declspec(dllexport)
#else
	#define DLLOPT __attribute__((visibility("default")))
#endif

enum INPUT_PIXEL_FORMAT {NV12};

class DLLOPT VideoEncoder {

public:
    VideoEncoder();
    VideoEncoder(int width, int height, int fps);
    void createContext();
    void set_pixel_format(INPUT_PIXEL_FORMAT pixel_fmt);
    void openFile(std::string filename);
    void closeFile();
    void writeFrame(std::vector<uint8_t>& input_data);

    int getWidth() const;
    int getHeight() const;

private:
    libav::AVCodecContext media; //This is a unique_ptr
    libav::AVFrame frame; //This is a unique ptr

    int frame_count;
    int width;
    int height;
    int fps;
    FILE* file_out;
};



#endif // VIDEODECODER_H
