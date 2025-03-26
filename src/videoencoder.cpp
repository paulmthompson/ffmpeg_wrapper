#include "videoencoder.h"

#include "libavinc/libavinc.hpp"

#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavutil/frame.h"
#include "libavutil/pixfmt.h"

#include <chrono>
#include <cstring>//memcpy
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <utility>


namespace ffmpeg_wrapper {

VideoEncoder::VideoEncoder() {
}

VideoEncoder::VideoEncoder(int width, int height, int fps) {
    _width = width;
    _height = height;
    _fps = fps;
    _flush_state = false;
}

//https://stackoverflow.com/questions/35530092/c-splitting-an-absolute-file-path/69990972#69990972
void VideoEncoder::setSavePath(std::string full_path) {
    std::filesystem::path path(full_path);
    _file_path = path.parent_path().string() + "/";
    _file_name = path.filename().string();
}

// I should be able to specify the type of context here, software or hardware
void VideoEncoder::createContext(int width, int height, int fps) {
    _width = width;
    _height = height;
    _fps = fps;

    std::string full_path = _file_path + _file_name;
    auto mymedia = libav::avformat_open_output(full_path, "mp4");
    _media = std::move(mymedia);

    //If hardware
    if (_hardware_encode) {
        auto mycodecCtx = libav::make_encode_context_nvenc(_media, _width, _height, _fps);
        _codecCtx = std::move(mycodecCtx);
    } else {
        //If software - TODO
        std::cout << "Software encoding not supported at this time" << std::endl;
    }

    _frame_count = 0;//Reset frame count to zero
}

//If we want to hardware encode, we need to bind the hardware context
//If not, we need to just make sure that we create any frames necessary for intermediate data receiving.
void VideoEncoder::set_pixel_format(INPUT_PIXEL_FORMAT pixel_fmt) {

    _frame = libav::av_frame_alloc();
    _frame->width = _width;
    _frame->height = _height;

    switch (pixel_fmt) {
        case (NV12):
            libav::bind_hardware_frames_context_nvenc(_codecCtx, _width, _height, libav::AV_PIX_FMT_NV12);
            _frame->format = libav::AV_PIX_FMT_NV12;
            break;
        case (GRAY8):
            libav::bind_hardware_frames_context_nvenc(_codecCtx, _width, _height, libav::AV_PIX_FMT_NV12);
            _frame->format = libav::AV_PIX_FMT_GRAY8;
            break;
        case (RGB0):
            libav::bind_hardware_frames_context_nvenc(_codecCtx, _width, _height, libav::AV_PIX_FMT_NV12);
            _frame->format = libav::AV_PIX_FMT_RGB0;
        default:
            break;
    }

    _frame_nv12 = libav::av_frame_alloc();
    _frame_nv12->format = libav::AV_PIX_FMT_NV12;
    _frame_nv12->width = _width;
    _frame_nv12->height = _height;
    ::av_frame_get_buffer(_frame_nv12.get(), 32);

    ::av_frame_get_buffer(_frame.get(), 32);
}

void VideoEncoder::openFile() {

    std::string full_path = _file_path + _file_name;
    libav::open_encode_stream_to_write(_media, full_path);
}

void VideoEncoder::closeFile() {

    ::av_write_trailer(_media.get());
    ::avio_closep(&_media->pb);
}

int VideoEncoder::writeFrameGray8(std::vector<uint8_t> & input_data) {

    int write_frame_err;
    if (!_flush_state) {
        ::av_frame_make_writable(_frame.get());
        std::memcpy(_frame->data[0], input_data.data(), _height * _width);

        auto t1 = std::chrono::high_resolution_clock::now();
        //libav::AVFrame nvframe = libav::convert_frame(frame,width,height,::AV_PIX_FMT_NV12);
        libav::convert_frame(_frame, _frame_nv12);
        auto t2 = std::chrono::high_resolution_clock::now();

        write_frame_err = libav::hardware_encode(_media, _codecCtx, _frame_nv12, _frame_count);

        auto t3 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed1 = t2 - t1;
        std::chrono::duration<double> elapsed2 = t3 - t2;
    } else {
        //We will send null packets to the encoder
        write_frame_err = libav::hardware_encode_flush(_media, _codecCtx, _frame_count);
    }
    //std::cout << "Elapsed time for scaling: " << elapsed1.count() << std::endl;
    //std::cout << "Elapsed time for encoding: " << elapsed2.count() << std::endl;

    _frame_count++;
    return write_frame_err;
}

void VideoEncoder::writeFrameRGB0(std::vector<uint32_t> & input_data) {

    ::av_frame_make_writable(_frame.get());
    memcpy(_frame->data[0], input_data.data(), _height * _width * sizeof(uint32_t));

    libav::convert_frame(_frame, _frame_nv12);

    libav::hardware_encode(_media, _codecCtx, _frame_nv12, _frame_count);

    _frame_count++;
}


}// namespace ffmpeg_wrapper
