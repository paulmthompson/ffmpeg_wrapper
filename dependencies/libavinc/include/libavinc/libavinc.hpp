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

#pragma once

extern "C" {
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS 1
#endif
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#if defined _WIN32 || defined __CYGWIN__
#define DLLOPT __declspec(dllexport)
#else
#define DLLOPT __attribute__((visibility("default")))
#endif

#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace libav {
///////////////////////////////////////////////////////////////////////////////
static int64_t const FLICKS_TIMESCALE = 705600000;
static ::AVRational const FLICKS_TIMESCALE_Q = {1, FLICKS_TIMESCALE};
using flicks = std::chrono::duration<int64_t, std::ratio<1, FLICKS_TIMESCALE>>;
inline flicks av_rescale(int64_t time, ::AVRational scale) {
    return flicks(::av_rescale_q(time, scale, FLICKS_TIMESCALE_Q));
}

inline int64_t av_rescale(flicks time, ::AVRational scale) {
    return ::av_rescale_q(time.count(), FLICKS_TIMESCALE_Q, scale);
}
///////////////////////////////////////////////////////////////////////////////
using ::AV_PIX_FMT_ARGB;
using ::AV_PIX_FMT_GRAY8;
using ::AV_PIX_FMT_NV12;
using ::AV_PIX_FMT_RGB0;

///////////////////////////////////////////////////////////////////////////////
using AVDictionary = std::multimap<std::string, std::string>;

inline ::AVDictionary * av_dictionary(AVDictionary const & dict) {
    ::AVDictionary * d = nullptr;
    for (auto const & entry: dict) {
        ::av_dict_set(&d, entry.first.c_str(), entry.second.c_str(), 0);
    }
    return d;
}

inline AVDictionary av_dictionary(::AVDictionary const * dict) {
    libav::AVDictionary d;
    ::AVDictionaryEntry * entry = nullptr;
    while ((entry = ::av_dict_get(dict, "", entry, AV_DICT_IGNORE_SUFFIX))) {
        d.emplace(entry->key, entry->value);
    }
    return d;
}

inline void av_dict_free(::AVDictionary * d) {
    if (d) {
        ::av_dict_free(&d);
    }
}


///////////////////////////////////////////////////////////////////////////////
using AVIOContext = std::unique_ptr<::AVIOContext, void (*)(::AVIOContext *)>;

/**
* @brief Allocates and initializes an AVIOContext for custom I/O operations.
*
* This function creates an AVIOContext that can be used for custom input/output operations
* in FFmpeg. It allows the user to define custom read, write, and seek operations through
* std::function callbacks.
*
* @param read A std::function for the read operation. It should take a buffer and its size as parameters,
*             returning the number of bytes read. It matches the signature `int(uint8_t* buf, int buf_size)`.
* @param write A std::function for the write operation. It should take a buffer and its size as parameters,
*              returning the number of bytes written. It matches the signature `int(uint8_t* buf, int buf_size)`.
*              This parameter is used only if the write_flag is set to enable writing.
* @param seek A std::function for the seek operation. It should take an offset and a whence as parameters,
*             returning the new offset from the beginning of the stream. It matches the signature
*             `int64_t(int64_t offset, int whence)`.
* @param buffer_size The size of the internal buffer FFmpeg will use for I/O operations, in bytes.
* @param write_flag An integer flag indicating if writing should be enabled (non-zero) or disabled (zero).
*                   When writing is disabled, the write function will not be used.
*
* @return An AVIOContext wrapped in a std::unique_ptr with a custom deleter. This context should be used
*         with FFmpeg functions that require an AVIOContext. The custom deleter ensures that resources
*         are properly freed when the context is no longer needed.
*/
inline AVIOContext avio_alloc_context(std::function<int(uint8_t *, int)> read,
                                      std::function<int(uint8_t *, int)> write,
                                      std::function<int64_t(int64_t, int)> seek,
                                      int buffer_size,
                                      int write_flag) {
    struct opaque_t {
        std::function<int(uint8_t *, int)> read;
        std::function<int(uint8_t *, int)> write;
        std::function<int64_t(int64_t, int)> seek;
    };

    // create a copy of the functions to enable captures
    auto buffer = (unsigned char *) ::av_malloc(buffer_size);
    auto opaque = new opaque_t{read, write, seek};

    auto read_wrapper = [](void * opaque, uint8_t * buf, int buf_size) -> int {
        auto o = reinterpret_cast<opaque_t *>(opaque);
        return o->read(buf, buf_size);
    };

    auto write_wrapper = [](void * opaque, uint8_t * buf, int buf_size) -> int {
        auto o = reinterpret_cast<opaque_t *>(opaque);
        return o->write(buf, buf_size);
    };

    auto seek_wrapper = [](void * opaque, int64_t offset, int whence) -> int64_t {
        auto o = reinterpret_cast<opaque_t *>(opaque);
        return o->seek(offset, whence);
    };

    return AVIOContext(
            ::avio_alloc_context(
                    buffer,
                    buffer_size,
                    write_flag,
                    opaque,
                    read_wrapper,
                    write_wrapper,
                    seek_wrapper),
            [](::AVIOContext * c) {
                delete reinterpret_cast<opaque_t *>(c->opaque);
                av_free(c->buffer),
                        c->buffer = nullptr;
                ::avio_context_free(&c);
            });
}

/**
* Allocates and initializes an AVIOContext with custom read and seek operations, creating a read-only context.
* This function is a convenience wrapper that sets up a read-only AVIOContext by providing custom read and seek
* functions, while disabling the write functionality. It is particularly useful for scenarios where only reading
* and seeking operations are required, and writing to the stream is either not needed or not supported.
*
* @param read A std::function object representing the custom read operation. It should match the signature
*             `int(uint8_t* buf, int buf_size)` where `buf` is a pointer to the buffer where the read data should
*             be stored, and `buf_size` is the size of the buffer. The function should return the number of bytes
*             read, or a negative value on error.
* @param seek A std::function object representing the custom seek operation. It should match the signature
*             `int64_t(int64_t offset, int whence)` where `offset` is the offset from the location specified by
*             `whence`, which can be one of the standard `SEEK_SET`, `SEEK_CUR`, or `SEEK_END`. The function should
*             return the resulting offset location as measured in bytes from the beginning of the stream, or a
*             negative value on error.
* @param buffer_size The size of the buffer to be used for I/O operations in bytes. This value should be chosen
*                    based on the expected workload and stream characteristics for optimal performance.
* @return Returns an AVIOContext object wrapped in a std::unique_ptr with a custom deleter. The AVIOContext is
*         configured for read-only access with the provided read and seek functions.
*/
inline AVIOContext avio_alloc_context(std::function<int(uint8_t *, int)> read,
                                      std::function<int64_t(int64_t, int)> seek,
                                      int buffer_size) {
    auto write = [](uint8_t *, int) { return 0; };
    return libav::avio_alloc_context(
            read, write, seek, 0, buffer_size);
}

///////////////////////////////////////////////////////////////////////////////

inline int DLLOPT av_read_frame(::AVFormatContext * ctx, ::AVPacket * pkt) {
    if (!ctx || !pkt) {
        return AVERROR(1);
    }

    auto err = ::av_read_frame(ctx, pkt);
    if (0 <= err) {
        auto & track = ctx->streams[pkt->stream_index];
        ::av_packet_rescale_ts(pkt, track->time_base, FLICKS_TIMESCALE_Q);
        // TODO check for pkt->size == 0 but not EOF
    } else {
        //pkt = ::av_packet_alloc();
        pkt->size = 0;
        pkt->data = nullptr;
    }

    return err;
}


// AVPacket is extended to enable ranged for loop
struct PktDeleter {
    void operator()(::AVPacket * pkt) { ::av_packet_free(&pkt); }
};

using AVPacketBase = std::unique_ptr<::AVPacket, PktDeleter>;
class DLLOPT AVPacket : public AVPacketBase {
private:
    ::AVFormatContext * _fmtCtx = nullptr;

    // From Video
    /*
    void av_read_frame() {
        if (*this) {
            auto err = ::av_read_frame(_fmtCtx, get());
            if (0 <= err) {
                auto & track = _fmtCtx->streams[get()->stream_index];
                ::av_packet_rescale_ts(get(), track->time_base, FLICKS_TIMESCALE_Q);
            } else {
                reset();
            }
        }
    }
    */
    // End From Video
public:
    AVPacket()
        : AVPacketBase(nullptr),
          _fmtCtx(nullptr) {
    }

    AVPacket(::AVFormatContext * fmtCtx)
        : AVPacketBase(::av_packet_alloc())
          //     : AVPacketBase(::av_packet_alloc(), [](::AVPacket* p) {::av_packet_free(&p); }) //Original Code
          // : AVPacketBase(::av_packet_alloc(), [](::AVPacket * pkt) {auto p = &pkt; ::av_packet_free(p); })// From Video
          ,
          _fmtCtx(fmtCtx) {
        libav::av_read_frame(fmtCtx, get());
    }

    AVPacket & operator++() {

        if (!(*this)) {
            return *this;
        }

        /*
        if (0 >= get()->size) {
            reset();
        } else {
            libav::av_read_frame(_fmtCtx, get());
        }
        */
        auto err = libav::av_read_frame(_fmtCtx, get());
        if (0 > err) {
            reset();
        }

        return *this;
    }
};

inline AVPacket av_packet_alloc() {
    return AVPacket(nullptr);
}

inline AVPacket av_packet_clone(::AVPacket const * src) {
    auto newPkt = AVPacket(nullptr);
    if (::av_packet_ref(newPkt.get(), src) < 0) {
        newPkt.reset();
    }
    return newPkt;
}

inline AVPacket av_packet_clone(AVPacket const & src) {
    return libav::av_packet_clone(src.get());
}

///////////////////////////////////////////////////////////////////////////////
using AVBufferRefBase = std::unique_ptr<::AVBufferRef, void (*)(::AVBufferRef *)>;
class AVBufferRef : public AVBufferRefBase {
public:
    AVBufferRef()
        : AVBufferRefBase(nullptr, [](::AVBufferRef *) {}) {
    }
};
///////////////////////////////////////////////////////////////////////////////
using AVFormatContextBase = std::unique_ptr<::AVFormatContext, void (*)(::AVFormatContext *)>;
using AVCodecContextBase = std::unique_ptr<::AVCodecContext, void (*)(::AVCodecContext *)>;
class DLLOPT AVCodecContext : public AVCodecContextBase {
public:
    AVCodecContext()
        : AVCodecContextBase(nullptr, [](::AVCodecContext *) {}) {
    }

    template<class T>
    AVCodecContext(::AVCodecContext * Ctx, T deleter)
        : AVCodecContextBase(Ctx, deleter) {
    }

    void flush_buffers() {
        ::avcodec_flush_buffers(get());
    }
};
class DLLOPT AVFormatContext : public AVFormatContextBase {
public:
    AVFormatContext()
        : AVFormatContextBase(nullptr, [](::AVFormatContext *) {}) {
    }

    template<class T>
    AVFormatContext(::AVFormatContext * fmtCtx, T deleter)
        : AVFormatContextBase(fmtCtx, deleter) {
    }

    AVPacket end() { return AVPacket(); }
    AVPacket begin() { return AVPacket(get()); }
    std::map<int, AVCodecContext> open_streams;// Original
    //std::map<int, std::unique_ptr<::AVCodecContext, void (*)(::AVCodecContext*)>> open_streams; #From Video Lecture
};

inline AVFormatContext avformat_open_input(std::string const & url, AVDictionary const & options = AVDictionary()) {
    ::AVFormatContext * fmtCtx = nullptr;
    auto avdict = libav::av_dictionary(options);
    auto err = ::avformat_open_input(&fmtCtx, url.c_str(), nullptr, &avdict);
    libav::av_dict_free(avdict);
    if (err < 0 || !fmtCtx) {
        return libav::AVFormatContext();
    }

    err = ::avformat_find_stream_info(fmtCtx, nullptr);
    if (err < 0) {
        ::avformat_close_input(&fmtCtx);
        return libav::AVFormatContext();
    }

    return libav::AVFormatContext(fmtCtx, [](::AVFormatContext * ctx) {
        auto p_ctx = &ctx;
        ::avformat_close_input(p_ctx);
    });
}

inline int av_seek_frame(AVFormatContext & ctx, flicks time, int idx = -1, int flags = 0) {
    ::AVRational time_base;
    if (0 <= idx) {
        time_base = ctx->streams[idx]->time_base;
    } else {
        time_base = ::av_get_time_base_q();
    }
    return ::av_seek_frame(ctx.get(), idx, av_rescale(time, time_base), flags);
}

inline int av_seek_frame(AVFormatContext & ctx, int64_t frame, int idx = -1, int flags = 0) {
    return ::av_seek_frame(ctx.get(), idx, frame, flags);
}

inline AVFormatContext avformat_open_output(std::string const & url, std::string format_name = std::string()) {
    ::AVFormatContext * fmtCtx = nullptr;
    auto err = ::avformat_alloc_output_context2(&fmtCtx, nullptr, format_name.empty() ? nullptr : format_name.c_str(), url.c_str());
    if (0 != err || !fmtCtx) {
        return AVFormatContext();
    }

    if (0 > ::avio_open(&fmtCtx->pb, url.c_str(), AVIO_FLAG_WRITE)) {
        ::avformat_free_context(fmtCtx);
        return AVFormatContext();
    }

    return AVFormatContext(fmtCtx, [](::AVFormatContext * fmtCtx) {
        ::avio_close(fmtCtx->pb);
        ::avformat_free_context(fmtCtx);
    });
}
///////////////////////////////////////////////////////////////////////////////
using AVStreamBase = std::unique_ptr<::AVStream, void (*)(::AVStream *)>;

class DLLOPT AVStream : public AVStreamBase {
public:
    AVStream()
        : AVStreamBase(nullptr, [](::AVStream *) {}) {
    }

    template<class T>
    AVStream(::AVStream * Ctx, T deleter)
        : AVStreamBase(Ctx, deleter) {
    }
};

inline int avformat_new_stream(AVFormatContext & fmtCtx, ::AVCodecParameters const * par) {
    auto out_stream = ::avformat_new_stream(fmtCtx.get(), nullptr);
    ::avcodec_parameters_copy(out_stream->codecpar, par);
    return out_stream->index;
}

inline int av_interleaved_write_frame(AVFormatContext & fmtCtx, int stream_index, ::AVPacket const & pkt) {
    int err = 0;
    // this is a flush packet, ignore and return.
    if (!pkt.data || !pkt.size) {
        return AVERROR_EOF;
    }
    ::AVPacket dup;
    ::av_packet_ref(&dup, &pkt);
    dup.stream_index = stream_index;
    auto & track = fmtCtx->streams[stream_index];
    ::av_packet_rescale_ts(&dup, FLICKS_TIMESCALE_Q, track->time_base);
    err = ::av_interleaved_write_frame(fmtCtx.get(), &dup);
    ::av_packet_unref(&dup);
    return err;
}
///////////////////////////////////////////////////////////////////////////////
using AVFrame = std::shared_ptr<::AVFrame>;

inline AVFrame av_frame_alloc() {
    return AVFrame(::av_frame_alloc(), [](::AVFrame * frame) {
        auto * pframe = &frame;
        av_frame_free(pframe);
    });
}

inline AVFrame av_frame_clone(::AVFrame const * frame) {
    auto newFrame = av_frame_alloc();
    if (::av_frame_ref(newFrame.get(), frame) < 0) {
        newFrame.reset();
    }
    return newFrame;
}

inline AVFrame av_frame_clone(AVFrame const & frame) {
    return libav::av_frame_clone(frame.get());
}

inline void av_frame_free(AVFrame frame) {
    auto pframe = frame.get();
    //::av_frame_free(&pframe);
}

inline int av_frame_copy(AVFrame dst, AVFrame src) {
    return ::av_frame_copy(dst.get(), src.get());
}

inline int av_frame_get_buffer(AVFrame const & frame) {
    return ::av_frame_get_buffer(frame.get(), 32);
}

inline AVFrame convert_frame(::AVFrame * frame, int width_out, int height_out, ::AVPixelFormat pix_out) {
    ::SwsContext * pContext = ::sws_getContext(
            frame->width,
            frame->height,
            (::AVPixelFormat) frame->format,
            width_out,
            height_out,
            pix_out,
            (SWS_FULL_CHR_H_INT | SWS_ACCURATE_RND | SWS_FAST_BILINEAR),
            nullptr,
            nullptr,
            nullptr);

    AVFrame frame2 = av_frame_alloc();
    frame2->format = pix_out;
    frame2->width = width_out;
    frame2->height = height_out;
    ::av_frame_get_buffer(frame2.get(), 32);

    sws_scale(
            pContext,
            frame->data,
            frame->linesize,
            0,
            frame->height,
            frame2->data,
            frame2->linesize);

    sws_freeContext(pContext);

    return frame2;
}


inline void convert_frame(AVFrame & frame_in, AVFrame & frame_out) {

    // I originally used flags = (SWS_FULL_CHR_H_INT | SWS_ACCURATE_RND | SWS_FAST_BILINEAR).
    // I think this came from an old version of ffmpeg and was related to needing to select some
    // particular kind of algorithm that had few artifacts when resizing didn't need to take place.
    // However, it is significantly slower to use SWS_ACCURATE_RND, so I have discontinued and I do not believe
    // There are drawbacks on the newer ffmpeg version.
    // https://libav-user.ffmpeg.narkive.com/Ig2s0MJN/sws-scale-has-weird-behavior-when-not-resizing
    ::SwsContext * pContext = ::sws_getContext(frame_in->width,
                                               frame_in->height,
                                               (::AVPixelFormat) frame_in->format,
                                               frame_out->width,
                                               frame_out->height,
                                               (::AVPixelFormat) frame_out->format,
                                               (SWS_FULL_CHR_H_INT | SWS_FAST_BILINEAR),
                                               nullptr,
                                               nullptr,
                                               nullptr);

    sws_scale(pContext, frame_in->data, frame_in->linesize, 0, frame_in->height, frame_out->data, frame_out->linesize);

    sws_freeContext(pContext);
}


///////////////////////////////////////////////////////////////////////////////
inline int av_open_best_stream(AVFormatContext & fmtCtx, AVMediaType type, int related_stream = -1) {
    int idx = -1;

    //https://www.mail-archive.com/debian-bugs-dist@lists.debian.org/msg1862296.html
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59, 0, 100)
    ::AVCodec * codec = nullptr;
    ::AVCodec ** pcodec = &codec;
#else
    ::AVCodec const * codec = nullptr;
    ::AVCodec const ** pcodec = &codec;
#endif

    if ((idx = ::av_find_best_stream(fmtCtx.get(), type, -1, related_stream, pcodec, 0)) < 0) {
        return -1;
    }
    auto codecCtx = AVCodecContext(::avcodec_alloc_context3(codec),
                                   [](::AVCodecContext * c) {
                                       ::avcodec_free_context(&c);
                                   });

    if (::avcodec_parameters_to_context(codecCtx.get(), fmtCtx->streams[idx]->codecpar) < 0) {
        return -1;
    }
    if (::avcodec_open2(codecCtx.get(), codec, nullptr) < 0) {
        return -1;
    }

    fmtCtx.open_streams.emplace(idx, std::move(codecCtx));
    return idx;
}

