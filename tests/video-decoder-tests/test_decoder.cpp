
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include "ffmpeg_wrapper/VideoDecoder.h"

#include <string>

TEST_CASE("VideoDecoder object creation", "[ffmpeg_wrapper]") {
    SECTION("Create a VideoDecoder object") {

        std::string video_filename = "data/test_each_frame_number.mp4";

        ffmpeg_wrapper::VideoDecoder decoder;
        decoder.createMedia(video_filename);

        CHECK(decoder.getHeight() == 480);
        CHECK(decoder.getWidth() == 640);

    }
}