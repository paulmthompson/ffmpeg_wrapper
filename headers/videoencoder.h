#ifndef VIDEOENCODER_H
#define VIDEOENCODER_H

#include "libavinc/libavinc.hpp"
#include <string>
#include <vector>
#include <stdio.h>

#if defined _WIN32 || defined __CYGWIN__
	#define DLLOPT __declspec(dllexport)
#else
	#define DLLOPT __attribute__((visibility("default")))
#endif

namespace ffmpeg_wrapper {

enum INPUT_PIXEL_FORMAT {NV12, GRAY8};

class DLLOPT VideoEncoder {

public:

    VideoEncoder();
    VideoEncoder(int width, int height, int fps);

    void createContext(int width, int height, int fps);

    void set_pixel_format(INPUT_PIXEL_FORMAT pixel_fmt);
    void openFile();
    void closeFile();
    void writeFrameGray8(std::vector<uint8_t>& input_data);

    int getWidth() const {return width;}
    int getHeight() const {return height;}

    void setSavePath(std::string full_path);

private:
    libav::AVFormatContext media;
    libav::AVCodecContext codecCtx; 
    //libav::AVStream;
    libav::AVFrame frame; //This frame has the same format as the camera
    libav::AVFrame frame_nv12; // This frame must be compatible with hardware encoding (nv12). frame will be scaled to frame_2.

    int frame_count;
    int width;
    int height;
    int fps;

    std::string encoder_name;
    std::string file_path;
    std::string file_name;

};

}


#endif // VIDEODECODER_H
