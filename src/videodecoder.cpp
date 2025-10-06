#include "videodecoder.h"

#include "libavinc/libavinc.hpp"

#include "libavcodec/packet.h"
#include "libavutil/avutil.h"
#include "libavutil/pixfmt.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace ffmpeg_wrapper {

void FrameBuffer::buildFrameBuffer(int buf_size) {

    _frame_buf.clear();
    _frame_buf = boost::circular_buffer<FrameBufferElement>(buf_size);
}

void FrameBuffer::addFrametoBuffer(libav::AVFrame frame, int pos) {

    if (_enable) {
        //Check if the position is already in the buffer
        if (isFrameInBuffer(pos)) {
            if (_verbose) {
                std::cout << "Frame " << pos << " is already in the buffer" << std::endl;
            }
        } else {
            _frame_buf.push_back(FrameBufferElement{pos, std::move(frame)});
        }
    }
}

bool FrameBuffer::isFrameInBuffer(int frame) {
    auto element = std::find_if(
            _frame_buf.begin(), _frame_buf.end(),
            [&frame](FrameBufferElement const & x) { return x.frame_id == frame; });
    if (element == _frame_buf.end()) {
        return false;
    } else {
        return true;
    }
}

libav::AVFrame FrameBuffer::getFrameFromBuffer(int frame) {
    auto element = std::find_if(
            _frame_buf.begin(), _frame_buf.end(),
            [&frame](FrameBufferElement const & x) { return x.frame_id == frame; });

    return (*element).frame;
}

VideoDecoder::VideoDecoder() {
    constexpr size_t kReservePts = 500000ull;
    _pts.reserve(kReservePts);

    _frame_buf = std::make_unique<FrameBuffer>();
}

VideoDecoder::VideoDecoder(std::string const & filename) {
    createMedia(filename);
}

/*

There are multiple, sometimes redundant, pieces of information stored in the larger AVFormatContext structure (media variable)
and the AVStream member structure (media->streams[0]). Some are in slightly different units.

For instance, media->start_time and media->duration are in units of AV_TIME_BASE fractional sections, and should be roughly equivalent to
media->streams[0]->duration and media->streams[0]->start_time, which are in units of the stream timebase. Consequently:
media->duration = media->streams[0]->duration * stream_timebase (12800 or 90000 usually) / 1000

This helpful answer outlines some of them:
https://stackoverflow.com/questions/40275242/libav-ffmpeg-copying-decoded-video-timestamps-to-encoder



*/

