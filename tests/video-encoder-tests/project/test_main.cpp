
#include <videoencoder.h>

#include <memory>
#include <random>
#include <string>
#include <iostream>
#include <algorithm>

int main() {

    std::unique_ptr<ffmpeg_wrapper::VideoEncoder> ve = std::make_unique<ffmpeg_wrapper::VideoEncoder>();

    const int width = 640;
    const int height = 480;
    const int fps = 25;
    const int total_frames = 100;
    const std::string saveFileName = "./test.mp4"; //TODO set different paths and some that don't make sense

    ve->setSavePath(saveFileName);

    std::cout << "Set save File path as " << saveFileName << std::endl;

    ve->createContext(width, height, fps);

    std::cout << "Create context for " << width << " x " << height << " at " << fps << " frames per second " << std::endl;

    ve->set_pixel_format(ffmpeg_wrapper::VideoEncoder::INPUT_PIXEL_FORMAT::GRAY8);

    std::cout << "Pixel format set to GRAY8" << std::endl;

    ve->openFile();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::vector<uint8_t> data(width * height);

    for (int i = 0; i < total_frames; ++i) {
        std::generate(data.begin(), data.end(), [&]() { return dis(gen); });
        ve->writeFrameGray8(data);
    }

    ve->enterDrainMode();
    

    ve->closeFile();

    std::cout << "Done writing" << std::endl;


    return 0;
}