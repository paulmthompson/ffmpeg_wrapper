cmake_minimum_required(VERSION 3.4)
project(ffmpeg_wrapper VERSION 0.1.0)

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/cmake)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
enable_testing()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

#Create Library
set(Sources
        src/videodecoder.cpp
        src/libavinc.cpp
        ffmpeg_wrapper.cpp
        src/videoencoder.cpp
)

set(Headers
    headers/videodecoder.h
    headers/libavinc.hpp
    headers/videoencoder.h
)

set(Public_Headers
    headers/videodecoder.h
    headers/videoencoder.h
)

set(FFMPEG_PATH "C:\\Users\\wanglab\\Downloads\\ffmpeg-5.0-full_build-shared\\lib")
set(FFMPEG_INCLUDE "C:\\Users\\wanglab\\Downloads\\ffmpeg-5.0-full_build-shared\\include")

add_library(ffmpeg_wrapper SHARED ${Sources} ${Headers})

#add the include directories
target_include_directories(ffmpeg_wrapper
                           PRIVATE
                           #These are my directories
                           ${CMAKE_CURRENT_SOURCE_DIR}/headers
                           ${FFMPEG_INCLUDE}
                           PUBLIC
                           "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
                           "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)


#target_include_directories(ffmpeg_wrapper PRIVATE ${FFMPEG_INCLUDE})
#include_directories((${FFMPEG_INCLUDE}))

find_package(ffmpeg MODULE)

find_library(AVCODEC_LIBRARY avcodec HINTS ${FFMPEG_PATH})
find_library(AVFORMAT_LIBRARY avformat HINTS ${FFMPEG_PATH})
find_library(AVUTIL_LIBRARY avutil HINTS ${FFMPEG_PATH})
find_library(AVFILTER_LIBRARY avfilter HINTS ${FFMPEG_PATH})
find_library(SWSCALE_LIBRARY swscale HINTS ${FFMPEG_PATH})
find_library(SWRESAMPLE_LIBRARY swresample HINTS ${FFMPEG_PATH})

target_link_libraries(ffmpeg_wrapper PRIVATE
    ${AVCODEC_LIBRARY}
    ${AVFORMAT_LIBRARY}
    ${AVUTIL_LIBRARY}
    ${AVFILTER_LIBRARY}
    ${SWSCALE_LIBRARY}
    ${SWRESAMPLE_LIBRARY}
)

# note that ${public_headers} has to be in quotes
set_target_properties(ffmpeg_wrapper PROPERTIES PUBLIC_HEADER "${Public_Headers}")

#install and create export set
# install the target and create export-set
install(TARGETS ffmpeg_wrapper EXPORT ffmpeg_wrapperTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
        PUBLIC_HEADER DESTINATION include/ffmpeg_wrapper # include/SomeLibrary
)

#Install the header files
install(FILES ${Headers} DESTINATION include ${CMAKE_INSTALL_INCLUDEDIR})

target_link_options(ffmpeg_wrapper PRIVATE "-Wl,--as-needed")

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

include(CMakePackageConfigHelpers)

#Create config File
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/ffmpeg_wrapperConfig.cmake"
  INSTALL_DESTINATION cmake
)

#install config file
install(FILES
          "${CMAKE_CURRENT_BINARY_DIR}/ffmpeg_wrapperConfig.cmake"
        DESTINATION cmake
)

# generate and install export file
install(EXPORT ffmpeg_wrapperTargets
        FILE ffmpeg_wrapperTargets.cmake
        NAMESPACE ffmpeg_wrapper::
        DESTINATION cmake
)

#Generate the export targets for the build tree
export(EXPORT "ffmpeg_wrapperTargets"
    FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/ffmpeg_wrapperTargets.cmake"
    NAMESPACE ffmpeg_wrapper::
)