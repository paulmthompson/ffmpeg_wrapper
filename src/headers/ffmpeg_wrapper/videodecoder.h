#ifndef VIDEODECODER_H
#define VIDEODECODER_H

#include "libavinc/libavinc.hpp"

#include "libavformat/avformat.h"
#include "libavutil/frame.h"

#include <boost/circular_buffer.hpp>

#include <memory>
#include <stdint.h>
#include <string>
#include <vector>
#include <unordered_map>

#if defined _WIN32 || defined __CYGWIN__
#define DLLOPT __declspec(dllexport)
#else
#define DLLOPT __attribute__((visibility("default")))
#endif

namespace ffmpeg_wrapper {

struct FrameBufferElement {
    int frame_id;
    libav::AVFrame frame;
};

class FrameBuffer {
public:
    FrameBuffer() = default;
    void buildFrameBuffer(int buf_size);
    void addFrametoBuffer(libav::AVFrame frame, int pos);
    bool isFrameInBuffer(int frame);
    libav::AVFrame getFrameFromBuffer(int frame);

    void setVerbose(bool verbose) {
        _verbose = verbose;
    }


private:
    boost::circular_buffer<FrameBufferElement> _frame_buf;
    bool _enable{true};
    bool _verbose{false};
};

class DLLOPT VideoDecoder {

public:
    VideoDecoder();
    VideoDecoder(std::string const & filename);
    void createMedia(std::string const & filename);

    /*!
    *
    * Future improvements could return different output types (such as 16-bit uints)
    *
    * @param desired_frame Frame we wish to seek to
    * @param isFrameByFrameMode We wish to see to the desired frame by decoding each frame in between
    * rather than seeking to the next keyframe.
    * @return Image corresponding to the decoded desired_frame
    */
    std::vector<uint8_t> getFrame(int const desired_frame, bool isFrameByFrameMode = false);

    int getFrameCount() const { return _frame_count; }
    int getWidth() const { return _width; }
    int getHeight() const { return _height; }
    std::vector<int64_t> getKeyFrames() const { return _i_frames; }

    /**
     *
     * Find the nearest keyframe to frame_id
     *
     * @param frame_id
     * @return
     */
    int64_t nearest_iframe(int64_t frame_id);

    void setVerbose(bool verbose) {
        _verbose = verbose;
        _frame_buf->setVerbose(verbose);
    }


    enum OutputFormat {
        Gray8,
        ARGB,
    };

    void setFormat(OutputFormat format) {
        _format = format;
    }

private:
    libav::AVFormatContext _media;//This is a unique_ptr
    libav::AVPacket _pkt;         //This is a unique ptr

    int _frame_count{0};
    long long _last_decoded_frame{0};
    long long _last_key_frame{0};
    int _width{0};
    int _height{0};
    int _fps_num{0};
    int _fps_denom{0};

    OutputFormat _format{OutputFormat::Gray8};

    bool _verbose{false};

    bool _last_packet_decoded{false};

    std::vector<uint64_t> _pts;// We keep a vector of every pts value corresponding to each frame. in AVStream->time_base units;
    // Fast lookup from pts -> index in _pts; helps avoid O(n) searches and issues with partially non-monotonic PTS.
    std::unordered_map<uint64_t, int64_t> _pts_index;
    std::vector<int64_t> _i_frames;
    std::vector<uint64_t> _pkt_durations;

    std::vector<uint64_t> _i_frame_pts;

    std::unique_ptr<FrameBuffer> _frame_buf;

    void _convertFrameToOutputFormat(::AVFrame * frame, std::vector<uint8_t> & output) const;
    int _getFormatBytes() const;
    void _togray8(::AVFrame * frame, std::vector<uint8_t> & output) const;
    void _torgb32(::AVFrame * frame, std::vector<uint8_t> & output) const;

    int64_t _findFrameByPts(uint64_t pts);

    uint64_t _getDuration() const { return _media->duration; }   // This is in AV_TIME_BASE (1000000) fractional seconds
    uint64_t _getStartTime() const { return _media->start_time; }// This is in AV_TIME_BASE (1000000) fractional seconds

    void _seekToFrame(int const frame, bool keyframe = false);
};

template<typename T>
int find_buffer_size(std::vector<T> & vec);
}// namespace ffmpeg_wrapper

#endif// VIDEODECODER_H