void VideoDecoder::createMedia(std::string const & filename) {

    auto mymedia = libav::avformat_open_input(filename);
    _media = std::move(mymedia);
    libav::av_open_best_streams(_media);

    // Clear any previous state
    _pts.clear();
    _pts_index.clear();
    _pkt_durations.clear();
    _i_frames.clear();
    _i_frame_pts.clear();
    _frame_count = 0;

    // Determine the primary video stream index (assume 0 if single-stream usage)
    int const video_stream_index = 0; // this wrapper assumes the first stream is the video stream

    // Fast, safe scan: only consider valid video packets with usable PTS; collect keyframe locations
    // Note: not every packet produces a frame; we record only packets that have a defined PTS.
    for (auto & pkg: _media) {
        // Only consider packets from the selected video stream
        if (pkg.stream_index != video_stream_index) {
            ::av_packet_unref(&pkg);
            continue;
        }

        // Skip clearly unusable/corrupt packets
        if ((pkg.flags & AV_PKT_FLAG_CORRUPT) || pkg.size <= 0) {
            ::av_packet_unref(&pkg);
            continue;
        }

        // We rely on PTS to map to frame indices; skip packets without a valid PTS
        if (pkg.pts == static_cast<int64_t>(AV_NOPTS_VALUE)) {
            ::av_packet_unref(&pkg);
            continue;
        }

        // Keep a list of candidate frame PTS values (monotonically non-decreasing in most containers)
    _pts.push_back(static_cast<uint64_t>(pkg.pts));
    _pts_index[static_cast<uint64_t>(pkg.pts)] = static_cast<int64_t>(_pts.size() - 1);
        _pkt_durations.push_back(pkg.duration);

        if (pkg.flags & AV_PKT_FLAG_KEY) {
            // Store keyframe index as the index into our _pts vector
            _i_frames.push_back(static_cast<int>(_pts.size() - 1));
            _i_frame_pts.push_back(pkg.pts);
        }

        ::av_packet_unref(&pkg);
    }

    // Fallback: ensure we always have at least a starting keyframe at 0
    if (_i_frames.empty() && !_pts.empty()) {
        _i_frames.push_back(0);
        _i_frame_pts.push_back(static_cast<int64_t>(_pts[0]));
    }

    _height = static_cast<int>(_media->streams[0]->codecpar->height);
    _width = static_cast<int>(_media->streams[0]->codecpar->width);

    _frame_count = static_cast<int>(_pts.size());
    _last_decoded_frame = _frame_count > 0 ? _frame_count - 1 : 0;

    if (_verbose) {
        std::cout << "Frame number is " << _frame_count << std::endl;

        std::cout << "The start time is " << _getStartTime() << std::endl;
        std::cout << "The stream start time is " << _media->streams[0]->start_time << std::endl;
    if (!_pts.empty()) std::cout << "The first pts is " << _pts[0] << std::endl;
    if (_pts.size() > 1) std::cout << "The second pts is " << _pts[1] << std::endl;
    }

    auto & track = _media->streams[0];

    if (_verbose) {
        std::cout << "real_frame rate of stream " << track->r_frame_rate.num << " denom: " << track->r_frame_rate.den
                  << std::endl;
    }

    //Future calculations with frame rate use 1 / fps, so we swap numerator and denominator here
    _fps_num = _media->streams[0]->r_frame_rate.den;
    _fps_denom = _media->streams[0]->r_frame_rate.num;
    if (_fps_num == 0) _fps_num = 1;
    if (_fps_denom == 0) {
        constexpr int kDefaultFps = 30;
        _fps_denom = kDefaultFps; // sensible default
    }
    if (_verbose) {
        std::cout << "FPS numerator " << _fps_num << std::endl;
        std::cout << "FPS denominator " << _fps_denom << std::endl;
    }

    int largest_diff = find_buffer_size(_i_frames);
    if (largest_diff < 1) largest_diff = 1;

    //Now let's decode the first frame

    _pkt = std::move(_media.begin());
    _seekToFrame(0);

    _frame_buf->buildFrameBuffer(largest_diff);

    if (_verbose) {
        std::cout << "Buffer size set to " << largest_diff << std::endl;
    }
}

template<typename T>
int find_buffer_size(std::vector<T> & vec) {
    if (vec.size() < 2) {
        // Minimal buffer when we don't have enough keyframe spacing information
        return 1;
    }

    int largest_diff = static_cast<int>(vec[1] - vec[0]);
    for (size_t i = 2; i < vec.size(); i++) {
        const int diff = static_cast<int>(vec[i] - vec[i - 1]);
        if (diff > largest_diff) {
            largest_diff = diff;
        }
    }
    return largest_diff;
}

