cmake_minimum_required(VERSION 3.24)
project(ffmpeg_wrapper VERSION 0.1.0)

#[[
Best practice for looking for cmake modules cmake folder in root of the project
Currently, this project does not have any cmake modules
From Professional CMake Chapter 11 Modules
]]
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#[[
Best practice for rpath from Professional Cmake regarding rpath (26.2.2)
]]
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

#[[
Manifest mode from VCPKG will make all dependancies specified in vcpkg.json be associated and installed
with this particular project
]]
set(VCPKG_MANIFEST_MODE ON)

#Create Library
set(Sources
        src/videodecoder.cpp
        src/videoencoder.cpp
)

set(Headers
    headers/ffmpeg_wrapper/videodecoder.h
    headers/ffmpeg_wrapper/videoencoder.h
)

set(Public_Headers
    headers/ffmpeg_wrapper/videodecoder.h
    headers/ffmpeg_wrapper/videoencoder.h
)

#Setup Testing
include(CTest)

if (BUILD_TESTING)
  enable_testing()
endif()


#[[
ffmpeg_wrapper is a shared library
I am also creating an alias with the format that has the same structure as the library when it is imported as a target from find_package
I think this helps with in-source builds, but I am not sure

the nlohmann_json library also uses an alias, but I am not sure if it is for this purpose.
CMake startup from c++ weekly also does this
]]
add_library(ffmpeg_wrapper SHARED ${Sources} ${Headers})
add_library(ffmpeg_wrapper::ffmpeg_wrapper ALIAS ffmpeg_wrapper)

#[[
FFMPEG is a dependancy of this project, specifically the libavinc interface library
FFMPEG sublibraries are found using pkg-config

https://github.com/microsoft/vcpkg/pull/19308
https://gist.github.com/xdlg/18830e1cdf53e2e20a435b77fb6d9dd5
]]
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

                           "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libavinc/include>"
                           
                           PUBLIC
                           "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/headers/ffmpeg_wrapper>"
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