inline int av_open_best_streams(AVFormatContext & fmtCtx) {
    auto v = av_open_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO);
    auto a = av_open_best_stream(fmtCtx, AVMEDIA_TYPE_AUDIO, v);
    auto s = av_open_best_stream(fmtCtx, AVMEDIA_TYPE_SUBTITLE, 0 <= v ? v : a);
    (void) v, (void) a, (void) s;
    return fmtCtx.open_streams.size();
}

inline AVCodecContext & find_open_audio_stream(AVFormatContext & fmtCtx) {
    for (auto & stream: fmtCtx.open_streams) {
        if (stream.second->codec_type == AVMEDIA_TYPE_AUDIO) {
            return stream.second;
        }
    }

    static auto err = AVCodecContext(nullptr, [](::AVCodecContext *) {});
    return err;
}

inline AVCodecContext & find_open_video_stream(AVFormatContext & fmtCtx) {
    for (auto & stream: fmtCtx.open_streams) {
        if (stream.second->codec_type == AVMEDIA_TYPE_VIDEO) {
            return stream.second;
        }
    }

    static auto err = AVCodecContext(nullptr, [](::AVCodecContext *) {});
    return err;
}

/**
* Sends a packet to the decoder and processes all resulting frames.
*
* This function sends a packet to the decoder associated with the packet's stream index within the given format context.
* It then enters a loop to receive and process all frames resulting from the decoding of the packet. The processing of
* each frame is handled by the `onFrame` callback function provided by the caller. The loop continues until no more frames
* are available or an error occurs. If the error `AVERROR(EAGAIN)` is returned, it indicates that further input is required
* before more frames can be decoded, and the function returns 0 to signal this expected state. Any other error will halt
* the processing and return the error code.
*
* Ref:
* https://stackoverflow.com/questions/55354120/ffmpeg-avcodec-receive-frame-returns-averroreagain
*
*
* @param fmtCtx The format context containing the stream to which the packet belongs.
* @param pkt The packet to be sent to the decoder. Must not be `nullptr`.
* @param onFrame A callback function to be called for each decoded frame. The function signature should match
*                `void(AVFrame)`, where the `AVFrame` is the decoded frame.
* @return Returns 0 if the packet was processed successfully or if `AVERROR(EAGAIN)` was returned indicating that
*         further input is needed. Any other error code is returned if an error occurred during processing.
*/
inline int avcodec_send_packet(AVFormatContext & fmtCtx,
                               ::AVPacket * pkt,
                               std::function<void(AVFrame)> onFrame) {
    int err = AVERROR(1);
    auto codecCtx = fmtCtx.open_streams.find(pkt->stream_index);

    if (codecCtx != fmtCtx.open_streams.end()) {
        err = ::avcodec_send_packet(codecCtx->second.get(), pkt);
        while (err >= 0) {
            auto frame = av_frame_alloc();
            err = ::avcodec_receive_frame(codecCtx->second.get(), frame.get());
            if (err < 0) break;
            onFrame(frame);
        };
    }
    return err == AVERROR(EAGAIN) ? 0 : err;
}

