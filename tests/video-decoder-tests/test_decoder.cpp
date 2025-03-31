
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include "ffmpeg_wrapper/videodecoder.h"

#include <algorithm>
#include <fstream>
#include <string>

inline auto load_img = [](std::string filename){
    std::ifstream stream(filename, std::ios::binary);

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

    return data;
};

// Function to save a decoded image as a binary file
inline void save_decoded_image_as_binary(const std::vector<uint8_t>& decoded_image, const std::string& output_path) {
    std::ofstream file(output_path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open " << output_path << " for writing.\n";
        return;
    }
    file.write(reinterpret_cast<const char*>(decoded_image.data()), decoded_image.size());
}

size_t calculate_pixel_difference(const std::vector<uint8_t>& original, const std::vector<uint8_t>& decoded, int tolerance) {
    size_t diff_count = 0;
    for(size_t i = 0; i < original.size(); ++i) {
        if(std::abs(original[i] - decoded[i]) > tolerance) {
            std::cout 
            << "Difference at index " 
            << i 
            << ": " 
            << static_cast<int>(original[i]) 
            << " vs " 
            << static_cast<int>(decoded[i]) 
            << std::endl;
            ++diff_count;
        }
    }
    std::cout << "Number of different elements: " << diff_count << std::endl;

    if (diff_count > 0) {
        save_decoded_image_as_binary(decoded, "data/decoded_image.bin");
    }
    return diff_count;
}

static const int tolerance = 7; // Linux machine needs a tolerance of 7
static auto frame_0 = load_img("data/frame_0.bin");
static auto frame_100 = load_img("data/frame_100.bin");
static auto frame_200 = load_img("data/frame_200.bin");
static auto frame_300 = load_img("data/frame_300.bin");
static auto frame_400 = load_img("data/frame_400.bin");
static auto frame_500 = load_img("data/frame_500.bin"); //keyframe

static std::string video_filename = "data/test_each_frame_number.mp4";

TEST_CASE("VideoDecoder object creation", "[ffmpeg_wrapper]") {


    ffmpeg_wrapper::VideoDecoder decoder;
    decoder.createMedia(video_filename);

    CHECK(decoder.getHeight() == 480);
    CHECK(decoder.getWidth() == 640);
}

TEST_CASE("VideoDecoder get keyframes", "[ffmpeg_wrapper]") {
    ffmpeg_wrapper::VideoDecoder decoder;
    decoder.createMedia(video_filename);

    //Print keyframes
    auto keyframes = decoder.getKeyFrames();
    std::cout << "Keyframes: " << std::endl;
    for (auto keyframe: keyframes) {
        std::cout << keyframe << std::endl;
    }

    CHECK(keyframes.size() == 5);
}

TEST_CASE("VideoDecoder get frame", "[ffmpeg_wrapper]") {
    ffmpeg_wrapper::VideoDecoder decoder;
    decoder.createMedia(video_filename);
    auto frame_0_decoded = decoder.getFrame(0);

    CHECK(frame_0.size() == frame_0_decoded.size());

    size_t diff_count = calculate_pixel_difference(frame_0, frame_0_decoded, tolerance);
    CHECK(diff_count == 0);



}

TEST_CASE("VideoDecoder get frame by frame", "[ffmpeg_wrapper]") {

    ffmpeg_wrapper::VideoDecoder decoder;
    decoder.createMedia(video_filename);

    for (int i = 0; i < 200; ++i) {
        auto frame_decoded = decoder.getFrame(i);
    }

    auto frame_200_decoded = decoder.getFrame(200);

    size_t diff_count = calculate_pixel_difference(frame_200, frame_200_decoded, tolerance);
    CHECK(diff_count == 0);

}

