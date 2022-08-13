#include "videodecoder.h"

#include "libavinc/libavinc.hpp"

#include <functional>
#include <string>
#include <memory>

namespace ffmpeg_wrapper {

VideoDecoder::VideoDecoder()
{

    frame_count = 0;
    last_decoded_frame = 0;
    pts.reserve(500000);
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
        }
        this->frame_count++;
        int64_t pts = pkg.pts;
        this->pts.push_back(pts);
    }
    this->frame_count--;
    this->height = media->streams[0]->codecpar->height;
    this->width = media->streams[0]->codecpar->width;

    this->last_decoded_frame = frame_count;

    std::cout << "pts contains " << pts.size() << " entries" << std::endl;
    
}

std::vector<uint8_t> VideoDecoder::getFrame(int frame_id,bool frame_by_frame)
{
    std::vector<uint8_t> output(this->height * this->width); // How should this be passed?

    if ((frame_id == (this->last_decoded_frame + 1)) & (frame_by_frame)) {
        libav::flicks time = libav::av_rescale(frame_id,
                {this->getDuration(),this->getFrameCount() * 1000 * 1000});
        ++(this->pkt);
    } else {
        if (this->getStartTime() == 0) {
            libav::flicks time = libav::av_rescale(frame_id,
                {this->getDuration(),this->getFrameCount() * 1000 * 1000});
            libav::av_seek_frame(this->media, time,-1,AVSEEK_FLAG_BACKWARD);
        } else {
            //https://libav-user.ffmpeg.narkive.com/tW3tNrlA/use-avpacket-pts-with-av-seek-frame
            //Relationships between pts and time is given here
            const int64_t time_stamp = this->pts[frame_id] / 1000;
            libav::av_seek_frame(this->media, time_stamp,-1,AVSEEK_FLAG_BACKWARD);
        }
        this->pkt = std::move(this->media.begin());
    }

    long long frame_id_d = frame_id * this->pkt.get()->duration;

    const int64_t desired_pts = this->pts[frame_id];

    bool frame_to_display = false;
    while (!frame_to_display)
    {
        if (this->pkt.get()->pts == desired_pts) {
            libav::avcodec_send_packet(this->media,this->pkt.get(), [&](libav::AVFrame frame) {
                yuv420togray8(frame,output);
            });
        frame_to_display = true;
        } else {
            ++(this->pkt);
        }
    }
    this->last_decoded_frame = frame_id; // This isn't great. Assumes everything worked

    return output;
}

void VideoDecoder::yuv420togray8(libav::AVFrame& frame,std::vector<uint8_t>& output)
{
    auto frame2 = libav::convert_frame(frame, this->width, this->height, AV_PIX_FMT_GRAY8);
    memcpy(output.data(),frame2->data[0],this->height*this->width);
}

}
