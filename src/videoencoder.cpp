#include "videoencoder.h"

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



VideoEncoder::VideoEncoder() {
    width = 640;
    height = 480;
    frame_count = 0;
}

VideoEncoder::VideoEncoder(int width, int height, int fps) {
    this->width = width;
    this->height = height;
    this->fps = fps;
}

void VideoEncoder::createContext(int width, int height, int fps) {
    this->width = width;
    this->height = height;
    this->fps = fps;

    auto mymedia = libav::avformat_open_output("test2.mp4","mp4");
    this->media = std::move(mymedia);

    auto mycodecCtx = libav::make_encode_context_nvenc(this->media,this->width, this->height, this->fps);
    this->codecCtx = std::move(mycodecCtx);

}

void VideoEncoder::set_pixel_format(INPUT_PIXEL_FORMAT pixel_fmt) {
    
    this->frame = libav::av_frame_alloc();
    this->frame->width = this->width;
    this->frame->height = this->height;

    switch (pixel_fmt) {
        case (NV12):
            libav::bind_hardware_frames_context_nvenc(this->codecCtx, this->width, this->height, ::AV_PIX_FMT_NV12);
            this->frame->format = ::AV_PIX_FMT_NV12;
            break;
        case (GRAY8):
            libav::bind_hardware_frames_context_nvenc(this->codecCtx, this->width, this->height, ::AV_PIX_FMT_NV12);
            this->frame->format = ::AV_PIX_FMT_GRAY8;
            break;
        default:
            break;
    }

    ::av_frame_get_buffer(this->frame.get(),32);

}

void VideoEncoder::openFile(std::string filename) {
    //this->file_out = ::fopen(filename.c_str(),"wb");
}

void VideoEncoder::closeFile() {
    //::fclose(this->file_out);
    ::av_write_trailer(this->media.get());
    ::avio_closep(&media->pb);
}

void VideoEncoder::writeFrameGray8(std::vector<uint8_t>& input_data) {
    ::av_frame_make_writable(this->frame.get());
    memcpy(this->frame->data[0],input_data.data(),this->height*this->width);
    libav::hardware_encode(this->media,this->codecCtx, libav::convert_frame(this->frame,this->width,this->height,::AV_PIX_FMT_NV12),this->frame_count);
    this->frame_count++;
}

int VideoEncoder::getWidth() const {
    return this->width;
}

int VideoEncoder::getHeight() const {
    return this->height;
}