TEST_CASE("VideoDecoder move backward", "[ffmpeg_wrapper]") {

    ffmpeg_wrapper::VideoDecoder decoder;
    decoder.createMedia(video_filename);

    decoder.getFrame(0); // Needs to decode first frame to not segfault?

    auto frame_200_decoded = decoder.getFrame(200);

    size_t diff_count = calculate_pixel_difference(frame_200, frame_200_decoded, tolerance);
    CHECK(diff_count == 0);

    // move back to frame 0 and check if the decoded frame is the same as frame 0
    auto frame_0_decoded = decoder.getFrame(0);
    diff_count = calculate_pixel_difference(frame_0, frame_0_decoded, tolerance);
    CHECK(diff_count == 0);

}

TEST_CASE("VideoDecoder move backward frame by frame", "[ffmpeg_wrapper]") {

    ffmpeg_wrapper::VideoDecoder decoder;
    decoder.createMedia(video_filename);
    decoder.getFrame(0);

    auto frame_200_decoded = decoder.getFrame(200);

    size_t diff_count = calculate_pixel_difference(frame_200, frame_200_decoded, tolerance);
    CHECK(diff_count == 0);

    // move back to frame 0 and check if the decoded frame is the same as frame 0
    for (int i = 200; i >= 0; --i) {
        auto frame_decoded = decoder.getFrame(i);
    }

    auto frame_0_decoded = decoder.getFrame(0);
    diff_count = calculate_pixel_difference(frame_0, frame_0_decoded, tolerance);
    CHECK(diff_count == 0);

}

TEST_CASE("Determine nearest keyframe","[ffmpeg_wrapper]")
{
    ffmpeg_wrapper::VideoDecoder decoder;
    decoder.createMedia(video_filename);
    decoder.getFrame(0);

    CHECK(decoder.nearest_iframe(0) == 0);
    CHECK(decoder.nearest_iframe(1) == 0);
    CHECK(decoder.nearest_iframe(300) == 250);
    CHECK(decoder.nearest_iframe(500) == 500);
}

TEST_CASE("Seek to keyframes","[ffmpeg_wrapper]")
{
    ffmpeg_wrapper::VideoDecoder decoder;
    decoder.createMedia(video_filename);
    decoder.getFrame(0);

    decoder.getFrame(250);

    auto frame_decoded_500 = decoder.getFrame(500);
    size_t diff_count = calculate_pixel_difference(frame_500, frame_decoded_500, tolerance);
    CHECK(diff_count == 0);
}

TEST_CASE("Seek to keyframe and move backward","[ffmpeg_wrapper]")
{
    ffmpeg_wrapper::VideoDecoder decoder;
    decoder.createMedia(video_filename);
    decoder.getFrame(0);

    decoder.getFrame(250);

    // move back to frame 0 and check if the decoded frame is the same as frame 0
    for (int i = 249; i >= 100; --i) {
        auto frame_decoded = decoder.getFrame(i);
    }
    
    auto frame_100_decoded = decoder.getFrame(100);

    size_t diff_count = calculate_pixel_difference(frame_100, frame_100_decoded, tolerance);
    
    CHECK(diff_count == 0);

}

TEST_CASE("VideoDecoder get frame by frame past keyframe", "[ffmpeg_wrapper]") {

    ffmpeg_wrapper::VideoDecoder decoder;
    decoder.createMedia(video_filename);

    for (int i = 0; i < 250; ++i) {
        auto frame_decoded = decoder.getFrame(i);
    }

    decoder.getFrame(250);

    for (int i = 251; i < 300; ++i) {
        auto frame_decoded = decoder.getFrame(i);
    }

    auto frame_300_decoded = decoder.getFrame(300);

    size_t diff_count = calculate_pixel_difference(frame_300, frame_300_decoded, tolerance);
    CHECK(diff_count == 0);

}

TEST_CASE("VideoDecoder get frame count", "[ffmpeg_wrapper]") {

    ffmpeg_wrapper::VideoDecoder decoder;
    decoder.createMedia(video_filename);

    CHECK(decoder.getFrameCount() == 1000);
}
