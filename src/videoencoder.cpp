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

bool mux_video_and_audio(std::string const & video_file,
                         std::string const & audio_file,
                         std::string const & output_file,
                         int video_fps) {
    AVFormatContext * video_ctx = nullptr;
    AVFormatContext * audio_ctx = nullptr;
    AVFormatContext * out_ctx = nullptr;
    int ret = 0;
    bool success = false;

    // Open video input
    ret = avformat_open_input(&video_ctx, video_file.c_str(), nullptr, nullptr);
    if (ret < 0) {
        std::cerr << "Failed to open video file: " << video_file << std::endl;
        return false;
    }
    avformat_find_stream_info(video_ctx, nullptr);

    // Open audio input
    ret = avformat_open_input(&audio_ctx, audio_file.c_str(), nullptr, nullptr);
    if (ret < 0) {
        std::cerr << "Failed to open audio file: " << audio_file << std::endl;
        avformat_close_input(&video_ctx);
        return false;
    }
    avformat_find_stream_info(audio_ctx, nullptr);

    // Create output context
    avformat_alloc_output_context2(&out_ctx, nullptr, nullptr, output_file.c_str());
    if (!out_ctx) {
        std::cerr << "Failed to create output context" << std::endl;
        avformat_close_input(&video_ctx);
        avformat_close_input(&audio_ctx);
        return false;
    }

    // Copy video stream
    AVStream * in_video_stream = nullptr;
    AVStream * out_video_stream = nullptr;
    for (unsigned int i = 0; i < video_ctx->nb_streams; i++) {
        if (video_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            in_video_stream = video_ctx->streams[i];
            out_video_stream = avformat_new_stream(out_ctx, nullptr);
            if (!out_video_stream) {
                std::cerr << "Failed to create output video stream" << std::endl;
                if (out_ctx && !(out_ctx->oformat->flags & AVFMT_NOFILE)) {
                    avio_closep(&out_ctx->pb);
                }
                avformat_free_context(out_ctx);
                avformat_close_input(&video_ctx);
                avformat_close_input(&audio_ctx);
                return false;
            }
            avcodec_parameters_copy(out_video_stream->codecpar, in_video_stream->codecpar);
            out_video_stream->codecpar->codec_tag = 0;
            out_video_stream->time_base = in_video_stream->time_base;
            break;
        }
    }

    // Copy audio stream
    AVStream * in_audio_stream = nullptr;
    AVStream * out_audio_stream = nullptr;
    for (unsigned int i = 0; i < audio_ctx->nb_streams; i++) {
        if (audio_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            in_audio_stream = audio_ctx->streams[i];
            out_audio_stream = avformat_new_stream(out_ctx, nullptr);
            if (!out_audio_stream) {
                std::cerr << "Failed to create output audio stream" << std::endl;
                if (out_ctx && !(out_ctx->oformat->flags & AVFMT_NOFILE)) {
                    avio_closep(&out_ctx->pb);
                }
                avformat_free_context(out_ctx);
                avformat_close_input(&video_ctx);
                avformat_close_input(&audio_ctx);
                return false;
            }
            avcodec_parameters_copy(out_audio_stream->codecpar, in_audio_stream->codecpar);
            out_audio_stream->codecpar->codec_tag = 0;
            out_audio_stream->time_base = in_audio_stream->time_base;
            break;
        }
    }

    // Open output file
    if (!(out_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&out_ctx->pb, output_file.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            std::cerr << "Failed to open output file: " << output_file << std::endl;
            avformat_free_context(out_ctx);
            avformat_close_input(&video_ctx);
            avformat_close_input(&audio_ctx);
            return false;
        }
    }

    // Write header
    ret = avformat_write_header(out_ctx, nullptr);
    if (ret < 0) {
        std::cerr << "Failed to write output header" << std::endl;
        if (out_ctx && !(out_ctx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&out_ctx->pb);
        }
        avformat_free_context(out_ctx);
        avformat_close_input(&video_ctx);
        avformat_close_input(&audio_ctx);
        return false;
    }

    // Copy video packets
    AVPacket pkt;
    while (av_read_frame(video_ctx, &pkt) >= 0) {
        if (pkt.stream_index == in_video_stream->index) {
            pkt.stream_index = out_video_stream->index;
            av_packet_rescale_ts(&pkt, in_video_stream->time_base, out_video_stream->time_base);
            pkt.pos = -1;
            ret = av_interleaved_write_frame(out_ctx, &pkt);
            if (ret < 0) {
                std::cerr << "Error writing video packet" << std::endl;
            }
        }
        av_packet_unref(&pkt);
    }

    // Copy audio packets
    while (av_read_frame(audio_ctx, &pkt) >= 0) {
        if (pkt.stream_index == in_audio_stream->index) {
            pkt.stream_index = out_audio_stream->index;
            av_packet_rescale_ts(&pkt, in_audio_stream->time_base, out_audio_stream->time_base);
            pkt.pos = -1;
            ret = av_interleaved_write_frame(out_ctx, &pkt);
            if (ret < 0) {
                std::cerr << "Error writing audio packet" << std::endl;
            }
        }
        av_packet_unref(&pkt);
    }

    // Write trailer
    av_write_trailer(out_ctx);
    success = true;

    // Cleanup
    if (out_ctx && !(out_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&out_ctx->pb);
    }
    avformat_free_context(out_ctx);
    avformat_close_input(&video_ctx);
    avformat_close_input(&audio_ctx);

    return success;
}


}// namespace ffmpeg_wrapper
