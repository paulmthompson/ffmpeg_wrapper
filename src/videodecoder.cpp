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
    enable = false;
    verbose = false;
};

void FrameBuffer::buildFrameBuffer(int buf_size, libav::AVFrame frame) {

    this->frame_buf.clear();
    for (int i=0; i < buf_size; i++) {
        this->frame_buf.push_back(libav::av_frame_alloc());
        this->frame_buf[i]->format = frame->format;
        this->frame_buf[i]->width = frame->width;
        this->frame_buf[i]->height = frame->height;
        this->frame_buf[i]->channels = frame->channels;
        this->frame_buf[i]->channel_layout = frame->channel_layout;
        this->frame_buf[i]->nb_samples = frame->nb_samples;
        libav::av_frame_get_buffer(this->frame_buf[i]);
    }

    this->frame_buf_id.resize(buf_size);
    std::fill(this->frame_buf_id.begin(),this->frame_buf_id.end(), -1);
}

void FrameBuffer::addFrametoBuffer(libav::AVFrame& frame, int pos) {

    if (this->enable) {
        //Check if the position is already in the buffer
        if ( this->isFrameInBuffer(pos)) {
            if (this->verbose) {
                std::cout << "Frame " << pos << " is already in the buffer" << std::endl;
            }
        } else {

            if (pos - this->keyframe > this->frame_buf.size() - 1 ) {
                if (this->verbose) {
                    std::cout << "Error, buffer index equal to " << pos - this->keyframe << std::endl;
                }
            } else {
                libav::av_frame_copy(this->frame_buf[pos - this->keyframe],frame);
                this->frame_buf_id[pos - this->keyframe] = pos;
            }
        }
    }
}

bool FrameBuffer::isFrameInBuffer(int frame) {
    return std::find(this->frame_buf_id.begin(), this->frame_buf_id.end(), frame) != this->frame_buf_id.end();
}

libav::AVFrame FrameBuffer::getFrameFromBuffer(int frame) {
    auto buf_pos = std::distance(this->frame_buf_id.begin(), std::find(this->frame_buf_id.begin(), this->frame_buf_id.end(), frame));

    return this->frame_buf[buf_pos];
}

VideoDecoder::VideoDecoder()
{

    frame_count = 0;
    last_decoded_frame = 0;
    last_key_frame = 0;
    pts.reserve(500000);
    last_packet_decoded = false;

    verbose = false;
    
    frame_buf = std::make_unique<FrameBuffer>();
}