std::vector<uint8_t> VideoDecoder::getFrame(int const desired_frame, bool isFrameByFrameMode) {
    size_t const pixel_size = static_cast<size_t>(_getFormatBytes());
    size_t const buf_size = static_cast<size_t>(_height) * static_cast<size_t>(_width) * pixel_size;
    std::vector<uint8_t> output(buf_size);

    if (_pts.empty()) {
        return output; // nothing to decode
    }

    int const clamped_desired = std::clamp(desired_frame, 0, static_cast<int>(_pts.size() - 1));
    uint64_t const desired_frame_pts = _pts[static_cast<size_t>(clamped_desired)];

    if (_frame_buf->isFrameInBuffer(clamped_desired)) {
        auto frame = _frame_buf->getFrameFromBuffer(clamped_desired);
        _convertFrameToOutputFormat(frame.get(), output);// Convert the frame to format to render
        return output;
    }

    bool seek_flag = false;
    int64_t const desired_nearest_iframe = nearest_iframe(clamped_desired);

    int64_t cur_index = -1;
    if (_pkt.get() && _pkt.get()->pts != static_cast<int64_t>(AV_NOPTS_VALUE)) {
        cur_index = _findFrameByPts(static_cast<uint64_t>(_pkt.get()->pts));
    }
    if (cur_index < 0) cur_index = 0;

    constexpr int kIframeSeekThreshold = 10;
    auto const distance_to_next_iframe = desired_nearest_iframe - cur_index;
    if (clamped_desired < cur_index || distance_to_next_iframe > kIframeSeekThreshold) {
        _seekToFrame(static_cast<int>(desired_nearest_iframe), clamped_desired == desired_nearest_iframe);
        seek_flag = true;
    }

    if (!seek_flag) {
        int64_t pos = -1;
        if (_pkt.get() && _pkt.get()->pts != static_cast<int64_t>(AV_NOPTS_VALUE)) {
            pos = _findFrameByPts(static_cast<uint64_t>(_pkt.get()->pts));
        }
        if (pos != clamped_desired) {
        ::av_packet_unref(_pkt.get());
        ++(_pkt);
            // Skip non-video packets
            while (_pkt.get() && _pkt.get()->stream_index != 0) {
                ::av_packet_unref(_pkt.get());
                ++(_pkt);
            }
    }
    }


    bool is_packet_decoded = false;

    int frames_decoded = 0;

    auto t1 = std::chrono::high_resolution_clock::now();

    bool is_frame_to_display = false;
    while (!is_frame_to_display) {

        is_packet_decoded = false;

        // Skip non-video or invalid-PTS packets before sending to decoder
        while (_pkt.get() && (_pkt.get()->stream_index != 0 || _pkt.get()->pts == static_cast<int64_t>(AV_NOPTS_VALUE))) {
            ::av_packet_unref(_pkt.get());
            ++_pkt;
        }

        if (!_pkt.get()) break;

        libav::avcodec_send_packet(_media, _pkt.get(), [&](const libav::AVFrame &frame) {
            // Tag buffered frames by the decoded frame PTS, not the packet PTS.
            int idx = -1;
            if (frame.get()) {
                int64_t ts = (frame.get()->best_effort_timestamp != static_cast<int64_t>(AV_NOPTS_VALUE))
                             ? frame.get()->best_effort_timestamp
                             : frame.get()->pts;
                if (ts != static_cast<int64_t>(AV_NOPTS_VALUE)) {
                    idx = _findFrameByPts(static_cast<uint64_t>(ts));
                }
            }
            if (idx >= 0) {
                _frame_buf->addFrametoBuffer(frame, idx);
            }
            is_packet_decoded = true;
            int64_t ts = (frame.get()->best_effort_timestamp != static_cast<int64_t>(AV_NOPTS_VALUE))
                         ? frame.get()->best_effort_timestamp
                         : frame.get()->pts;
            if (ts == static_cast<int64_t>(desired_frame_pts)) {
                is_frame_to_display = true;
                _convertFrameToOutputFormat(frame.get(), output);
            }
        });
        if ((!is_packet_decoded) || (!is_frame_to_display)) {
            if (_pkt.get()) {
                ::av_packet_unref(_pkt.get());
            }
            ++(_pkt);
            // Skip non-video packets
            while (_pkt.get() && _pkt.get()->stream_index != 0) {
                ::av_packet_unref(_pkt.get());
                ++(_pkt);
            }
            if (!_pkt.get()) {
                // Reached end without finding frame; break to avoid infinite loop
                break;
            }
        }
        frames_decoded++;
    }

    auto t2 = std::chrono::high_resolution_clock::now();

    const std::chrono::duration<double> elapsed1 = t2 - t1;

    // 2/22/23 - Time results show decoding takes ~3ms a frame, which adds up if there are 100-200 frames to decode.

    {
        int64_t idx = -1;
        if (_pkt.get() && _pkt.get()->pts != static_cast<int64_t>(AV_NOPTS_VALUE)) {
            idx = _findFrameByPts(static_cast<uint64_t>(_pkt.get()->pts));
        }
        _last_decoded_frame = (idx >= 0) ? idx : clamped_desired;
    }
    return output;
}

