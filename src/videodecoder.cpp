#include "videodecoder.h"

#include "libavinc/libavinc.hpp"

#include <functional>
#include <string>
#include <memory>
#include <algorithm>
#include <numeric>

#include <chrono>

namespace ffmpeg_wrapper {

FrameBuffer::FrameBuffer() {
    _enable = true;
    _verbose = false;
};

void FrameBuffer::buildFrameBuffer(int buf_size) {

    _frame_buf.clear();
    _frame_buf = std::vector<libav::AVFrame>(buf_size);

    /*
    for (int i = 0; i < buf_size; i++) {
        _frame_buf.push_back(libav::av_frame_alloc());
        _frame_buf[i]->format = frame->format;
        _frame_buf[i]->width = frame->width;
        _frame_buf[i]->height = frame->height;
        _frame_buf[i]->channels = frame->channels;
        _frame_buf[i]->channel_layout = frame->channel_layout;
        _frame_buf[i]->nb_samples = frame->nb_samples;
        libav::av_frame_get_buffer(_frame_buf[i]);
    }
    */

    _frame_buf_id.resize(buf_size);
    std::fill(_frame_buf_id.begin(), _frame_buf_id.end(), -1);

}

void FrameBuffer::addFrametoBuffer(libav::AVFrame frame, int pos) {

    if (_enable) {
        //Check if the position is already in the buffer
        if (isFrameInBuffer(pos)) {
            if (_verbose) {
                std::cout << "Frame " << pos << " is already in the buffer" << std::endl;
            }
        } else {

            if (pos - _keyframe > _frame_buf.size() - 1) {
                if (_verbose) {
                    std::cout << "Error, buffer index equal to " << pos - _keyframe << std::endl;
                }
            } else {
                _frame_buf[pos - _keyframe] = frame;
                //libav::av_frame_copy(_frame_buf[pos - _keyframe], frame);
                _frame_buf_id[pos - _keyframe] = pos;
            }
        }
    }
}

bool FrameBuffer::isFrameInBuffer(int frame) {
    return std::find(_frame_buf_id.begin(), _frame_buf_id.end(), frame) != _frame_buf_id.end();
}

libav::AVFrame FrameBuffer::getFrameFromBuffer(int frame) {
    auto buf_pos = std::distance(_frame_buf_id.begin(), std::find(_frame_buf_id.begin(), _frame_buf_id.end(), frame));

    return _frame_buf[buf_pos];
}

VideoDecoder::VideoDecoder() {

    _frame_count = 0;
    _last_decoded_frame = 0;
    _last_key_frame = 0;
    _pts.reserve(500000);
    _last_packet_decoded = false;

    _format = OutputFormat::Gray8;

    _verbose = false;

    _frame_buf = std::make_unique<FrameBuffer>();
}

VideoDecoder::VideoDecoder(const std::string &filename) {
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

void VideoDecoder::createMedia(const std::string &filename) {

    auto mymedia = libav::avformat_open_input(filename);
    _media = std::move(mymedia);
    libav::av_open_best_streams(_media);


    /*
    Loop through and get the PTS values, which we will use to determine if we are at the correct frame or not in the future.
    */
    //seekToFrame(0,true);
    for (auto &pkg: _media) {
        if (pkg.flags & AV_PKT_FLAG_KEY) {
            // this is I-frame. We may want to keep a list of these for fast scrolling.
            _i_frames.push_back((_frame_count));
            _i_frame_pts.push_back(pkg.pts);
        }
        _frame_count++;
        uint64_t pts = pkg.pts;
        _pts.push_back(pts);
        _pkt_durations.push_back(pkg.duration);
    }

    _frame_count--;
    _height = _media->streams[0]->codecpar->height;
    _width = _media->streams[0]->codecpar->width;

    //pkt = std::move(media.begin());

    /*
    seekToFrame(0,true);

    for (int i = 0; i < pts.size(); i++) {

        pts[i] = pkt.get()->pts;
        ++(pkt);
    }
    */

    _last_decoded_frame = _frame_count;

    if (_verbose) {
        std::cout << "Frame number is " << _frame_count << std::endl;

        std::cout << "The start time is " << _getStartTime() << std::endl;
        std::cout << "The stream start time is " << _media->streams[0]->start_time << std::endl;
        std::cout << "The first pts is " << _pts[0] << std::endl;
        std::cout << "The second pts is " << _pts[1] << std::endl;
    }

    auto &track = _media->streams[0];

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

    _seekToFrame(0);

    libav::avcodec_send_packet(_media, _pkt.get(), [&](libav::AVFrame frame) {
        // Use that decoded frame to set up the frame buffer properties
        //frame_buf->buildFrameBuffer(largest_diff,frame);
    });

    _frame_buf->buildFrameBuffer(largest_diff);

    if (_verbose) {
        std::cout << "Buffer size set to " << largest_diff << std::endl;
    }

}

template<typename T>
int find_buffer_size(std::vector<T> &vec) {

    int largest_diff = vec[1] - vec[0];
    for (int i = 2; i < vec.size(); i++) {
        if (vec[i] - vec[i - 1] > largest_diff) {
            largest_diff = vec[i] - vec[i - 1];
        }
    }
    return largest_diff;
}

std::vector<uint8_t> VideoDecoder::getFrame(const int desired_frame, bool isFrameByFrameMode)
{
    std::vector<uint8_t> output(_height *
                                _width *
                                _getFormatBytes()); //The height and width of the video do not change, so we should initialize this and re-use it.

    //First we can check the pts value of the pkt we currently have stored and compare it to the PTS of the frame we wish to seek to
    const int64_t desired_frame_pts = _pts[desired_frame];
    const int64_t desired_nearest_iframe = nearest_iframe(desired_frame);

    int64_t current_pkt_pts = _pkt.get()->pts;
    auto current_pkt_frame = _findFrameByPts(current_pkt_pts);

    bool is_packet_decoded = false;
    bool is_keyframe = false;

    /*
    Now we may wish to either 1) seek to the nearest (backward) keyframe, and decode forward 2) decode forward from where we are already or 3) load an already decoded frame
    from the frame buffer
    */
    if (_frame_buf->isFrameInBuffer(desired_frame)) {
        if (_verbose) {
            std::cout << "Desired frame is " << desired_frame << " and it is already in the buffer" << std::endl;
        }
        auto frame = _frame_buf->getFrameFromBuffer(desired_frame);
        _convertFrameToOutputFormat(frame.get(), output); // Convert the frame to format to render

        return output;

    } else if (desired_frame < current_pkt_frame) {

        if (_verbose) {
            std::cout << "The desired frame " << desired_frame
                      << " is before where we currently are in the video, but we don't have it in the buffer. We need to seek"
                      << std::endl;
        }
        if (desired_frame == desired_nearest_iframe) {
            if (_verbose) {
                std::cout << "The desired frame is a keyframe" << std::endl;
            }
            _seekToFrame(desired_frame, true);
        } else {
            _seekToFrame(desired_frame);
        }

    } else if (desired_nearest_iframe > current_pkt_frame) {

        if (_verbose) {
            std::cout
                    << "The nearest iframe is greater than our current frame, so we should seek there instead of decoding forward"
                    << std::endl;
        }
        if (desired_frame == desired_nearest_iframe) {
            // I think that this doesn't work well if the desired frame is also a keyframe
            if (_verbose) {
                std::cout << "The desired frame is a keyframe" << std::endl;
            }
            _seekToFrame(desired_frame, true);
        } else {
            _seekToFrame(desired_frame);
        }


    } else if (desired_frame == current_pkt_frame) {

        if (_verbose) {
            std::cout << "Our current packet matches the desired frame" << std::endl;
        }
    } else {

        if (_verbose) {
            std::cout << "The desired frame is " << desired_frame - current_pkt_frame
                      << " frame ahead. We should seek forward from where we are" << std::endl;
        }
        ++(_pkt);
    }

    bool is_frame_to_display = false;

    int frames_decoded = 0;

    auto t1 = std::chrono::high_resolution_clock::now();

    while (!is_frame_to_display) {
        //this_pkt_pts = pkt.get()->pts;
        current_pkt_frame = _findFrameByPts(_pkt.get()->pts);
        is_packet_decoded = false;
        auto frame_pts = _pkt.get()->pts;

        auto err = libav::avcodec_send_packet(_media, _pkt.get(), [&](libav::AVFrame frame) {

            // We have the packet we want, so we should convert to grayscale to be displayed
            _frame_buf->addFrametoBuffer(frame, current_pkt_frame);
            is_packet_decoded = true;
            frame_pts = frame.get()->pts;
            //frame_pts = frame.get()->best_effort_timestamp;
            if (frame_pts == desired_frame_pts) {
                if (_verbose) {
                    std::cout << "frame pts matches desired frame" << std::endl;
                }
                is_frame_to_display = true;
                _convertFrameToOutputFormat(frame.get(), output);
            }
            if (_verbose) {
                std::cout << "Timestamp: " << frame_pts << std::endl;
                std::cout << "Frame pts: " << frame.get()->pts << std::endl;
            }

        });
        if ((!is_packet_decoded) | (!is_frame_to_display)) {
            ++(_pkt);
        }
        frames_decoded++;
    }

    if (_verbose) {
        std::cout << "Returning frame number : " << _findFrameByPts(_pkt.get()->pts) << std::endl;
    }

    auto t2 = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed1 = t2 - t1;

    if (_verbose) {
        std::cout << "Time for decoding was : " << elapsed1.count() << " for " << frames_decoded << " frames."
                  << std::endl;
    }
    // 2/22/23 - Time results show decoding takes ~3ms a frame, which adds up if there are 100-200 frames to decode.

    _last_decoded_frame = _findFrameByPts(_pkt.get()->pts);
    return output;
}

void VideoDecoder::_convertFrameToOutputFormat(::AVFrame* frame, std::vector<uint8_t>& output)
{
    switch (_format) {
        case OutputFormat::Gray8:
            _togray8(frame, output);
            break;
        case OutputFormat::ARGB:
            _torgb32(frame,output);
            break;
        default:
            std::cout << "Output not supported" << std::endl;
    }
}

int VideoDecoder::_getFormatBytes()
{
    switch (_format) {
        case OutputFormat::Gray8:
            return 1;
        case OutputFormat::ARGB:
            return 4;
        default:
            return 1;
    }
}

void VideoDecoder::_togray8(::AVFrame* frame, std::vector<uint8_t> &output) {
    auto t1 = std::chrono::high_resolution_clock::now();
    auto frame2 = libav::convert_frame(frame, _width, _height, AV_PIX_FMT_GRAY8);
    memcpy(output.data(), frame2->data[0], _height * _width * _getFormatBytes());
    auto t2 = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = t2 - t1;

    if (_verbose) {
        std::cout << "Time for conversion was : " << elapsed.count() << std::endl;
    }
}

void VideoDecoder::_torgb32(::AVFrame* frame, std::vector<uint8_t> &output) {

    auto t1 = std::chrono::high_resolution_clock::now();
    auto frame2 = libav::convert_frame(frame, _width, _height, AV_PIX_FMT_RGBA);
    memcpy(output.data(), frame2->data[0], _height * _width * _getFormatBytes());
    auto t2 = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = t2 - t1;

    if (_verbose) {
        std::cout << "Time for conversion was : " << elapsed.count() << std::endl;
    }
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

void VideoDecoder::_seekToFrame(const int frame, bool keyframe) {

    //https://ffmpeg.org/doxygen/trunk/group__lavf__decoding.html
    //stream_index	If stream_index is (-1), a default stream is selected, and timestamp is automatically converted from AV_TIME_BASE units to the stream specific time_base.
    //timestamp	Timestamp in AVStream.time_base units or, if no stream is specified, in AV_TIME_BASE units.

    //auto t1 = std::chrono::high_resolution_clock::now();

    //We should include an offset here if the starting time is not equal to 0.

    _pkt.reset(); // Does this flush buffers?

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
                _media.begin()); // After we seek to a frame, this will read frame, followed by rescaling to appropriate time scale.

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
    _frame_buf->resetKeyFrame(_last_key_frame);

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

}
