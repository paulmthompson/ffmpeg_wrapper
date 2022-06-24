cmake_minimum_required(VERSION 3.21)


add_library(libavinc INTERFACE)

target_include_directories(libavinc INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
                                                "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

message(STATUS ${FFMPEG_INCLUDE_DIRS})
target_include_directories(libavinc INTERFACE ${FFMPEG_INCLUDE_DIRS})

target_link_libraries(libavinc INTERFACE PkgConfig::FFMPEG)

include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
