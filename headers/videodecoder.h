#ifndef VIDEODECODER_H
#define VIDEODECODER_H

#include "libavinc/libavinc.hpp"
#include <string>
#include <vector>

#if defined _WIN32 || defined __CYGWIN__
	#define DLLOPT __declspec(dllexport)
#else
	#define DLLOPT __attribute__((visibility("default")))
#endif

namespace ffmpeg_wrapper {

class FrameBuffer {
public:
    FrameBuffer();
    void buildFrameBuffer(int buf_size, libav::AVFrame frame);
    void addFrametoBuffer(libav::AVFrame& frame, int pos, int last_key_frame);
    bool isFrameInBuffer(int frame);
    libav::AVFrame getFrameFromBuffer(int frame);

private:
    std::vector<libav::AVFrame> frame_buf;
    std::vector<int64_t> frame_buf_id;
};

class DLLOPT VideoDecoder {

public:
    VideoDecoder();
    VideoDecoder(const std::string& filename);
    void createMedia(const std::string& filename);
    std::vector<uint8_t> getFrame(const int frame_id, bool frame_by_frame = false);

    int getFrameCount() const {return frame_count;}
    int getWidth() const {return width;}
    int getHeight() const {return height;}

private:
    libav::AVFormatContext media; //This is a unique_ptr
    libav::AVPacket pkt; //This is a unique ptr

    int frame_count;
    long long last_decoded_frame;
    long long last_key_frame;
    int width;
    int height;
    int fps_num;
    int fps_denom;
    void yuv420togray8(std::shared_ptr<::AVFrame>& frame,std::vector<uint8_t>&);

    int64_t nearest_iframe(int64_t frame_id);
    int64_t find_frame_by_pts(int64_t pts);

    int64_t getDuration() const {return media->duration;}
    int64_t getStartTime() const {return media->start_time;}
    std::vector<int64_t> pts; // We keep a vector of every pts value corresponding to each frame.
    std::vector<int64_t> i_frames;
    std::vector<int64_t> pkt_durations;

    std::unique_ptr<FrameBuffer> frame_buf;
};

template <typename T>
int find_buffer_size(std::vector<T>& vec);
}

#endif // VIDEODECODER_H