/**
* Flushes the decoder by sending an empty packet.
*
* This function is designed to flush the internal buffers of the decoder. It allocates an empty packet
* (both data and size set to zero) and sends it to the decoder through the `avcodec_send_packet` function.
* This operation signals the decoder that there are no more packets to decode, prompting it to process
* and return any frames that are still buffered within. The function then frees the allocated packet to
* prevent memory leaks. This is particularly useful at the end of a decoding process to ensure all
* buffered frames are processed and returned.
*
* @param fmtCtx The format context associated with the decoder. It contains the decoder's state and stream information.
* @param onFrame A callback function that will be called for each frame that is flushed from the decoder. The callback
*                function must accept an `AVFrame` as its parameter.
* @return Returns the result of the `avcodec_send_packet` function. A return value of 0 indicates success, while
*         a negative value indicates an error.
*/
inline int flush_decoder(AVFormatContext & fmtCtx, std::function<void(AVFrame)> onFrame) {
    ::AVPacket * flushPkt = ::av_packet_alloc();
    flushPkt->data = nullptr;
    flushPkt->size = 0;
    int result = avcodec_send_packet(fmtCtx, flushPkt, onFrame);
    ::av_packet_free(&flushPkt);
    return result;
}

///////////////////////////////////////////////////////////////////////////////
/*
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
*/
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
// Modified from instructions at https://habr.com/en/company/intel/blog/575632/
// See also https://github.com/kmsquire/split_video/blob/master/split_video.c