void VideoDecoder::_convertFrameToOutputFormat(::AVFrame * frame, std::vector<uint8_t> & output) const {
    switch (_format) {
        case OutputFormat::Gray8:
            _togray8(frame, output);
            break;
        case OutputFormat::ARGB:
            _torgb32(frame, output);
            break;
        default:
            std::cout << "Output not supported" << std::endl;
    }
}

int VideoDecoder::_getFormatBytes() const {
    switch (_format) {
        case OutputFormat::Gray8:
            return 1;
        case OutputFormat::ARGB:
            return 4;
        default:
            return 1;
    }
}

static inline void copy_plane(uint8_t * dst, int dst_stride,
                              uint8_t const * src, int src_stride,
                              int width_bytes, int height) {
    if (src_stride == dst_stride && dst_stride == width_bytes) {
        // Fast path: tightly packed, one memcpy
        std::memcpy(dst, src, static_cast<size_t>(height) * static_cast<size_t>(width_bytes));
        return;
    }
    for (int y = 0; y < height; ++y) {
        std::memcpy(dst + y * dst_stride, src + y * src_stride, width_bytes);
    }
}

void VideoDecoder::_togray8(::AVFrame * frame, std::vector<uint8_t> & output) const {
    // Output is WxH, 1 byte per pixel
    uint8_t * dst = output.data();
    int const dst_stride = _width; // tightly packed GRAY8

    if (frame->format == AV_PIX_FMT_YUV420P) {
        // Fast path: start from the luma plane; expand range if source is limited (MPEG) range
        uint8_t const * srcY = frame->data[0];
        int const src_stride = std::abs(frame->linesize[0]);

        // Detect color range; default to limited if unspecified
        int const range = frame->color_range;
        bool const is_full_range = (range == AVCOL_RANGE_JPEG);

        if (is_full_range) {
            // Full range already in 0..255
            copy_plane(dst, dst_stride, srcY, src_stride, _width /*bytes*/, _height);
            return;
        }

        // Limited range (typically 16..235). Expand to full range: (Y-16) * 255 / 219
        for (int y = 0; y < _height; ++y) {
            uint8_t const * srow = srcY + y * src_stride;
            uint8_t * drow = dst + y * dst_stride;
            for (int x = 0; x < _width; ++x) {
                int v = static_cast<int>(srow[x]) - 16;
                if (v < 0) v = 0;
                // scale by 255/219 with rounding
                int scaled = (v * 255 + 109) / 219;
                if (scaled < 0) scaled = 0;
                if (scaled > 255) scaled = 255;
                drow[x] = static_cast<uint8_t>(scaled);
            }
        }
        return;
    }

    // Fallback: use libav conversion to GRAY8 (handles other formats and range correctly)
    auto gray = libav::convert_frame(frame, _width, _height, AV_PIX_FMT_GRAY8);
    uint8_t const * src = gray->data[0];
    int const src_stride = std::abs(gray->linesize[0]);
    copy_plane(dst, dst_stride, src, src_stride, _width /*bytes*/, _height);
}

void VideoDecoder::_torgb32(::AVFrame * frame, std::vector<uint8_t> & output) const {
    // Output is WxH, 4 bytes per pixel (RGBA)
    auto rgba = libav::convert_frame(frame, _width, _height, AV_PIX_FMT_RGBA);

    uint8_t * dst = output.data();
    int const bpp = 4;
    int const dst_stride = _width * bpp;

    uint8_t const * src = rgba->data[0];
    int const src_stride = std::abs(rgba->linesize[0]);

    copy_plane(dst, dst_stride, src, src_stride, _width * bpp, _height);
}

int64_t VideoDecoder::nearest_iframe(int64_t frame_id) {

    int64_t nearest_i_frame = 0;

    if (_i_frames.empty()) return 0;

    if (frame_id <= _i_frames.front()) {
        return _i_frames.front();
    }

    for (size_t i = 1; i < _i_frames.size(); i++) {
        if (_i_frames[i] > frame_id) {
            nearest_i_frame = _i_frames[i - 1];
            return nearest_i_frame;
        }
    }
    // If we didn't find a later keyframe, clamp to the last known keyframe
    nearest_i_frame = _i_frames.back();

    return nearest_i_frame;
}

