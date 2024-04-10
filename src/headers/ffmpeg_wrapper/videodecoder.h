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
    void resetKeyFrame(const int frame) {
        _keyframe = frame;
        std::fill(_frame_buf_id.begin(), _frame_buf_id.end(), -1);
        }
    void addFrametoBuffer(libav::AVFrame& frame, int pos);
    bool isFrameInBuffer(int frame);
    libav::AVFrame getFrameFromBuffer(int frame);

    void setVerbose(bool verbose) {
        _verbose = verbose;
    }


private:
    std::vector<libav::AVFrame> _frame_buf;
    std::vector<int64_t> _frame_buf_id;
    int _keyframe; // The first index of each buffer is a keyframe from which we have decoded forward
    bool _enable;
    bool _verbose;

};

class DLLOPT VideoDecoder {

public:
    VideoDecoder();
    VideoDecoder(const std::string& filename);
    void createMedia(const std::string& filename);
    std::vector<uint8_t> getFrame(const int frame_id, bool isFrameByFrameMode = false);

    int getFrameCount() const {return _frame_count;}
    int getWidth() const {return _width;}
    int getHeight() const {return _height;}
    std::vector<int64_t> getKeyFrames() const {return _i_frames;}
    int64_t nearest_iframe(int64_t frame_id);

    void setVerbose(bool verbose) {
        _verbose = verbose;
        _frame_buf->setVerbose(verbose);
    }

    enum OutputFormat
    {
        Gray8
    };

private:
    libav::AVFormatContext _media; //This is a unique_ptr
    libav::AVPacket _pkt; //This is a unique ptr

    int _frame_count;
    long long _last_decoded_frame;
    long long _last_key_frame;
    int _width;
    int _height;
    int _fps_num;
    int _fps_denom;
    OutputFormat _format;
    void _yuv420togray8(std::shared_ptr<::AVFrame>& frame, std::vector<uint8_t>& output);

    bool _verbose;

    bool _last_packet_decoded;

    int64_t _findFrameByPts(uint64_t pts);

    uint64_t _getDuration() const {return _media->duration;} // This is in AV_TIME_BASE (1000000) fractional seconds
    uint64_t _getStartTime() const {return _media->start_time;} // This is in AV_TIME_BASE (1000000) fractional seconds

    void _seekToFrame(const int frame, bool keyframe = false);

    std::vector<uint64_t> _pts; // We keep a vector of every pts value corresponding to each frame. in AVStream->time_base units;
    std::vector<int64_t> _i_frames;
    std::vector<uint64_t> _pkt_durations;

    std::vector<uint64_t> _i_frame_pts;

    std::unique_ptr<FrameBuffer> _frame_buf;
};

template <typename T>
int find_buffer_size(std::vector<T>& vec);
}

#endif // VIDEODECODER_H
