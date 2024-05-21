cmake_minimum_required(VERSION 3.24)


add_library(libboost INTERFACE)

#[[
Libavinc depends on ffmpeg.
The following libraries and includes need to be linked as follows:
https://github.com/microsoft/vcpkg/blob/master/ports/ffmpeg/usage

Note that Interface means that targets that link to libavinc will need to be linked to ffmpeg as well.

The BUILD_INTERFACE generator expression must be included here to avoid errors with ffmpeg being located in the build directory:
https://www.reddit.com/r/cmake/comments/n9hue2/comment/gxqmtl8/
]]
find_package(Boost REQUIRED)
target_include_directories(libboost INTERFACE
        "$<BUILD_INTERFACE:${Boost_INCLUDE_DIRS}>"
)

include(GNUInstallDirs)
install(TARGETS libboost
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
