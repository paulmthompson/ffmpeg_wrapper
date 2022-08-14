#include "videodecoder.h"

#include "libavinc/libavinc.hpp"

#include <functional>
#include <string>
#include <memory>
#include <algorithm>

namespace ffmpeg_wrapper {

VideoDecoder::VideoDecoder()
{

    frame_count = 0;
    last_decoded_frame = 0;
    pts.reserve(500000);
    //fps = 25;
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

    int64_t divisor =  std::__gcd(this->getDuration() , (int64_t) this->getFrameCount() * 1000000);

    this->fps_num = this->getDuration() / divisor;
    this->fps_denom = (int64_t) this->getFrameCount() * 1000000 / divisor;
    std::cout << "FPS numerator " << fps_num << std::endl;
    std::cout << "FPS denominator " << fps_denom << std::endl;

    //std::cout << "pts contains " << pts.size() << " entries" << std::endl;
    std::cout << "i_frames contains " << i_frames.size() << " entries" << std::endl;
    //for (int i = 0; i < this->i_frames.size(); i++) {
    //    std::cout << this->i_frames[i] << std::endl;
    //}
    
}

std::vector<uint8_t> VideoDecoder::getFrame(int frame_id,bool frame_by_frame)
{
    std::vector<uint8_t> output(this->height * this->width); // How should this be passed?

    if (frame_id == (this->last_decoded_frame + 1)) {
        ++(this->pkt);
    } else if ((frame_id > this->last_decoded_frame) & (frame_by_frame)) {
        ++(this->pkt);
    } else {
        libav::flicks time = libav::av_rescale(frame_id,
                {this->fps_num,this->fps_denom});
        libav::av_seek_frame(this->media, time,-1,AVSEEK_FLAG_BACKWARD);

        this->pkt = std::move(this->media.begin());
    }

    const int64_t desired_pts = this->pts[frame_id];

    bool frame_to_display = false;
    int decoded_frames = 0;
    while (!frame_to_display)
    {
        if (this->pkt.get()->pts == desired_pts) {
            libav::avcodec_send_packet(this->media,this->pkt.get(), [&](libav::AVFrame frame) {
                yuv420togray8(frame,output);
            });
            frame_to_display = true;
        } else  {
            libav::avcodec_send_packet(this->media,this->pkt.get(), [](libav::AVFrame frame) {});
            ++(this->pkt);
        }
        decoded_frames ++;
        
    }
    this->last_decoded_frame = frame_id; // This isn't great. Assumes everything worked
    std::cout << "Decoded " << decoded_frames << std::endl;
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

    //std::cout << "Nearest i_frame: " << frame_id - nearest_i_frame << std::endl;
    //std::cout << "nearest i_frame pts: " << this->pts[nearest_i_frame] << std::endl;

    return nearest_i_frame;
}

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
