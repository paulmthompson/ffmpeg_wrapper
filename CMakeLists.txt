cmake_minimum_required(VERSION 3.21)
project(ffmpeg_wrapper VERSION 0.1.0)

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/cmake)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#include(CTest)
#enable_testing()

#set(CPACK_PROJECT_NAME ${PROJECT_NAME})
#set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
#include(CPack)

#Create Library
set(Sources
        src/videodecoder.cpp
        src/videoencoder.cpp
)

set(Headers
    headers/videodecoder.h
    headers/videoencoder.h
)

set(Public_Headers
    headers/videodecoder.h
    headers/videoencoder.h
)

add_library(ffmpeg_wrapper SHARED ${Sources} ${Headers})

#https://github.com/microsoft/vcpkg/pull/19308
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
                libavcodec
                libavformat
                libavutil
                libswscale
                libswresample
                )

add_subdirectory(libavinc)

#add the include directories
target_include_directories(ffmpeg_wrapper
                           PUBLIC
                           #These are my directories
#                           ${CMAKE_CURRENT_SOURCE_DIR}/headers
                        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libavinc/include>"
                           
                           PUBLIC
                           "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/headers>"
                           "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(ffmpeg_wrapper PRIVATE libavinc)

# note that ${public_headers} has to be in quotes
set_target_properties(ffmpeg_wrapper PROPERTIES PUBLIC_HEADER "${Public_Headers}")

#install and create export set
# install the target and create export-set
install(TARGETS ffmpeg_wrapper
        EXPORT ffmpeg_wrapperTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
        PUBLIC_HEADER DESTINATION include/ffmpeg_wrapper # include/SomeLibrary
)

install(TARGETS ffmpeg_wrapper RUNTIME_DEPENDENCY_SET appDeps)

IF (WIN32)

install(RUNTIME_DEPENDENCY_SET appDeps
        PRE_EXCLUDE_REGEXES
                [[api-ms-win-.*]] [[ext-ms-.*]] [[kernel32\.dll]] 
                [[bcrypt.dll]] [[mfplat.dll]] [[msvcrt.dll]] [[ole32.dll]] [[secur32.dll]] [[user32.dll]] [[vcruntime140.dll]]
                [[ws2_32.dll]]
                [[libgcc_s_seh-1\.dll]] [[libstdc\+\+\-6.dll]] 
        POST_EXCLUDE_REGEXES
                [[.*/system32/.*\.dll]]
)

ELSE()

#set(MY_STRING "/home/vcpkg/test")
#message(STATUS "MY_STRING = ${MY_STRING}")
#string(REGEX MATCH ".+\\v\\c\\p\\k\\g.*" FIRST_LINE "${MY_STRING}")
#string(REGEX MATCH ".*vcpkg.*" FIRST_LINE "${MY_STRING}")
#message(STATUS "FIRST_LINE = ${FIRST_LINE}")


#install(RUNTIME_DEPENDENCY_SET appDeps
#        PRE_EXCLUDE_REGEXES
#                [[libc\.so\..*]] [[libgcc_s\.so\..*]] [[libm\.so\..*]] [[libstdc\+\+\.so\..*]]
#                [[ld.*]] [[libbz2.*]] [[libdl.*]] [[libgmp.*]] [[libgnutls.*]] [[libhogweed.*]]
#                [[libpthread.*]] [[librt.*]] [[libz.*]]
#        PRE_INCLUDE_REGEXES
#                ".*vcpkg.*"
#)

ENDIF()



#Install the header files

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