inline AVCodecContext make_encode_context(AVFormatContext & media, std::string const codec_name, int width, int height, int fps, ::AVPixelFormat pix_fmt) {
    ::AVCodec const * codec = ::avcodec_find_encoder_by_name(codec_name.c_str());
    if (!codec) {
        std::cout << "Could not find codec by name" << std::endl;
    }
    auto codecCtx = AVCodecContext(::avcodec_alloc_context3(codec),
                                   [](::AVCodecContext * c) {
                                       ::avcodec_free_context(&c);
                                   });

    //https://stackoverflow.com/questions/46444474/c-ffmpeg-create-mp4-file
    ::AVStream * videoStream = ::avformat_new_stream(media.get(), codec);

    videoStream->codecpar->codec_id = ::AV_CODEC_ID_H264;
    videoStream->codecpar->codec_type = ::AVMEDIA_TYPE_VIDEO;
    videoStream->codecpar->width = width;
    videoStream->codecpar->height = height;
    videoStream->codecpar->format = pix_fmt;
    //videoStream->codecpar->bit_rate = 2000 * 1000; // setting this really ****s it up

    ::avcodec_parameters_to_context(codecCtx.get(), videoStream->codecpar);

    codecCtx->time_base = ::AVRational({1, fps});
    codecCtx->framerate = ::AVRational({fps, 1});

    //::av_opt_set(codecCtx.get(),"preset","fast",0);
    //https://trac.ffmpeg.org/ticket/1548
    ::av_opt_set(codecCtx.get()->priv_data, "profile", "high", AV_OPT_SEARCH_CHILDREN);
    ::av_opt_set(codecCtx.get(), "bf", "0", 0);
    ::av_opt_set(codecCtx.get(), "b", "500k", 0);

    //std::cout << "Max b frames: " << codecCtx->max_b_frames << std::endl;

    if (codecCtx->max_b_frames > 0) {
        codecCtx->max_b_frames = 0;
    }

    ::avcodec_parameters_from_context(videoStream->codecpar, codecCtx.get());

    return codecCtx;
}

