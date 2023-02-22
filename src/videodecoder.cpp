#include "videodecoder.h"

#include "libavinc/libavinc.hpp"

#include <functional>
#include <string>
#include <memory>
#include <algorithm>
#include <numeric>

namespace ffmpeg_wrapper {

FrameBuffer::FrameBuffer() {

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

    //Check if the position is already in the buffer
    if ( this->isFrameInBuffer(pos)) {
        std::cout << "Frame " << pos << " is already in the buffer" << std::endl;
    } else {

        if (pos - this->keyframe > this->frame_buf.size() - 1 ) {
            std::cout << "Error, buffer index equal to " << pos - this->keyframe << std::endl;
        } else {
            libav::av_frame_copy(this->frame_buf[pos - this->keyframe],frame);
            this->frame_buf_id[pos - this->keyframe] = pos;
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
    
    frame_buf = std::make_unique<FrameBuffer>();
}

VideoDecoder::VideoDecoder(const std::string& filename) {
    createMedia(filename);
}

void VideoDecoder::createMedia(const std::string& filename) {

    auto mymedia = libav::avformat_open_input(filename);
    this->media = std::move(mymedia);
    libav::av_open_best_streams(this->media);

    for (auto& pkg : this->media)
    {
        if (pkg.flags & AV_PKT_FLAG_KEY) {
            // this is I-frame. We may want to keep a list of these for fast scrolling.
            this->i_frames.push_back((this->frame_count));
        }
        this->frame_count++;
        int64_t pts = pkg.dts;
        this->pts.push_back(pts);
        this->pkt_durations.push_back(pkg.duration);
    }
    this->frame_count--;
    this->height = media->streams[0]->codecpar->height;
    this->width = media->streams[0]->codecpar->width;

    this->last_decoded_frame = frame_count;

    int64_t divisor =  std::gcd(this->getDuration() , (int64_t) this->getFrameCount() * 1000000);

    this->fps_num = this->getDuration() / divisor;
    this->fps_denom = (int64_t) this->getFrameCount() * 1000000 / divisor;
    std::cout << "FPS numerator " << fps_num << std::endl;
    std::cout << "FPS denominator " << fps_denom << std::endl;

    //std::cout << "pts contains " << pts.size() << " entries" << std::endl;
    std::cout << "i_frames contains " << i_frames.size() << " entries" << std::endl;

    int largest_diff = find_buffer_size(this->i_frames);

    //Now let's decode the first frame
    this->seekToFrame(0);
    //libav::flicks time = libav::av_rescale(0,{this->fps_num,this->fps_denom});
    //libav::av_seek_frame(this->media, time,-1,AVSEEK_FLAG_BACKWARD);
    //this->pkt = std::move(this->media.begin());
    libav::avcodec_send_packet(this->media,this->pkt.get(), [&](libav::AVFrame frame) {
    // Use that decoded frame to set up the frame buffer properties
        frame_buf->buildFrameBuffer(largest_diff,frame);
    });

    std::cout << "Buffer size set to " << largest_diff << std::endl;
    
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

    /*
    Now we may wish to either 1) seek to the nearest (backward) keyframe, and decode forward 2) decode forward from where we are already or 3) load an already decoded frame
    from the frame buffer
    */
    if (frame_buf->isFrameInBuffer(desired_frame)) {
        std::cout << "Desired frame is " << desired_frame << " and it is already in the buffer" << std::endl;

        auto frame = frame_buf->getFrameFromBuffer(desired_frame);
        yuv420togray8(frame, output); // Convert the frame to gray format to render

        return output;

    } else if (desired_frame < this_pkt_frame) {

        std::cout << "The desired frame " << desired_frame << " is before where we currently are in the video, but we don't have it in the buffer. We need to seek" << std::endl;
        this->seekToFrame(desired_frame);

    } else if (desired_nearest_iframe > this_pkt_frame) {

        std::cout << "The nearest iframe is greater than our current frame, so we should seek there instead of decoding forward" << std::endl;
        this->seekToFrame(desired_frame);

    } else if (desired_frame == this_pkt_frame) {

        std::cout << "Our current packet matches the desired frame" << std::endl;

    } else {

        std::cout << "We should seek forward from where we are" << std::endl;
        ++(this->pkt);
    }

    bool frame_to_display = false;

    while (!frame_to_display)
    {
        this_pkt_pts = this->pkt.get()->pts;
        this_pkt_frame = find_frame_by_pts(this_pkt_pts);

        // Check if our packet has the frame of interest
        if (this_pkt_pts == desired_frame_pts) {
            libav::avcodec_send_packet(this->media,this->pkt.get(), [&](libav::AVFrame frame) {

                // We have the packet we want, so we should convert to grayscale to be displayed
                frame_buf->addFrametoBuffer(frame, this_pkt_frame);
                yuv420togray8(frame,output); 
            });
            frame_to_display = true;
        } else  {
            libav::avcodec_send_packet(this->media,this->pkt.get(), [&](libav::AVFrame frame) {
                //We are not at the appropriate frame, but we just spent time decoding so we should save it to buffer
                //and we can just pull from there in the future
                frame_buf->addFrametoBuffer(frame, this_pkt_frame);
            });
            ++(this->pkt);
        }
        
    }
    this->last_decoded_frame = desired_frame; // This isn't great. Assumes everything worked
    return output;
}

void VideoDecoder::yuv420togray8(libav::AVFrame& frame,std::vector<uint8_t>& output)
{
    auto frame2 = libav::convert_frame(frame, this->width, this->height, AV_PIX_FMT_GRAY8);
    memcpy(output.data(),frame2->data[0],this->height*this->width);
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

void VideoDecoder::seekToFrame(const int frame) {
    libav::flicks time = libav::av_rescale(frame,
                {this->fps_num,this->fps_denom});

    libav::av_seek_frame(this->media, time,-1,AVSEEK_FLAG_BACKWARD);

    this->pkt = std::move(this->media.begin());

    this->last_key_frame = find_frame_by_pts(this->pkt.get()->pts);
    this->frame_buf->resetKeyFrame(this->last_key_frame);
}

/*
Frames in a video file have unique PTS values that roughly correspond to time stamps
When we first read the video file, we keep a vector of all PTS values
in the pts member variable. So if we have a pts value, we can find the frame ID
By searching the dictionary


Note, this the pts vector should be in increasing order, so a more efficient search method 
could be used here
*/
int64_t VideoDecoder::find_frame_by_pts(int64_t this_pts) {

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
