
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include "libavinc/libavinc.hpp"

static std::string video_filename = "data/test_each_frame_number.mp4";

TEST_CASE("Check for Keyframes", "[libavinc]") {

    auto mymedia = libav::avformat_open_input(video_filename);

    libav::av_open_best_streams(mymedia);

    int keyframe_count = 0;
    for (auto & pkg: mymedia) {
        if (pkg.flags & AV_PKT_FLAG_KEY) {
            keyframe_count++;
        }
    }
    
    CHECK(keyframe_count == 5);

}

TEST_CASE("Decode 5 Frames", "[libavinc]") {

    auto mymedia = libav::avformat_open_input(video_filename);

    libav::av_open_best_streams(mymedia);

    int frame_count = 0;
    //auto pkt = mymedia.begin();
    auto pkt = ::av_packet_alloc();
    //libav::av_read_frame(mymedia.get(), pkt.get());
    auto err = ::av_read_frame(mymedia.get(), pkt);
    ::av_packet_free(&pkt);


    
    CHECK(frame_count == 5);

}