inline AVCodecContext DLLOPT make_encode_context_nvenc(AVFormatContext & media, int width, int height, int fps) {
    return make_encode_context(media, "h264_nvenc", width, height, fps, ::AV_PIX_FMT_CUDA);
}
/*
inline AVCodecContext make_encode_context_h264(AVFormatContext& media,int width, int height, int fps) 
{
//This should call with a different context name and pixel format. Otherwise Okay I think.
return make_encode_context(media,"h264_nvenc",width,height,fps,::AV_PIX_FMT_CUDA);
}
*/
inline void bind_hardware_frames_context(AVCodecContext & ctx, int width, int height, ::AVPixelFormat hw_pix_fmt, ::AVPixelFormat sw_pix_fmt) {
    ::AVBufferRef * hw_device_ctx = nullptr;
    auto err = ::av_hwdevice_ctx_create(&hw_device_ctx, ::AV_HWDEVICE_TYPE_CUDA, NULL, NULL, 0);// Deallocator?

    if (err < 0) {
        std::cout << "Failed to create CUDA device with error code " << std::endl;
    }

    auto hw_frames_ref = ::av_hwframe_ctx_alloc(hw_device_ctx);// Deallocator?

    ::AVHWFramesContext * frames_ctx;
    frames_ctx = (::AVHWFramesContext *) (hw_frames_ref->data);
    frames_ctx->format = hw_pix_fmt;
    frames_ctx->sw_format = sw_pix_fmt;
    frames_ctx->width = width;
    frames_ctx->height = height;

    ::av_hwframe_ctx_init(hw_frames_ref);

    ctx->hw_frames_ctx = ::av_buffer_ref(hw_frames_ref);

    ::av_buffer_unref(&hw_frames_ref);
}