void VideoDecoder::_seekToFrame(int const frame, bool keyframe) {

    //https://ffmpeg.org/doxygen/trunk/group__lavf__decoding.html
    //stream_index	If stream_index is (-1), a default stream is selected, and timestamp is automatically converted from AV_TIME_BASE units to the stream specific time_base.
    //timestamp	Timestamp in AVStream.time_base units or, if no stream is specified, in AV_TIME_BASE units.

    //auto t1 = std::chrono::high_resolution_clock::now();

    //We should include an offset here if the starting time is not equal to 0.

    //flush_decoder(_media, _pkt->stream_index);
    //_pkt.reset(); // Does this flush buffers?
    // Always flush the video stream decoder (assumed stream index 0)
    auto codecCtx = _media.open_streams.find(0);
    if (codecCtx != _media.open_streams.end()) {
        codecCtx->second.flush_buffers();
    }

    const libav::flicks time = libav::av_rescale(frame,
                                           {_media->streams[0]->r_frame_rate.den,
                                            _media->streams[0]->r_frame_rate.num});

    if (keyframe) {
    const libav::flicks adjust = libav::av_rescale(_media->streams[0]->start_time, {_media->streams[0]->time_base.num,
                                                                                  _media->streams[0]->time_base.den});
    const libav::flicks time2 = time + adjust;
        //libav::flicks time2 = time;
        //libav::av_seek_frame(media,time2,-1,AVSEEK_FLAG_ANY);
        libav::av_seek_frame(_media, time2, -1, AVSEEK_FLAG_BACKWARD);

    _pkt = std::move(
        _media.begin());// After we seek to a frame, this will read frame, followed by rescaling to appropriate time scale.
    // Advance to first key video packet for a clean decoder state
    while (_pkt.get() && (_pkt.get()->stream_index != 0 || !(_pkt.get()->flags & AV_PKT_FLAG_KEY))) {
        ::av_packet_unref(_pkt.get());
        ++_pkt;
    }

        if (_verbose) {
            if (_pkt.get()->flags & AV_PKT_FLAG_KEY) {
                std::cout << "The frame we seeked to is a keyframe" << std::endl;
            } else {
                std::cout << "We did NOT seek to a key frame" << std::endl;
            }
        }

    } else {
        libav::av_seek_frame(_media, time, -1, AVSEEK_FLAG_BACKWARD);

        _pkt = std::move(_media.begin());
        // Advance to first video packet
        while (_pkt.get() && _pkt.get()->stream_index != 0) {
            ::av_packet_unref(_pkt.get());
            ++_pkt;
        }
    }

    {
        int64_t idx = -1;
        if (_pkt.get() && _pkt.get()->pts != static_cast<int64_t>(AV_NOPTS_VALUE)) {
            idx = _findFrameByPts(static_cast<uint64_t>(_pkt.get()->pts));
        }
        _last_key_frame = (idx >= 0) ? idx : 0;
    }

    if (_verbose) {
        std::cout << "Seeked to frame " << _last_key_frame << std::endl;
    }

    //auto t2 = std::chrono::high_resolution_clock::now();

    //std::chrono::duration<double> elapsed1 = t2 - t1;

    //std::cout << "Time for frame seek was : " << elapsed1.count() << std::endl;
    // 2/22/23 - Time results suggest that frame seeking takes less than 1 ms
}

/**
*
* Frames in a video file have unique PTS values that roughly correspond to time stamps
* When we first read the video file, we keep a vector of all PTS values
* in the pts member variable. So if we have a pts value, we can find the frame ID
* By searching the dictionary
*
* Note, this the pts vector should be in increasing order, so a more efficient search method
* could be used here
*
* @param pts
* @return frame with matching pts input value
*/
int64_t VideoDecoder::_findFrameByPts(uint64_t pts)
{
    auto it = _pts_index.find(pts);
    if (it != _pts_index.end()) {
        return it->second;
    }
    // Fallback to binary search if map miss due to duplicates/cleanup
    auto lb = std::lower_bound(_pts.begin(), _pts.end(), pts);
    if (lb != _pts.end() && *lb == pts) {
        return static_cast<int64_t>(std::distance(_pts.begin(), lb));
    }
    return -1; // not found
}

}// namespace ffmpeg_wrapper
