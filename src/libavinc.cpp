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


///////////////////////////////////////////////////////////////////////////////

} // End libav namespace
#endif // LIBAVINC_CPP