VideoDecoder::VideoDecoder(const std::string& filename) {
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

void VideoDecoder::createMedia(const std::string& filename) {

    auto mymedia = libav::avformat_open_input(filename);
    this->media = std::move(mymedia);
    libav::av_open_best_streams(this->media);


    /*
    Loop through and get the PTS values, which we will use to determine if we are at the correct frame or not in the future.
    */
    //this->seekToFrame(0,true);
    for (auto& pkg : this->media)
    {
        if (pkg.flags & AV_PKT_FLAG_KEY) {
            // this is I-frame. We may want to keep a list of these for fast scrolling.
            this->i_frames.push_back((this->frame_count));
            this->i_frame_pts.push_back(pkg.pts);
        }
        this->frame_count++;
        uint64_t pts = pkg.pts;
        this->pts.push_back(pts);
        this->pkt_durations.push_back(pkg.duration);
    }
    
    this->frame_count--;
    this->height = media->streams[0]->codecpar->height;
    this->width = media->streams[0]->codecpar->width;

    //this->pkt = std::move(this->media.begin());
    
    /*
    this->seekToFrame(0,true);

    for (int i = 0; i < this->pts.size(); i++) {

        this->pts[i] = this->pkt.get()->pts;
        ++(this->pkt);
    }
    */

    this->last_decoded_frame = frame_count;

    if (this->verbose) {
        std::cout << "Frame number is " << this->frame_count << std::endl;

        std::cout << "The start time is " << this->getStartTime() << std::endl;
        std::cout << "The stream start time is " << this->media->streams[0]->start_time << std::endl;
        std::cout << "The first pts is " << this->pts[0] << std::endl;
        std::cout << "The second pts is " << this->pts[1] << std::endl;
    }

    auto& track = this->media->streams[0];

    if (this->verbose) {
        std::cout << "real_frame rate of stream " << track->r_frame_rate.num << " denom: " << track->r_frame_rate.den << std::endl;
    }

    //Future calculations with frame rate use 1 / fps, so we swap numerator and denominator here
    this->fps_num = this->media->streams[0]->r_frame_rate.den;
    this->fps_denom = this->media->streams[0]->r_frame_rate.num;
    if (this->verbose) {
        std::cout << "FPS numerator " << fps_num << std::endl;
        std::cout << "FPS denominator " << fps_denom << std::endl;
    }

    int largest_diff = find_buffer_size(this->i_frames);

    //Now let's decode the first frame
    
    this->seekToFrame(0);
    
    libav::avcodec_send_packet(this->media,this->pkt.get(), [&](libav::AVFrame frame) {
    // Use that decoded frame to set up the frame buffer properties
        //frame_buf->buildFrameBuffer(largest_diff,frame);
    });
    
    if (this->verbose) {
        std::cout << "Buffer size set to " << largest_diff << std::endl;
    }
    
}

template <typename T>
int find_buffer_size(std::vector<T>& vec) {

    int largest_diff = vec[1] - vec[0];
    for (int i = 2; i < vec.size(); i++) {
        if (vec[i] - vec[i-1] > largest_diff) {
            largest_diff = vec[i] - vec[i-1];
        }
    }
    return largest_diff;
}

/*
desired_frame is the frame to seek to in a compressed video
We want to return this frame in a vector of bytes that can be easily rendered

Future improvements could return different output types (such as 16-bit uints)
*/
std::vector<uint8_t> VideoDecoder::getFrame(const int desired_frame,bool frame_by_frame)
{
    std::vector<uint8_t> output(this->height * this->width); //The height and width of the video do not change, so we should initialize this and re-use it.

    //First we can check the pts value of the pkt we currently have stored and compare it to the PTS of the frame we wish to seek to
    const int64_t desired_frame_pts = this->pts[desired_frame];
    const int64_t desired_nearest_iframe = this->nearest_iframe(desired_frame);

    int64_t this_pkt_pts = this->pkt.get()->pts;
    auto this_pkt_frame = find_frame_by_pts(this_pkt_pts);

    bool packet_decoded = false;
    bool keyframe = false;

    /*
    Now we may wish to either 1) seek to the nearest (backward) keyframe, and decode forward 2) decode forward from where we are already or 3) load an already decoded frame
    from the frame buffer
    */
    if (frame_buf->isFrameInBuffer(desired_frame)) {
        if (this->verbose) {
            std::cout << "Desired frame is " << desired_frame << " and it is already in the buffer" << std::endl;
        }
        auto frame = frame_buf->getFrameFromBuffer(desired_frame);
        yuv420togray8(frame, output); // Convert the frame to gray format to render

        return output;

    } else if (desired_frame < this_pkt_frame) {

        if (this->verbose) {
            std::cout << "The desired frame " << desired_frame << " is before where we currently are in the video, but we don't have it in the buffer. We need to seek" << std::endl;
        }
        if (desired_frame == desired_nearest_iframe) {
            if (this->verbose) {
                std::cout << "The desired frame is a keyframe" << std::endl;
            }
            this->seekToFrame(desired_frame, true);
        } else {
            this->seekToFrame(desired_frame);
        }

    } else if (desired_nearest_iframe > this_pkt_frame) {

        if (this->verbose) {
            std::cout << "The nearest iframe is greater than our current frame, so we should seek there instead of decoding forward" << std::endl;
        }
        if (desired_frame == desired_nearest_iframe) {
             // I think that this doesn't work well if the desired frame is also a keyframe
            if (this->verbose) {
                std::cout << "The desired frame is a keyframe" << std::endl;
            }
            this->seekToFrame(desired_frame, true);
        } else {
            this->seekToFrame(desired_frame);
        }
        

    } else if (desired_frame == this_pkt_frame) {

        if (this->verbose) {
            std::cout << "Our current packet matches the desired frame" << std::endl;
        }
    } else {

        if (this->verbose) {
            std::cout << "The desired frame is " << desired_frame - this_pkt_frame << " frame ahead. We should seek forward from where we are" << std::endl;
        }
        ++(this->pkt);
    }

    bool frame_to_display = false;

    int frames_decoded = 0;

    auto t1 = std::chrono::high_resolution_clock::now();

    while (!frame_to_display)
    {
        //this_pkt_pts = this->pkt.get()->pts;
        this_pkt_frame = find_frame_by_pts(this->pkt.get()->pts);
        packet_decoded = false;
        auto frame_pts = this->pkt.get()->pts;

        auto err = libav::avcodec_send_packet(this->media,this->pkt.get(), [&](libav::AVFrame frame) {

                // We have the packet we want, so we should convert to grayscale to be displayed
                //frame_buf->addFrametoBuffer(frame, this_pkt_frame);
                packet_decoded = true;
                frame_pts = frame.get()->pts;
                //frame_pts = frame.get()->best_effort_timestamp;
                if (frame_pts == desired_frame_pts) {
                    if (this->verbose) {
                        std::cout << "frame pts matches desired frame" << std::endl;
                    }
                    frame_to_display = true;
                    yuv420togray8(frame,output); 
                }
                if (this->verbose) {
                    std::cout << "Timestamp: " << frame_pts << std::endl;
                    std::cout << "Frame pts: " << frame.get()->pts << std::endl;
                }
                
            });
        if ( (!packet_decoded) | (!frame_to_display) ) {
            ++(this->pkt);
        }
        frames_decoded ++;  
    }

    if (this->verbose) {
        std::cout << "Returning frame number : " << find_frame_by_pts(this->pkt.get()->pts) << std::endl;
    }

    auto t2 = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed1 = t2 - t1;

    if (this->verbose) {
        std::cout << "Time for decoding was : " << elapsed1.count() << " for " << frames_decoded << " frames." << std::endl;
    }
    // 2/22/23 - Time results show decoding takes ~3ms a frame, which adds up if there are 100-200 frames to decode.

    this->last_decoded_frame =  find_frame_by_pts(this->pkt.get()->pts);
    return output;
}

void VideoDecoder::yuv420togray8(libav::AVFrame& frame,std::vector<uint8_t>& output)
{
    auto t1 = std::chrono::high_resolution_clock::now();
    auto frame2 = libav::convert_frame(frame, this->width, this->height, AV_PIX_FMT_GRAY8);
    memcpy(output.data(),frame2->data[0],this->height*this->width);
    auto t2 = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = t2 - t1;

    if (this->verbose) {
        std::cout << "Time for conversion was : " << elapsed.count() << std::endl;
    }
}

int64_t VideoDecoder::nearest_iframe(int64_t frame_id) {
    
    int64_t nearest_i_frame = 0;
    
    for (int i = 1; i < this->i_frames.size(); i++) {
        if (this->i_frames[i] > frame_id) {
            nearest_i_frame = this->i_frames[i-1];
            break;
        }
    }

    return nearest_i_frame;
}

void VideoDecoder::seekToFrame(const int frame, bool keyframe) {

    //https://ffmpeg.org/doxygen/trunk/group__lavf__decoding.html
    //stream_index	If stream_index is (-1), a default stream is selected, and timestamp is automatically converted from AV_TIME_BASE units to the stream specific time_base.
    //timestamp	Timestamp in AVStream.time_base units or, if no stream is specified, in AV_TIME_BASE units. 

    //auto t1 = std::chrono::high_resolution_clock::now();

    //We should include an offset here if the starting time is not equal to 0.

    this->pkt.reset(); // Does this flush buffers?

    libav::flicks time = libav::av_rescale(frame,
                {this->media->streams[0]->r_frame_rate.den,this->media->streams[0]->r_frame_rate.num});

    if (keyframe) {
        libav::flicks adjust = libav::av_rescale(this->media->streams[0]->start_time,{this->media->streams[0]->time_base.num ,this->media->streams[0]->time_base.den});
        libav::flicks time2 = time + adjust;
        //libav::flicks time2 = time;
        //libav::av_seek_frame(this->media,time2,-1,AVSEEK_FLAG_ANY);
        libav::av_seek_frame(this->media,time2,-1,AVSEEK_FLAG_BACKWARD);

        this->pkt = std::move(this->media.begin()); // After we seek to a frame, this will read frame, followed by rescaling to appropriate time scale.
        
        if (this->verbose) {
            if (this->pkt.get()->flags & AV_PKT_FLAG_KEY) {
                std::cout << "The frame we seeked to is a keyframe" << std::endl;
            } else {
                std::cout << "We did NOT seek to a key frame" << std::endl;
            }
        }
        
    } else {
        libav::av_seek_frame(this->media, time,-1,AVSEEK_FLAG_BACKWARD);

        this->pkt = std::move(this->media.begin());
    }

    this->last_key_frame = find_frame_by_pts(this->pkt.get()->pts);
    this->frame_buf->resetKeyFrame(this->last_key_frame);

    if (this->verbose) {
        std::cout << "Seeked to frame " << this->last_key_frame << std::endl;
    }

    //auto t2 = std::chrono::high_resolution_clock::now();

    //std::chrono::duration<double> elapsed1 = t2 - t1;

    //std::cout << "Time for frame seek was : " << elapsed1.count() << std::endl;
    // 2/22/23 - Time results suggest that frame seeking takes less than 1 ms
}

/*
Frames in a video file have unique PTS values that roughly correspond to time stamps
When we first read the video file, we keep a vector of all PTS values
in the pts member variable. So if we have a pts value, we can find the frame ID
By searching the dictionary


Note, this the pts vector should be in increasing order, so a more efficient search method 
could be used here
*/
int64_t VideoDecoder::find_frame_by_pts(uint64_t this_pts) {

    int64_t matching_frame = 0;

    for (int i = 0; i < this->pts.size(); i++) {
        if (this->pts[i] == this_pts) {
            matching_frame = i;
            break;
        }
    }

    return matching_frame;
}

}
