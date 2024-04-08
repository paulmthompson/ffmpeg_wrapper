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

class DLLOPT VideoEncoder {

public:

    VideoEncoder();
    VideoEncoder(int width, int height, int fps);

    void createContext(int width, int height, int fps);

    enum INPUT_PIXEL_FORMAT {NV12, GRAY8, RGB0};
    void set_pixel_format(INPUT_PIXEL_FORMAT pixel_fmt);
    void openFile();
    void closeFile();
    int writeFrameGray8(std::vector<uint8_t>& input_data);
    void writeFrameRGB0(std::vector<uint32_t>& input_data);

    int getWidth() const {return _width;}
    int getHeight() const {return _height;}

    void setSavePath(std::string full_path);

    void enterDrainMode() {
        _flush_state = true;
        libav::encode_enter_drain_mode(_media, _codecCtx);
    }

    // The verbose state will give us lots of info as different steps are run.
    void setVerbose(bool verbose) {
        _verbose = verbose;
    }

    

private:
    libav::AVFormatContext _media;
    libav::AVCodecContext _codecCtx;
    //libav::AVStream;
    libav::AVFrame _frame; //This frame has the same format as the camera
    libav::AVFrame _frame_nv12; // This frame must be compatible with hardware encoding (nv12). frame will be scaled to frame_2.

    int _frame_count;
    int _width;
    int _height;
    int _fps;
    bool _hardware_encode;
    bool _flush_state;

    std::string _encoder_name;
    std::string _file_path;
    std::string _file_name;

    bool _verbose;

};

}


#endif // VIDEODECODER_H
