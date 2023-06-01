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
            INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>")

message(STATUS "Looking for FFMPEG Include Directories: "  ${FFMPEG_INCLUDE_DIRS})            
target_include_directories(libavinc INTERFACE ${FFMPEG_INCLUDE_DIRS})

#[[
Libavinc depends on ffmpeg. Pkg-config found these libraries, so we link to them here.
]]
target_link_libraries(libavinc INTERFACE PkgConfig::FFMPEG)

include(GNUInstallDirs)
install(TARGETS libavinc
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

#[[
Without this line, the headers will not be installed when ffmpeg_wrapper is used as a library
Is this because the target libavinc is never installed?
]]
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")