inline void DLLOPT open_encode_stream_to_write(AVFormatContext & media, std::string file_name) {
    ::avio_open(&media->pb, file_name.c_str(), AVIO_FLAG_WRITE);

    auto success = ::avformat_write_header(media.get(), NULL);
}

inline void DLLOPT bind_hardware_frames_context_nvenc(AVCodecContext & ctx, int width, int height, ::AVPixelFormat sw_pix_fmt) {
    bind_hardware_frames_context(ctx, width, height, AV_PIX_FMT_CUDA, sw_pix_fmt);
}

/*
When we no longer will be acquiring frames, we will be flushing the internal buffers of the encoder
To start this process, we must send a NULL frame. Then, every receieve packet after will be from internal buffers
*/
inline void DLLOPT encode_enter_drain_mode(AVFormatContext & media, AVCodecContext & ctx) {
    ::avcodec_send_frame(ctx.get(), NULL);
}

inline int DLLOPT hardware_encode_flush(AVFormatContext & media, AVCodecContext & ctx, int frame_count) {

    std::string const codec_name = "h264_nvenc";
    ::AVCodec const * codec = ::avcodec_find_encoder_by_name(codec_name.c_str());
    ::avcodec_open2(ctx.get(), codec, NULL);// Why does this have to happen with every write?

    auto pkt = av_packet_alloc();

    auto err = ::avcodec_receive_packet(ctx.get(), pkt.get());

    if (err == AVERROR_EOF) {
        //std::cout << "End flush complete at frame " << frame_count << std::endl;
        return 0;
    }

    auto & track = media->streams[0];

    //https://stackoverflow.com/questions/54491521/can-i-use-av-write-frame-instead-of-av-interleaved-write-frame
    //Interleaved Write frame is preferred becuase the decoder takes care of any reordering
    //av_interleaved_write_frame(media, 0, *pkt);
    //::av_packet_rescale_ts(pkt.get(), FLICKS_TIMESCALE_Q, track->time_base);

    // I'm getting this duration from some kind of lookup table for that particular fps. I should be able to set it in ffmpeg somehow.
    int64_t const duration = 2000;
    pkt->pts = static_cast<int64_t>(frame_count) * duration;
    pkt->dts = pkt->pts;
    pkt->duration = duration;

    err = ::av_write_frame(media.get(), pkt.get());

    if (err < 0) {
        std::cout << "Error writing frame " << frame_count << std::endl;
    }

    ::av_packet_unref(pkt.get());

    return (err == AVERROR_EOF) ? 0 : err;
}

