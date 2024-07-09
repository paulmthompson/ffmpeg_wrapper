
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

TEST_CASE("VideoDecoder object creation", "[ffmpeg_wrapper]") {
    SECTION("Create a VideoDecoder object") {

        std::string video_filename = "data/test_each_frame_number.mp4";

        ffmpeg_wrapper::VideoDecoder decoder;
        decoder.createMedia(video_filename);

        CHECK(decoder.getHeight() == 480);
        CHECK(decoder.getWidth() == 640);

        static auto frame_0 = load_img("data/frame_0.bin");

        auto frame_0_decoded = decoder.getFrame(0);

        save_decoded_image_as_binary(frame_0_decoded, "data/decoded_frame_0.bin");

        CHECK(frame_0.size() == frame_0_decoded.size());

        int tolerance = 5;

        size_t diff_count = 0;
        for(size_t i = 0; i < frame_0.size(); ++i) {
            if(std::abs(frame_0[i] - frame_0_decoded[i]) > tolerance) {
                ++diff_count;
            }
        }

        std::cout << "Number of different elements: " << diff_count << std::endl;

        CHECK(diff_count == 0);
    }
}