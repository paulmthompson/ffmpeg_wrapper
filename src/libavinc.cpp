#ifndef LIBAVINC_CPP
#define LIBAVINC_CPP

#include "libavinc.hpp"

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
#include <libavutil/opt.h>
}

#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <stdio.h>

namespace libav {

///////////////////////////////////////////////////////////////////////////////
// AV Dictionary

::AVDictionary* av_dictionary(const libav::AVDictionary& dict)
{
    ::AVDictionary* d = nullptr;
    for (const auto& entry : dict) {
        av_dict_set(&d, entry.first.c_str(), entry.second.c_str(), 0);
    }
    return d;
}

libav::AVDictionary av_dictionary(const ::AVDictionary* dict)
{
    libav::AVDictionary d;
    AVDictionaryEntry* entry = nullptr;
    while ((entry = av_dict_get(dict, "", entry, AV_DICT_IGNORE_SUFFIX))) {
        d.emplace(entry->key, entry->value);
    }
    return d;
}

void av_dict_free(::AVDictionary* d)
{
    if (d) {
        av_dict_free(&d);
    }
}

///////////////////////////////////////////////////////////////////////////////
// AVIO Context
AVIOContext avio_alloc_context(std::function<int(uint8_t*, int)> read,
    std::function<int(uint8_t*, int)> write,
    std::function<int64_t(int64_t, int)> seek,
    int buffer_size, int write_flag)
{
    struct opaque_t {
        std::function<int(uint8_t*, int)> read;
        std::function<int(uint8_t*, int)> write;
        std::function<int64_t(int64_t, int)> seek;
    };

    // create a copy of the functions to enable captures
    auto buffer = (unsigned char*)::av_malloc(buffer_size);
    auto opaque = new opaque_t { read, write, seek };
    return AVIOContext(
        ::avio_alloc_context(
            buffer, buffer_size, write_flag, opaque,
            [](void* opaque, uint8_t* buf, int buf_size) {
                auto o = reinterpret_cast<opaque_t*>(opaque);
                return o->read(buf, buf_size);
            },
            [](void* opaque, uint8_t* buf, int buf_size) {
                auto o = reinterpret_cast<opaque_t*>(opaque);
                return o->write(buf, buf_size);
            },
            [](void* opaque, int64_t offset, int whence) {
                auto o = reinterpret_cast<opaque_t*>(opaque);
                return o->seek(offset, whence);
            }),
        [](::AVIOContext* c) {
            delete reinterpret_cast<opaque_t*>(c->opaque);
            av_free(c->buffer), c->buffer = nullptr;
            ::avio_context_free(&c);
        });
}

AVIOContext avio_alloc_context(std::function<int(uint8_t*, int)> read,
    std::function<int64_t(int64_t, int)> seek, int buffer_size)
{
    return libav::avio_alloc_context(
        read, [](uint8_t*, int) { return 0; }, seek, 0, buffer_size);
}

///////////////////////////////////////////////////////////////////////////////


int av_read_frame(::AVFormatContext* ctx, ::AVPacket* pkt)
{
    if (!ctx || !pkt) {
        return AVERROR(1);
    }

    auto err = ::av_read_frame(ctx, pkt);
    if (0 <= err) {
        auto& track = ctx->streams[pkt->stream_index];
        ::av_packet_rescale_ts(pkt, track->time_base, FLICKS_TIMESCALE_Q);
        // TODO check for pkt->size == 0 but not EOF
    } else {
        pkt = ::av_packet_alloc();
        pkt->size = 0;
        pkt->data = nullptr;
    }

    return err;
}

AVPacket av_packet_alloc()
{
    return AVPacket(nullptr);
}

AVPacket av_packet_clone(const ::AVPacket* src)
{
    auto newPkt = AVPacket(nullptr);
    if (::av_packet_ref(newPkt.get(), src) < 0) {
        newPkt.reset();
    }
    return newPkt;
}

AVPacket av_packet_clone(const AVPacket& src)
{
    return libav::av_packet_clone(src.get());
}


///////////////////////////////////////////////////////////////////////////////

AVFormatContext avformat_open_input(const std::string& url, const libav::AVDictionary& options)
{
    ::AVFormatContext* fmtCtx = nullptr;
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

    return libav::AVFormatContext(fmtCtx, [](::AVFormatContext* ctx) {
        auto p_ctx = &ctx;
        ::avformat_close_input(p_ctx);
    });
}

int av_seek_frame(AVFormatContext& ctx, flicks time, int idx, int flags)
{
    ::AVRational time_base;
    if (0 <= idx) {
        time_base = ctx->streams[idx]->time_base;
    } else {
        time_base =  ::av_get_time_base_q();
    }
    return ::av_seek_frame(ctx.get(), idx, av_rescale(time, time_base), flags);
}

AVFormatContext avformat_open_output(const std::string& url, std::string format_name)
{
    ::AVFormatContext* fmtCtx = nullptr;
    auto err = ::avformat_alloc_output_context2(&fmtCtx, nullptr, format_name.empty() ? nullptr : format_name.c_str(), url.c_str());
    if (0 != err || !fmtCtx) {
        return AVFormatContext();
    }

    if (0 > ::avio_open(&fmtCtx->pb, url.c_str(), AVIO_FLAG_WRITE)) {
        ::avformat_free_context(fmtCtx);
        return AVFormatContext();
    }

    return AVFormatContext(fmtCtx, [](::AVFormatContext* fmtCtx) {
        ::avio_close(fmtCtx->pb);
        ::avformat_free_context(fmtCtx);
    });
}

///////////////////////////////////////////////////////////////////////////////
int avformat_new_stream(AVFormatContext& fmtCtx, const ::AVCodecParameters* par)
{
    auto out_stream = ::avformat_new_stream(fmtCtx.get(), nullptr);
    ::avcodec_parameters_copy(out_stream->codecpar, par);
    return out_stream->index;
}

int av_interleaved_write_frame(AVFormatContext& fmtCtx, int stream_index, const ::AVPacket& pkt)
{
    int err = 0;
    // this is a flush packet, ignore and return.
    if (!pkt.data || !pkt.size) {
        return AVERROR_EOF;
    }
    ::AVPacket dup;
    ::av_packet_ref(&dup, &pkt);
    dup.stream_index = stream_index;
    auto& track = fmtCtx->streams[stream_index];
    ::av_packet_rescale_ts(&dup, FLICKS_TIMESCALE_Q, track->time_base);
    err = ::av_interleaved_write_frame(fmtCtx.get(), &dup);
    ::av_packet_unref(&dup);
    return err;
}
///////////////////////////////////////////////////////////////////////////////

AVFrame av_frame_alloc()
{
    return AVFrame(::av_frame_alloc(), [](::AVFrame* frame) {
        auto* pframe = &frame;
        av_frame_free(pframe);
    });
}

AVFrame av_frame_clone(const ::AVFrame* frame)
{
    auto newFrame = av_frame_alloc();
    if (::av_frame_ref(newFrame.get(), frame) < 0) {
        newFrame.reset();
    }
    return newFrame;
}

AVFrame av_frame_clone(const AVFrame& frame)
{
    return libav::av_frame_clone(frame.get());
}

AVFrame convert_frame(AVFrame& frame, int width_out, int height_out, ::AVPixelFormat pix_out)
{
    ::SwsContext * pContext = ::sws_getContext(frame->width, frame->height, (::AVPixelFormat)frame->format,
                                          width_out, height_out, pix_out, (SWS_FULL_CHR_H_INT | SWS_ACCURATE_RND | SWS_FAST_BILINEAR), nullptr, nullptr, nullptr);

    AVFrame frame2 = av_frame_alloc();
    frame2->format = pix_out;
    frame2->width = width_out;
    frame2->height = height_out;
    ::av_frame_get_buffer(frame2.get(),32);

    sws_scale(pContext, frame->data, frame->linesize, 0, frame->height, frame2->data, frame2->linesize);

    sws_freeContext(pContext);

    return frame2;
}

///////////////////////////////////////////////////////////////////////////////

int av_open_best_stream(AVFormatContext& fmtCtx, AVMediaType type, int related_stream)
{
    int idx = -1;
    const ::AVCodec* codec = nullptr;
    if ((idx = ::av_find_best_stream(fmtCtx.get(), type, -1, related_stream, &codec, 0)) < 0) {
        return -1;
    }
    auto codecCtx = AVCodecContext(::avcodec_alloc_context3(codec),
        [](::AVCodecContext* c) {
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

int av_open_best_streams(AVFormatContext& fmtCtx)
{
    auto v = av_open_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO);
    auto a = av_open_best_stream(fmtCtx, AVMEDIA_TYPE_AUDIO, v);
    auto s = av_open_best_stream(fmtCtx, AVMEDIA_TYPE_SUBTITLE, 0 <= v ? v : a);
    (void)v, (void)a, (void)s;
    return fmtCtx.open_streams.size();
}

AVCodecContext& find_open_audio_stream(AVFormatContext& fmtCtx)
{
    for (auto& stream : fmtCtx.open_streams) {
        if (stream.second->codec_type == AVMEDIA_TYPE_AUDIO) {
            return stream.second;
        }
    }

    static auto err = AVCodecContext(nullptr, [](::AVCodecContext*) {});
    return err;
}

AVCodecContext& find_open_video_stream(AVFormatContext& fmtCtx)
{
    for (auto& stream : fmtCtx.open_streams) {
        if (stream.second->codec_type == AVMEDIA_TYPE_VIDEO) {
            return stream.second;
        }
    }

    static auto err = AVCodecContext(nullptr, [](::AVCodecContext*) {});
    return err;
}

int avcodec_send_packet(AVFormatContext& fmtCtx, ::AVPacket* pkt,
    std::function<void(AVFrame)> onFrame)
{
    int err = AVERROR(1);
    auto codecCtx = fmtCtx.open_streams.find(pkt->stream_index);
    if (codecCtx != fmtCtx.open_streams.end()) {
        ::avcodec_send_packet(codecCtx->second.get(), pkt);
        for (;;) {
            auto frame = av_frame_alloc();
            err = ::avcodec_receive_frame(codecCtx->second.get(), frame.get());
            if (err < 0) {
                break;
            }

            onFrame(frame);
        };
    }
    return err == AVERROR(EAGAIN) ? 0 : err;
    //return err;
}

int avcodec_send_packet(AVFormatContext& fmtCtx,
    std::function<void(AVFrame)> onFrame)
{
 //   ::AVPacket pkt;
    ::AVPacket* pkt = ::av_packet_alloc();
//    ::av_init_packet(&pkt);
    pkt->data = nullptr, pkt->size = 0;
    return avcodec_send_packet(fmtCtx, pkt, onFrame);
}
///////////////////////////////////////////////////////////////////////////////
// Filter Graph


///////////////////////////////////////////////////////////////////////////////
// Modified from instructions at https://habr.com/en/company/intel/blog/575632/
AVCodecContext make_encode_context(AVFormatContext& media,std::string codec_name,int width, int height, int fps, ::AVPixelFormat pix_fmt)
{
    const ::AVCodec* codec = ::avcodec_find_encoder_by_name(codec_name.c_str());
    if (! codec) {
        std::cout << "Could not find codec by name";
    }
    auto codecCtx = AVCodecContext(::avcodec_alloc_context3(codec),
        [](::AVCodecContext* c) {
            ::avcodec_free_context(&c);
        });

    //https://stackoverflow.com/questions/46444474/c-ffmpeg-create-mp4-file
    ::AVStream* videoStream = ::avformat_new_stream(media.get(),codec);

    videoStream->codecpar->codec_id = ::AV_CODEC_ID_H264;
    videoStream->codecpar->codec_type = ::AVMEDIA_TYPE_VIDEO;
    videoStream->codecpar->width = width;
    videoStream->codecpar->height = height;
    videoStream->codecpar->format = pix_fmt;
    videoStream->codecpar->bit_rate = 2000 * 1000;
    
    ::avcodec_parameters_to_context(codecCtx.get(),videoStream->codecpar);

    codecCtx->time_base = ::AVRational({1,fps});
    codecCtx->framerate = ::AVRational({fps,1});

    ::av_opt_set(codecCtx.get(),"preset","ultrafast",0);

    ::avcodec_parameters_from_context(videoStream->codecpar, codecCtx.get());

    std::string test_file = "test2.mp4";
    ::avio_open(&media->pb, test_file.c_str(), AVIO_FLAG_WRITE);

    ::avformat_write_header(media.get(), NULL);
    /*
    codecCtx->width = width;
    codecCtx->height = height;
    codecCtx->time_base = ::AVRational({1,fps});
    codecCtx->framerate = ::AVRational({fps,1});
    codecCtx->sample_aspect_ratio = ::AVRational({1,1});
    codecCtx->pix_fmt = pix_fmt;
    */

    return codecCtx;
}

AVCodecContext make_encode_context_nvenc(AVFormatContext& media,int width, int height, int fps) {
    return make_encode_context(media,"h264_nvenc",width,height,fps,::AV_PIX_FMT_CUDA);
}

void bind_hardware_frames_context(AVCodecContext& ctx, int width, int height, ::AVPixelFormat hw_pix_fmt,::AVPixelFormat sw_pix_fmt)
{
    ::AVBufferRef *hw_device_ctx = nullptr;
    auto err = ::av_hwdevice_ctx_create(&hw_device_ctx,::AV_HWDEVICE_TYPE_CUDA,NULL,NULL,0); // Deallocator?

    if (err < 0) {
        std::cout << "Failed to create CUDA device with error code ";
    }

    auto hw_frames_ref = ::av_hwframe_ctx_alloc(hw_device_ctx); // Deallocator?

    ::AVHWFramesContext *frames_ctx;
    frames_ctx = (::AVHWFramesContext *)(hw_frames_ref->data);
    frames_ctx->format = hw_pix_fmt;
    frames_ctx->sw_format = sw_pix_fmt;
    frames_ctx->width = width;
    frames_ctx->height = height;

    ::av_hwframe_ctx_init(hw_frames_ref);

    ctx->hw_frames_ctx = ::av_buffer_ref(hw_frames_ref);

    ::av_buffer_unref(&hw_frames_ref);

}

void bind_hardware_frames_context_nvenc(AVCodecContext& ctx, int width, int height, ::AVPixelFormat sw_pix_fmt) {
     bind_hardware_frames_context(ctx, width, height, AV_PIX_FMT_CUDA,sw_pix_fmt);
}

void hardware_encode(AVFormatContext& media,AVCodecContext& ctx, AVFrame& sw_frame, int frame_count)
{

    auto hw_frame = libav::av_frame_alloc();

    const ::AVCodec* codec = ::avcodec_find_encoder_by_name("h264_nvenc");
    auto err = ::av_hwframe_get_buffer(ctx->hw_frames_ctx,hw_frame.get(),0);
    if (err) {
        std::cout << "Could not get hardware frame buffer";
    }

    ::avcodec_open2(ctx.get(),codec,NULL);

    err = ::av_hwframe_transfer_data(hw_frame.get(),sw_frame.get(),0);
    if (err) {
        std::cout << "Error transferring data frame to surface";
    }

    auto pkt = av_packet_alloc();

    
    //hw_frame->pts = 2000 * frame_count;

    ::avcodec_send_frame(ctx.get(), hw_frame.get());

    ::avcodec_receive_packet(ctx.get(),pkt.get());

    pkt->pts = 2000 * frame_count;
    pkt->dts = pkt->pts;
    pkt->duration = 2000;
    //fwrite(pkt->data,pkt->size,1,pFile);
    ::av_write_frame(media.get(), pkt.get());

    ::av_packet_unref(pkt.get());
}

///////////////////////////////////////////////////////////////////////////////

} // End libav namespace
#endif // LIBAVINC_CPP
