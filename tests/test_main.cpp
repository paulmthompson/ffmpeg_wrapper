
#include <videoencoder.h>

#include <memory>

int main() {

    std::unique_ptr<ffmpeg_wrapper::VideoEncoder> vd = std::make_unique<ffmpeg_wrapper::VideoEncoder>();

    return 0;
}