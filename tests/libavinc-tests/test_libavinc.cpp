
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include "libavinc/libavinc.hpp"

#include <algorithm>
#include <cstdint>
#include <string>

static std::string video_filename = "data/test_each_frame_number.mp4";

TEST_CASE("Open encode context is idempotent", "[libavinc]") {
    ::AVCodec const * codec = ::avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if (!codec) {
        SKIP("MPEG4 encoder is not available in this FFmpeg build");
    }

    auto codec_context = libav::AVCodecContext(::avcodec_alloc_context3(codec),
                                               [](::AVCodecContext * context) {
                                                   ::avcodec_free_context(&context);
                                               });

    codec_context->width = 640;
    codec_context->height = 480;
    codec_context->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_context->time_base = AVRational{1, 30};
    codec_context->framerate = AVRational{30, 1};

    CHECK(libav::open_encode_context(codec_context) == 0);
    CHECK(::avcodec_is_open(codec_context.get()) != 0);

    CHECK(libav::open_encode_context(codec_context) == 0);
    CHECK(::avcodec_is_open(codec_context.get()) != 0);
}

TEST_CASE("Cached scale context matches one-shot conversion", "[libavinc]") {
    constexpr int width = 64;
    constexpr int height = 48;

    auto input = libav::av_frame_alloc();
    input->format = AV_PIX_FMT_GRAY8;
    input->width = width;
    input->height = height;

    auto one_shot = libav::av_frame_alloc();
    one_shot->format = AV_PIX_FMT_NV12;
    one_shot->width = width;
    one_shot->height = height;

    auto cached = libav::av_frame_alloc();
    cached->format = AV_PIX_FMT_NV12;
    cached->width = width;
    cached->height = height;

    REQUIRE(::av_frame_get_buffer(input.get(), 32) == 0);
    REQUIRE(::av_frame_get_buffer(one_shot.get(), 32) == 0);
    REQUIRE(::av_frame_get_buffer(cached.get(), 32) == 0);

    for (int row = 0; row < height; ++row) {
        auto * row_data = input->data[0] + row * input->linesize[0];
        for (int column = 0; column < width; ++column) {
            row_data[column] = static_cast<uint8_t>((row * width + column) % 256);
        }
    }

    libav::convert_frame(input, one_shot);

    ::SwsContext * scale_context = libav::make_scale_context(input, cached);
    REQUIRE(scale_context != nullptr);

    libav::convert_frame(input, cached, scale_context);
    ::sws_freeContext(scale_context);

    for (int row = 0; row < height; ++row) {
        auto const * expected = one_shot->data[0] + row * one_shot->linesize[0];
        auto const * actual = cached->data[0] + row * cached->linesize[0];
        CHECK(std::equal(expected, expected + width, actual));
    }

    for (int row = 0; row < height / 2; ++row) {
        auto const * expected = one_shot->data[1] + row * one_shot->linesize[1];
        auto const * actual = cached->data[1] + row * cached->linesize[1];
        CHECK(std::equal(expected, expected + width, actual));
    }
}

TEST_CASE("Check for Keyframes", "[libavinc]") {

    auto mymedia = libav::avformat_open_input(video_filename);

    libav::av_open_best_streams(mymedia);

    int keyframe_count = 0;
    int total_frame_count = 0;
    for (auto & pkg : mymedia) {
        if (pkg.flags & AV_PKT_FLAG_KEY) {
            keyframe_count++;
        }
        total_frame_count++;
        ::av_packet_unref(&pkg);
    }
    
    CHECK(keyframe_count == 5);
    CHECK(total_frame_count == 1000);

}

TEST_CASE("Decode 5 Frames", "[libavinc]") {

    // No leak
    auto mymedia = libav::avformat_open_input(video_filename);
    libav::av_open_best_streams(mymedia);
    int frame_count = 5;
    auto pkt = ::av_packet_alloc();
    auto err = ::av_read_frame(mymedia.get(), pkt);
    ::av_packet_free(&pkt);


    
    CHECK(frame_count == 5);

}

template <auto fn>
struct deleter_from_fn {
    template <typename T>
    constexpr void operator()(T* arg) const {
        fn(&arg);
    }
};

template <typename T, auto fn>
using my_unique_ptr = std::unique_ptr<T, deleter_from_fn<fn>>;

TEST_CASE("Decode 5 Frames with libavinc", "[libavinc]") {

    auto mymedia = libav::avformat_open_input(video_filename);
    libav::av_open_best_streams(mymedia);
    int frame_count = 0;

    //my_unique_ptr<::AVPacket, ::av_packet_free> pkt(::av_packet_alloc());
    //std::unique_ptr<::AVPacket, libav::PktDeleter> pkt(::av_packet_alloc());
    auto pkt = libav::AVPacketBase(::av_packet_alloc());
    //auto pkt = mymedia.begin();
    auto err = ::av_read_frame(mymedia.get(), pkt.get());
    //auto pkt_ptr = pkt.get();
    //::av_packet_free(&pkt_ptr);

    
    CHECK(frame_count == 0);

}

TEST_CASE("Decode whole video libavinc", "[libavinc]") {

    auto mymedia = libav::avformat_open_input(video_filename);
    libav::av_open_best_streams(mymedia);
    int frame_count = 0;

    //auto pkt = libav::AVPacketBase(::av_packet_alloc());
    //auto pkt = libav::AVPacket(mymedia.get());
    auto pkt = mymedia.begin(); // This automatically reads the first frame, so need to unref
    ::av_packet_unref(pkt.get());
    while (true) {
        auto err = ::av_read_frame(mymedia.get(), pkt.get());
        ::av_packet_unref(pkt.get());
        frame_count++;
        if (err < 0) {
            break;
        }
    }


    CHECK(frame_count == 1000);

}