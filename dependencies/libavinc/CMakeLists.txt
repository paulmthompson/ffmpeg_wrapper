cmake_minimum_required(VERSION 3.24)

#[[
Libavinc is a header only library that provides a c++ wrapper around multiple ffmpeg libraries.
It is intended to be used as a dependency for other projects.

Libavinc is header-only, so it can be added as an "Interface"
]]
add_library(libavinc INTERFACE)

#[[
libavinc is a header only library, so we add the include directories as a interface 
Professional CMake 17.2.4
]]
target_include_directories(libavinc 
            INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
            )

#[[
Libavinc depends on ffmpeg.
The following libraries and includes need to be linked as follows:
https://github.com/microsoft/vcpkg/blob/master/ports/ffmpeg/usage

Note that Interface means that targets that link to libavinc will need to be linked to ffmpeg as well.

The BUILD_INTERFACE generator expression must be included here to avoid errors with ffmpeg being located in the build directory:
https://www.reddit.com/r/cmake/comments/n9hue2/comment/gxqmtl8/
]]   
find_package(FFMPEG REQUIRED)
target_include_directories(libavinc INTERFACE 
    "$<BUILD_INTERFACE:${FFMPEG_INCLUDE_DIRS}>"
    )
target_link_directories(libavinc INTERFACE 
    "$<BUILD_INTERFACE:${FFMPEG_LIBRARY_DIRS}>"
    )
target_link_libraries(libavinc INTERFACE 
    ${FFMPEG_LIBRARIES}
    )

include(GNUInstallDirs)
install(TARGETS libavinc
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

#[[
Without this line, the headers will not be installed when ffmpeg_wrapper is used as a library
Is this because the target libavinc is never installed?
]]
install(DIRECTORY include DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")