#include "videodecoder.h"

#include "libavinc.hpp"

#include <functional>
#include <string>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

namespace ffmpeg_wrapper {

VideoDecoder::VideoDecoder()
{

    frame_count = 0;
    last_decoded_frame = 0;
}

void VideoDecoder::createMedia(std::string filename) {

    auto mymedia = libav::avformat_open_input(filename);
    this->media = std::move(mymedia);
    libav::av_open_best_streams(this->media);

    for (auto& pkg : this->media)
    {
        if (pkg.flags & AV_PKT_FLAG_KEY) {
            // this is I-frame. We may want to keep a list of these for fast scrolling.
        }
        this->frame_count++;
    }
    this->frame_count--;
    this->height = media->streams[0]->codecpar->height;
    this->width = media->streams[0]->codecpar->width;

    this->last_decoded_frame = frame_count;
    
}

std::vector<uint8_t> VideoDecoder::getFrame(int frame_id,bool frame_by_frame)
{
    std::vector<uint8_t> output(this->height * this->width); // How should this be passed?

    if ((frame_id == (last_decoded_frame + 1)) | (frame_by_frame)) {
        ++(this->pkt);
    } else {
        libav::flicks time = libav::av_rescale(frame_id,{1,25});
        libav::av_seek_frame(this->media, time,-1,AVSEEK_FLAG_BACKWARD);
        this->pkt = std::move(this->media.begin());
    }

    long long frame_id_d = frame_id * this->pkt.get()->duration;

    bool frame_to_display = false;
    while (!frame_to_display)
    {
        libav::avcodec_send_packet(this->media,this->pkt.get(), [&](libav::AVFrame frame) {

              if (frame->best_effort_timestamp == frame_id_d)
              {
                     frame_to_display = true;
                     yuv420togray8(frame,output);
               } else {
                    ++(this->pkt);
               }

              last_decoded_frame = frame->best_effort_timestamp / this->pkt.get()->duration;
        });
    }

    return output;
}

void VideoDecoder::yuv420togray8(libav::AVFrame& frame,std::vector<uint8_t>& output)
{
    auto frame2 = libav::convert_frame(frame, this->width, this->height, AV_PIX_FMT_GRAY8);
    memcpy(output.data(),frame2->data[0],this->height*this->width);
}

}
