/*

MIT License

Copyright (c) 2019 Matthew Szatmary

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifndef LIBAVINC_HPP
#define LIBAVINC_HPP

//#pragma once

extern "C" {
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS 1
#endif
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace libav {
///////////////////////////////////////////////////////////////////////////////
static const int64_t FLICKS_TIMESCALE = 705600000;
static const ::AVRational FLICKS_TIMESCALE_Q = { 1, FLICKS_TIMESCALE };
using flicks = std::chrono::duration<int64_t, std::ratio<1, FLICKS_TIMESCALE>>;
inline flicks av_rescale(int64_t time, ::AVRational scale)
{
    return flicks(::av_rescale_q(time, scale, FLICKS_TIMESCALE_Q));
}

inline int64_t av_rescale(flicks time, ::AVRational scale)
{
    return ::av_rescale_q(time.count(), FLICKS_TIMESCALE_Q, scale);
}
///////////////////////////////////////////////////////////////////////////////
using AVDictionary = std::multimap<std::string, std::string>;

::AVDictionary* av_dictionary(const AVDictionary& dict);

AVDictionary av_dictionary(const ::AVDictionary* dict);

///////////////////////////////////////////////////////////////////////////////
using AVIOContext = std::unique_ptr<::AVIOContext, void (*)(::AVIOContext*)>;

AVIOContext avio_alloc_context(std::function<int(uint8_t*, int)> read,
    std::function<int(uint8_t*, int)> write,
    std::function<int64_t(int64_t, int)> seek,
    int buffer_size = 4096, int write_flag = 1);

AVIOContext avio_alloc_context(std::function<int(uint8_t*, int)> read,
    std::function<int64_t(int64_t, int)> seek, int buffer_size = 4096);

///////////////////////////////////////////////////////////////////////////////
int av_read_frame(::AVFormatContext* ctx, ::AVPacket* pkt);

// AVPacket is extended to enable ranged for loop
using AVPacketBase = std::unique_ptr<::AVPacket, void (*)(::AVPacket*)>;
class AVPacket : public AVPacketBase {
private:
    ::AVFormatContext* fmtCtx = nullptr;
    // From Video
    void av_read_frame()
    {
        if (*this) {
            auto err = ::av_read_frame(fmtCtx, get());
            if (0 <= err) {
                auto& track = fmtCtx->streams[get()->stream_index];
                ::av_packet_rescale_ts(get(), track->time_base, FLICKS_TIMESCALE_Q);
            } else {
                reset();
            }
        }
    }
    // End From Video
public:
    AVPacket()
        : AVPacketBase(nullptr, [](::AVPacket*) {})
        , fmtCtx(nullptr)
    {
    }

    AVPacket(::AVFormatContext* fmtCtx)
    //    : AVPacketBase(::av_packet_alloc(), [](::AVPacket* p) { ::av_packet_free(&p); }) Original Code
        : AVPacketBase(::av_packet_alloc(), [](::AVPacket* pkt) {auto p = &pkt; ::av_packet_free(p); }) // From Video
        , fmtCtx(fmtCtx)
    {
        libav::av_read_frame(fmtCtx, get());
    }

    AVPacket& operator++()
    {
        if (!(*this)) {
            return *this;
        }

        if (0 >= get()->size) {
            reset();
        } else {
            libav::av_read_frame(fmtCtx, get());
        }

        return *this;
    }
};

AVPacket av_packet_alloc();

AVPacket av_packet_clone(const ::AVPacket* src);

AVPacket av_packet_clone(const AVPacket& src);

///////////////////////////////////////////////////////////////////////////////
using AVBufferRefBase = std::unique_ptr<::AVBufferRef,void (*)(::AVBufferRef*)>;
class AVBufferRef : public AVBufferRefBase {
public:
    AVBufferRef()
        : AVBufferRefBase(nullptr, [](::AVBufferRef*) {})
    {
    }


};
///////////////////////////////////////////////////////////////////////////////
using AVFormatContextBase = std::unique_ptr<::AVFormatContext, void (*)(::AVFormatContext*)>;
using AVCodecContext = std::unique_ptr<::AVCodecContext, void (*)(::AVCodecContext*)>;
class AVFormatContext : public AVFormatContextBase {
public:
    AVFormatContext()
        : AVFormatContextBase(nullptr, [](::AVFormatContext*) {})
    {
    }

    template <class T>
    AVFormatContext(::AVFormatContext* fmtCtx, T deleter)
        : AVFormatContextBase(fmtCtx, deleter)
    {
    }

    AVPacket end() { return AVPacket(); }
    AVPacket begin() { return AVPacket(get()); }
    std::map<int, AVCodecContext> open_streams; // Original
    //std::map<int, std::unique_ptr<::AVCodecContext, void (*)(::AVCodecContext*)>> open_streams; #From Video Lecture
};

AVFormatContext avformat_open_input(const std::string& url, const AVDictionary& options = AVDictionary());

int av_seek_frame(AVFormatContext& ctx, flicks time, int idx = -1, int flags = 0);

AVFormatContext avformat_open_output(const std::string& url, std::string format_name = std::string());

///////////////////////////////////////////////////////////////////////////////
int avformat_new_stream(AVFormatContext& fmtCtx, const ::AVCodecParameters* par);

int av_interleaved_write_frame(AVFormatContext& fmtCtx, int stream_index, const ::AVPacket& pkt);
///////////////////////////////////////////////////////////////////////////////
using AVFrame = std::shared_ptr<::AVFrame>;

AVFrame av_frame_alloc();

AVFrame av_frame_clone(const ::AVFrame* frame);

AVFrame av_frame_clone(const AVFrame& frame);

AVFrame convert_frame(AVFrame& frame, int width_out, int height_out, ::AVPixelFormat pix_out);

///////////////////////////////////////////////////////////////////////////////
int av_open_best_stream(AVFormatContext& fmtCtx, AVMediaType type, int related_stream = -1);

int av_open_best_streams(AVFormatContext& fmtCtx);

AVCodecContext& find_open_audio_stream(AVFormatContext& fmtCtx);

AVCodecContext& find_open_video_stream(AVFormatContext& fmtCtx);

int avcodec_send_packet(AVFormatContext& fmtCtx, ::AVPacket* pkt,
    std::function<void(AVFrame)> onFrame);

int avcodec_send_packet(AVFormatContext& fmtCtx,
    std::function<void(AVFrame)> onFrame);

///////////////////////////////////////////////////////////////////////////////
using AVFilterGraphBase = std::unique_ptr<::AVFilterGraph, void (*)(::AVFilterGraph*)>;
class AVFilterGraph : public AVFilterGraphBase {
public:
    class AVFilterArgs : public std::string {
    public:
        AVFilterArgs(const char* fmt, ...)
            : std::string(1024, 0)
        {
            va_list vargs;
            va_start(vargs, fmt);
            resize(vsnprintf((char*)data(), size(), fmt, vargs));
            va_end(vargs);
        }
    };

    class AVFilterChain {
    public:
        std::string name;
        std::string args;
        AVFilterChain() = default;
        AVFilterChain(std::string name, AVFilterArgs args)
            : name(name)
            , args(args)
        {
        }

        ::AVFilterContext* create_filter(::AVFilterContext* source, ::AVFilterGraph* graph) const
        {
            ::AVFilterContext* sink = nullptr;
            auto err = ::avfilter_graph_create_filter(&sink, avfilter_get_by_name(name.c_str()), nullptr, args.c_str(), nullptr, graph);
            err = ::avfilter_link(source, 0, sink, 0);
            return sink;
        }
    };

    ::AVFilterContext* sink = nullptr;
    ::AVFilterContext* source = nullptr;
    AVFilterGraph()
        : AVFilterGraphBase(nullptr, [](::AVFilterGraph*) {})
    {
    }

    AVFilterGraph(AVFormatContext& fmtCtx, int stream, const std::vector<AVFilterChain>& chain)
        : AVFilterGraphBase(::avfilter_graph_alloc(), [](::AVFilterGraph* p) { avfilter_graph_free(&p); })
    {
        auto& codecpar = fmtCtx->streams[stream]->codecpar;
        AVFilterChain buffer("buffer",
            { "video_size=%dx%d:pix_fmt=%d:time_base=%ld/%ld:pixel_aspect=%d/%d",
                codecpar->width, codecpar->height, codecpar->format,
                flicks::period::num, flicks::period::den,
                codecpar->sample_aspect_ratio.num, codecpar->sample_aspect_ratio.den });

        ::avfilter_graph_create_filter(&source, avfilter_get_by_name(buffer.name.c_str()), nullptr, buffer.args.c_str(), nullptr, get());
        auto _source = source;
        for (const auto& link : chain) {
            _source = link.create_filter(_source, get());
        }

        // Create sink
        enum AVPixelFormat pix_fmts[] = { (AVPixelFormat)codecpar->format, AV_PIX_FMT_NONE };
        AVBufferSinkParams* buffersink_params = av_buffersink_params_alloc();
        buffersink_params->pixel_fmts = pix_fmts;
        auto err = avfilter_graph_create_filter(&sink, avfilter_get_by_name("buffersink"), nullptr, nullptr, buffersink_params, get());
        err = ::avfilter_link(_source, 0, sink, 0);
        ::av_free(buffersink_params);
        err = ::avfilter_graph_config(get(), nullptr);
    }
};

/*
inline int avfilter_graph_write_frame(AVFilterGraph& graph, AVFrame& frame, std::function<void(AVFrame&)> onFrame)
{
    int err = 0;
    if ((err = ::av_buffersrc_write_frame(graph.source, frame.get())) < 0) {
        return err;
    }

    for (;;) {
        auto newFrame = av_frame_alloc();
        if (::av_buffersink_get_frame(graph.sink, newFrame.get()) < 0) {
            break;
        }

        onFrame(newFrame);
    }

    return err == AVERROR(EAGAIN) ? 0 : err;
}
*/

///////////////////////////////////////////////////////////////////////////////

AVCodecContext make_encode_context(std::string codec_name,int width, int height, int fps, ::AVPixelFormat pix_fmt);

AVCodecContext make_encode_context_nvenc(int width, int height, int fps);

void bind_hardware_frames_context(AVCodecContext& ctx, int width, int height, ::AVPixelFormat hw_pix_fmt,::AVPixelFormat sw_pix_fmt);

void bind_hardware_frames_context_nvenc(AVCodecContext& ctx, int width, int height, ::AVPixelFormat sw_pix_fmt);

void hardware_encode(FILE * pFile,AVCodecContext& ctx,AVFrame& hw_frame, AVFrame& sw_frame);

} // End namespace

#endif // LIBAVINC_HPP
