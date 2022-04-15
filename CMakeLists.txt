cmake_minimum_required(VERSION 3.0.0)
project(ffmpeg_wrapper VERSION 0.1.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
enable_testing()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)


set(FFMPEG_PATH "C:\\Users\\wanglab\\Downloads\\ffmpeg-5.0-full_build-shared\\lib")
set(FFMPEG_INCLUDE "C:\\Users\\wanglab\\Downloads\\ffmpeg-5.0-full_build-shared\\include")

find_library(AVCODEC_LIBRARY avcodec HINTS ${FFMPEG_PATH})
find_library(AVFORMAT_LIBRARY avformat HINTS ${FFMPEG_PATH})
find_library(AVUTIL_LIBRARY avutil HINTS ${FFMPEG_PATH})
find_library(AVFILTER_LIBRARY avfilter HINTS ${FFMPEG_PATH})
find_library(SWSCALE_LIBRARY swscale HINTS ${FFMPEG_PATH})
find_library(SWRESAMPLE_LIBRARY swresample HINTS ${FFMPEG_PATH})

include_directories(${FFMPEG_INCLUDE})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/headers)

set(Sources
        src/videodecoder.cpp
        src/libavinc.cpp
        ffmpeg_wrapper.cpp
)

set(Headers
    headers/videodecoder.h
    headers/libavinc.hpp
)

add_library(ffmpeg_wrapper SHARED ${Sources} ${Headers})

#target_sources(ffmpeg_wrapper
#    PRIVATE 
#        ${CMAKE_CURRENT_LIST_DIR}/src/videodecoder.cpp 
#        ${CMAKE_CURRENT_LIST_DIR}/src/libavinc.cpp 
#    PUBLIC
#        ${CMAKE_CURRENT_LIST_DIR}/headers/videodecoder.h 
#        ${CMAKE_CURRENT_LIST_DIR}/headers/libavinc.hpp 
#        ${CMAKE_CURRENT_LIST_DIR}/ffmpeg_wrapper.cpp
#)

target_link_libraries(ffmpeg_wrapper PUBLIC
    ${AVCODEC_LIBRARY}
    ${AVFORMAT_LIBRARY}
    ${AVUTIL_LIBRARY}
    ${AVFILTER_LIBRARY}
    ${SWSCALE_LIBRARY}
    ${SWRESAMPLE_LIBRARY}
)

#Does this 
target_include_directories(ffmpeg_wrapper PRIVATE .)

#install(TARGETS ffmpeg_wrapper
#    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

target_link_options(ffmpeg_wrapper PRIVATE "-Wl,--as-needed")

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
