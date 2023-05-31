cmake_minimum_required(VERSION 3.24)

#[[
Libavinc is a header only library that provides a c++ wrapper around multiple ffmpeg libraries.
It is intended to be used as a dependency for other projects.
]]
add_library(libavinc INTERFACE)

#[[
libavinc is a header only library, so we add the include directories as a interface 

]]
target_include_directories(libavinc INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>")

message(STATUS ${FFMPEG_INCLUDE_DIRS})
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