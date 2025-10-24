#include "videodecoder.h"

#include "libavinc/libavinc.hpp"

#include "libavcodec/packet.h"
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
            _frame_buf.push_back(FrameBufferElement{pos, frame});
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

    _pts.reserve(500000);

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

    /*
    Loop through and get the PTS values, which we will use to determine if we are at the correct frame or not in the future.
    */
    for (auto & pkg: _media) {
        if (pkg.flags & AV_PKT_FLAG_KEY) {
            // this is I-frame. We may want to keep a list of these for fast scrolling.
            _i_frames.push_back((_frame_count));
            _i_frame_pts.push_back(pkg.pts);
        }
        _frame_count++;
        uint64_t pts = pkg.pts;
        _pts.push_back(pts);
        _pkt_durations.push_back(pkg.duration);
        ::av_packet_unref(&pkg);
    }

    _height = _media->streams[0]->codecpar->height;
    _width = _media->streams[0]->codecpar->width;

    _last_decoded_frame = _frame_count;

    if (_verbose) {
        std::cout << "Frame number is " << _frame_count << std::endl;

        std::cout << "The start time is " << _getStartTime() << std::endl;
        std::cout << "The stream start time is " << _media->streams[0]->start_time << std::endl;
        std::cout << "The first pts is " << _pts[0] << std::endl;
        std::cout << "The second pts is " << _pts[1] << std::endl;
    }

    auto & track = _media->streams[0];

    if (_verbose) {
        std::cout << "real_frame rate of stream " << track->r_frame_rate.num << " denom: " << track->r_frame_rate.den
                  << std::endl;
    }

    //Future calculations with frame rate use 1 / fps, so we swap numerator and denominator here
    _fps_num = _media->streams[0]->r_frame_rate.den;
    _fps_denom = _media->streams[0]->r_frame_rate.num;
    if (_verbose) {
        std::cout << "FPS numerator " << _fps_num << std::endl;
        std::cout << "FPS denominator " << _fps_denom << std::endl;
    }

    int largest_diff = find_buffer_size(_i_frames);

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

    int largest_diff = vec[1] - vec[0];
    for (int i = 2; i < vec.size(); i++) {
        if (vec[i] - vec[i - 1] > largest_diff) {
            largest_diff = vec[i] - vec[i - 1];
        }
    }
    return largest_diff;
}

std::vector<uint8_t> VideoDecoder::getFrame(int const desired_frame, bool isFrameByFrameMode) {
    std::vector<uint8_t> output(_height * _width * _getFormatBytes());

    int64_t const desired_frame_pts = _pts[desired_frame];

    if (_frame_buf->isFrameInBuffer(desired_frame)) {
        auto frame = _frame_buf->getFrameFromBuffer(desired_frame);
        _convertFrameToOutputFormat(frame.get(), output);// Convert the frame to format to render
        return output;
    }

    bool seek_flag = false;
    int64_t const desired_nearest_iframe = nearest_iframe(desired_frame);
    auto distance_to_next_iframe = desired_nearest_iframe - _findFrameByPts(_pkt.get()->pts);
    if (desired_frame < _findFrameByPts(_pkt.get()->pts) || distance_to_next_iframe > 10) {
        _seekToFrame(desired_nearest_iframe, desired_frame == desired_nearest_iframe);
        seek_flag = true;
    }


    if (desired_frame != _findFrameByPts(_pkt.get()->pts) && !seek_flag) {
        ::av_packet_unref(_pkt.get());
        ++(_pkt);
    }


    bool is_packet_decoded = false;

    int frames_decoded = 0;

    auto t1 = std::chrono::high_resolution_clock::now();

    bool is_frame_to_display = false;
    while (!is_frame_to_display) {

        is_packet_decoded = false;

        libav::avcodec_send_packet(_media, _pkt.get(), [&](libav::AVFrame frame) {
            _frame_buf->addFrametoBuffer(frame, _findFrameByPts(_pkt.get()->pts));
            is_packet_decoded = true;
            if (frame.get()->pts == desired_frame_pts) {
                is_frame_to_display = true;
                _convertFrameToOutputFormat(frame.get(), output);
            }
        });
        if ((!is_packet_decoded) | (!is_frame_to_display)) {
            ::av_packet_unref(_pkt.get());
            ++(_pkt);
        }
        frames_decoded++;
    }

    auto t2 = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed1 = t2 - t1;

    // 2/22/23 - Time results show decoding takes ~3ms a frame, which adds up if there are 100-200 frames to decode.

    _last_decoded_frame = _findFrameByPts(_pkt.get()->pts);
    return output;
}

void VideoDecoder::_convertFrameToOutputFormat(::AVFrame * frame, std::vector<uint8_t> & output) {
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

int VideoDecoder::_getFormatBytes() {
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
        std::memcpy(dst, src, static_cast<size_t>(height) * width_bytes);
        return;
    }
    for (int y = 0; y < height; ++y) {
        std::memcpy(dst + y * dst_stride, src + y * src_stride, width_bytes);
    }
}

void VideoDecoder::_togray8(::AVFrame * frame, std::vector<uint8_t> & output) {
    // Output is WxH, 1 byte per pixel
    uint8_t * dst = output.data();
    int const dst_stride = _width;// tightly packed GRAY8

    if (frame->format == AV_PIX_FMT_YUV420P) {
        // Use the luma plane directly
        uint8_t const * src = frame->data[0];
        int const src_stride = std::abs(frame->linesize[0]);
        copy_plane(dst, dst_stride, src, src_stride, _width /*bytes*/, _height);
        return;
    }

    // Fallback: convert to GRAY8 first, then copy respecting stride
    auto gray = libav::convert_frame(frame, _width, _height, AV_PIX_FMT_GRAY8);
    uint8_t const * src = gray->data[0];
    int const src_stride = std::abs(gray->linesize[0]);
    copy_plane(dst, dst_stride, src, src_stride, _width /*bytes*/, _height);
}

void VideoDecoder::_torgb32(::AVFrame * frame, std::vector<uint8_t> & output) {
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

    for (int i = 1; i < _i_frames.size(); i++) {
        if (_i_frames[i] > frame_id) {
            nearest_i_frame = _i_frames[i - 1];
            break;
        }
    }

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
    auto codecCtx = _media.open_streams.find(_pkt->stream_index);
    codecCtx->second.flush_buffers();

    libav::flicks time = libav::av_rescale(frame,
                                           {_media->streams[0]->r_frame_rate.den,
                                            _media->streams[0]->r_frame_rate.num});

    if (keyframe) {
        libav::flicks adjust = libav::av_rescale(_media->streams[0]->start_time, {_media->streams[0]->time_base.num,
                                                                                  _media->streams[0]->time_base.den});
        libav::flicks time2 = time + adjust;
        //libav::flicks time2 = time;
        //libav::av_seek_frame(media,time2,-1,AVSEEK_FLAG_ANY);
        libav::av_seek_frame(_media, time2, -1, AVSEEK_FLAG_BACKWARD);

        _pkt = std::move(
                _media.begin());// After we seek to a frame, this will read frame, followed by rescaling to appropriate time scale.

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
    }

    _last_key_frame = _findFrameByPts(_pkt.get()->pts);

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
    int64_t matching_frame = 0;

    for (int i = 0; i < _pts.size(); i++) {
        if (_pts[i] == pts) {
            matching_frame = i;
            break;
        }
    }

    return matching_frame;
}

}// namespace ffmpeg_wrapper
