cmake_minimum_required(VERSION 3.21)
project(ffmpeg_wrapper VERSION 0.1.0)

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/cmake)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#Best practice for rpath from Professional Cmake
#set before defining library.
#alternatively I think we can use set target properties after library is defined
if(APPLE)
        set(base @loader_path)
else()
        set(base $ORIGIN)
endif()

include(GNUInstallDirs)
file(RELATIVE_PATH relDir
        ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}
        ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}
)
set(CMAKE_INSTALL_RPATH ${base} ${base}/${relDir})

set(VCPKG_MANIFEST_MODE ON)

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
#I am going to exclude system ffmpeg and only use vcpkg to build. 
install(RUNTIME_DEPENDENCY_SET appDeps
        PRE_EXCLUDE_REGEXES
                [[libc\.so\..*]] [[libgcc_s\.so\..*]] [[libm\.so\..*]] [[libstdc\+\+\.so\..*]]
                [[ld.*]] [[libbz2.*]] [[libdl.*]] [[libgmp.*]] [[libgnutls.*]] [[libhogweed.*]]
                [[libpthread.*]] [[librt.*]] [[libz.*]]
        POST_EXCLUDE_REGEXES
                [[/lib/x86_64-linux-gnu/libavcodec.*]]
                [[/lib/x86_64-linux-gnu/libavutil.*]]
                [[/lib/x86_64-linux-gnu/libswresample.*]]
)

ENDIF()

#Setup Testing
include(CTest)

if (BUILD_TESTING)
  add_subdirectory(tests)
endif()


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
