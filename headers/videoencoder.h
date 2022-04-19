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

enum INPUT_PIXEL_FORMAT {NV12, GRAY8};

class DLLOPT VideoEncoder {

public:
    VideoEncoder();
    VideoEncoder(int width, int height, int fps);
    void createContext(int width, int height, int fps);
    void set_pixel_format(INPUT_PIXEL_FORMAT pixel_fmt);
    void openFile(std::string filename);
    void closeFile();
    void writeFrameGray8(std::vector<uint8_t>& input_data);

    int getWidth() const;
    int getHeight() const;

private:
    libav::AVFormatContext media;
    libav::AVCodecContext codecCtx; 
    //libav::AVStream;
    libav::AVFrame frame; 

    int frame_count;
    int width;
    int height;
    int fps;
    FILE* file_out;
};



#endif // VIDEODECODER_H
