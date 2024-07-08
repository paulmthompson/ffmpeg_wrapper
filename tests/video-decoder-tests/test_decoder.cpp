
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include "ffmpeg_wrapper/VideoDecoder.h"

TEST_CASE("VideoDecoder object creation", "[ffmpeg_wrapper]") {
SECTION("Create a VideoDecoder object") {

ffmpeg_wrapper::VideoDecoder decoder;

REQUIRE(true); // Placeholder check, replace with actual verification if needed
}
}