inline int DLLOPT hardware_encode(AVFormatContext & media, AVCodecContext & ctx, AVFrame & sw_frame, int frame_count) {

    auto * hw_frame = ::av_frame_alloc();
    auto err = ::av_hwframe_get_buffer(ctx->hw_frames_ctx, hw_frame, 0);
    if (err) {
        std::cout << "Could not get hardware frame buffer";
    }

    std::string const codec_name = "h264_nvenc";
    ::AVCodec const * codec = ::avcodec_find_encoder_by_name(codec_name.c_str());
    ::avcodec_open2(ctx.get(), codec, NULL);// Why does this have to happen with every write?

    err = ::av_hwframe_transfer_data(hw_frame, sw_frame.get(), 0);
    if (err) {
        std::cout << "Error transferring data frame to surface";
    }

    auto pkt = av_packet_alloc();

    ::avcodec_send_frame(ctx.get(), hw_frame);

    ::avcodec_receive_packet(ctx.get(), pkt.get());

    auto & track = media->streams[0];

    //https://stackoverflow.com/questions/54491521/can-i-use-av-write-frame-instead-of-av-interleaved-write-frame
    //Interleaved Write frame is preferred becuase the decoder takes care of any reordering
    //av_interleaved_write_frame(media, 0, *pkt);
    //::av_packet_rescale_ts(pkt.get(), FLICKS_TIMESCALE_Q, track->time_base);

    // I'm getting this duration from some kind of lookup table for that particular fps. I should be able to set it in ffmpeg somehow.
    int64_t const duration = 2000;
    pkt->pts = static_cast<int64_t>(frame_count) * duration;
    pkt->dts = pkt->pts;
    pkt->duration = duration;

    err = ::av_write_frame(media.get(), pkt.get());

    if (err < 0) {
        std::cout << "Error writing frame " << frame_count << std::endl;
    }

    ::av_packet_unref(pkt.get());
    ::av_frame_free(&hw_frame);

    return err;
}

}// namespace libav

#endif// LIBAVINC_HPP
