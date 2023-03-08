#include "videoencoder.h"

#include "libavinc/libavinc.hpp"

#include <functional>
#include <string>
#include <memory>
#include <filesystem>

#include <chrono>  // for high_resolution_clock
#include <iostream>

namespace ffmpeg_wrapper {

VideoEncoder::VideoEncoder() {
    width = 640;
    height = 480;
    frame_count = 0;
    file_path = "./";
    file_name = "test.mp4";
    hardware_encode = true;
    flush_state = false;
}

VideoEncoder::VideoEncoder(int width, int height, int fps) {
    this->width = width;
    this->height = height;
    this->fps = fps;
    flush_state = false;
}

//https://stackoverflow.com/questions/35530092/c-splitting-an-absolute-file-path/69990972#69990972
void VideoEncoder::setSavePath(std::string full_path) {
    std::filesystem::path path(full_path);
    this->file_path = path.parent_path().string() + "/"; 
    this->file_name = path.filename().string(); 
}

// I should be able to specify the type of context here, software or hardware
void VideoEncoder::createContext(int width, int height, int fps) {
    this->width = width;
    this->height = height;
    this->fps = fps;

    std::string full_path = this->file_path + this->file_name;
    auto mymedia = libav::avformat_open_output(full_path,"mp4");
    this->media = std::move(mymedia);

    //If hardware
    if (hardware_encode) {
        auto mycodecCtx = libav::make_encode_context_nvenc(this->media,this->width, this->height, this->fps);
        this->codecCtx = std::move(mycodecCtx);
    }  else {
        //If software - TODO
        std::cout << "Software encoding not supported at this time" << std::endl;
    }

    this->frame_count = 0; //Reset frame count to zero
}

//If we want to hardware encode, we need to bind the hardware context
//If not, we need to just make sure that we create any frames necessary for intermediate data receiving.
void VideoEncoder::set_pixel_format(INPUT_PIXEL_FORMAT pixel_fmt) {
    
    this->frame = libav::av_frame_alloc();
    this->frame->width = this->width;
    this->frame->height = this->height;

    switch (pixel_fmt) {
        case (NV12):
            libav::bind_hardware_frames_context_nvenc(this->codecCtx, this->width, this->height, libav::AV_PIX_FMT_NV12);
            this->frame->format = libav::AV_PIX_FMT_NV12;
            break;
        case (GRAY8):
            libav::bind_hardware_frames_context_nvenc(this->codecCtx, this->width, this->height, libav::AV_PIX_FMT_NV12);
            this->frame->format = libav::AV_PIX_FMT_GRAY8;
            break;
        case (RGB0):
            libav::bind_hardware_frames_context_nvenc(this->codecCtx, this->width, this->height, libav::AV_PIX_FMT_NV12);
            this->frame->format = libav::AV_PIX_FMT_RGB0;
        default:
            break;
    }

    this->frame_nv12 = libav::av_frame_alloc();
    this->frame_nv12->format = libav::AV_PIX_FMT_NV12;
    this->frame_nv12->width = this->width;
    this->frame_nv12->height = this->height;
    ::av_frame_get_buffer(this->frame_nv12.get(),32);

    ::av_frame_get_buffer(this->frame.get(),32);

}

void VideoEncoder::openFile() {

    std::string full_path = this->file_path + this->file_name;
    libav::open_encode_stream_to_write(this->media,full_path);
}

void VideoEncoder::closeFile() {

    ::av_write_trailer(this->media.get());
    ::avio_closep(&media->pb);
}

int VideoEncoder::writeFrameGray8(std::vector<uint8_t>& input_data) {

    int write_frame_err;
    if (!this->flush_state) {
        ::av_frame_make_writable(this->frame.get());
        memcpy(this->frame->data[0],input_data.data(),this->height*this->width);

        auto t1 = std::chrono::high_resolution_clock::now();
        //libav::AVFrame nvframe = libav::convert_frame(this->frame,this->width,this->height,::AV_PIX_FMT_NV12);
        libav::convert_frame(this->frame,this->frame_nv12);
        auto t2 = std::chrono::high_resolution_clock::now();

        write_frame_err = libav::hardware_encode(this->media,this->codecCtx, this->frame_nv12,this->frame_count);

        auto t3 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed1 = t2 - t1;
        std::chrono::duration<double> elapsed2 = t3 - t2;
    } else {
        //We will send null packets to the encoder
        write_frame_err = libav::hardware_encode_flush(this->media,this->codecCtx,this->frame_count);
    }
    //std::cout << "Elapsed time for scaling: " << elapsed1.count() << std::endl;
    //std::cout << "Elapsed time for encoding: " << elapsed2.count() << std::endl;

    this->frame_count++;
    return write_frame_err;
}

void VideoEncoder::writeFrameRGB0(std::vector<uint32_t>& input_data) {

    ::av_frame_make_writable(this->frame.get());
    memcpy(this->frame->data[0],input_data.data(),this->height*this->width*sizeof(uint32_t));

    libav::convert_frame(this->frame,this->frame_nv12);

    libav::hardware_encode(this->media,this->codecCtx, this->frame_nv12,this->frame_count);

    this->frame_count++;
}


}


