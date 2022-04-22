#ifndef VIDEODECODER_H
#define VIDEODECODER_H

#include "libavinc.hpp"
#include <string>
#include <vector>

#if defined _WIN32 || defined __CYGWIN__
	#define DLLOPT __declspec(dllexport)
#else
	#define DLLOPT __attribute__((visibility("default")))
#endif

namespace ffmpeg_wrapper {

class DLLOPT VideoDecoder {

public:
    VideoDecoder();
    VideoDecoder(std::string filename);
    void createMedia(std::string filename);
    std::vector<uint8_t> getFrame(int frame_id, bool frame_by_frame = false);

    int getFrameCount() const {return frame_count;}
    int getWidth() const {return width;}
    int getHeight() const {return height;}

private:
    libav::AVFormatContext media; //This is a unique_ptr
    libav::AVPacket pkt; //This is a unique ptr

    int frame_count;
    long long last_decoded_frame;
    int width;
    int height;
    void yuv420togray8(std::shared_ptr<::AVFrame>& frame,std::vector<uint8_t>&);
};

}

#endif // VIDEODECODER_H
