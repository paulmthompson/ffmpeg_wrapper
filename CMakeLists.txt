# Preamble
cmake_minimum_required(VERSION 3.24)
project(ffmpeg_wrapper VERSION 0.1.0)
#[[
The CTest module helps with several testing functions.
It defines the BUILD_TESTING cache variable that defaults to true, and also automatically calls enable_testing(), so this does not need to be called manually
CTest should be included early in the top level CMakeLists.txt file
Professional CMake 25.10.3
]]
include(CTest)


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

if (WIN32)
        if (MSVC)
                set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /fsanitize=address")
                message(STATUS "This is a Debug build for MSVC, enabling address sanitizer")
        endif()
endif()

if (UNIX)
        if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
                message(STATUS "This is a Debug build for Clang, enabling address sanitizer")
                set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
        elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GCC")
                set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
        endif()
endif()


#[[
ffmpeg_wrapper is a shared library

I am also creating an alias with the format that has the same structure as the library when it is imported as a target from find_package
This is recommended practice so that the library can be linked to with ffmpeg_wrapper::ffmpeg_wrapper as an in-source build or 
as a package installed with find_package.
Professional CMake 17.4
]]
add_library(ffmpeg_wrapper SHARED ${Sources} ${Headers})
add_library(ffmpeg_wrapper::ffmpeg_wrapper ALIAS ffmpeg_wrapper)


add_subdirectory(dependencies)

target_link_libraries(ffmpeg_wrapper PUBLIC libavinc)

#[[
Here I link the include directories for ffmpeg_wrapper.
I add both headers and headers/ffmpeg_wrapper so that they can be included with both ffmpeg_wrapper/video_encoder.h and video_encoder.h
The BUILD_INTERFACCE is used to specify the location of in-source builds

Because we later install the target with the "INCLUDES DESTINATION" section, we do NOT need to specify the INSTALL_INTERFACE
generator expression here because it would be redundant
The include directories will be installed in CMAKE_INSTALL_INCLUDEDIR, so this is the location that will be used for installed builds
Professional CMake 26.10

We also want to include the libavinc include directory, also declared PUBLIC for reasons seen above for linking the library
]]
target_include_directories(ffmpeg_wrapper               
                           PUBLIC
                           "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/headers/ffmpeg_wrapper>"
                           "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/headers>"

                           "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/dependencies/libavinc/include>"
)

#[[
Public headers are installed with their relative paths removed
]]
set(Public_Headers
    headers/ffmpeg_wrapper/videodecoder.h
    headers/ffmpeg_wrapper/videoencoder.h
)
set_target_properties(ffmpeg_wrapper PROPERTIES PUBLIC_HEADER "${Public_Headers}")

#[[
In general, it is not good practice to export all symbols from a shared library
Specific class and functions can be exported using the __declspec(dllexport) keywords in the header file
]]
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

#[[
Here we define what is installed by the cmake install command.

Here export ffmpeg_wrapperTargets names the export set that they belong too.
The export set itself is installed later using teh install(EXPORT ffmpeg_wrapperTargets ...) command
Professional CMake 26.2

Note that libavinc must also be installed as a target since it is a PUBLIC dependancy of ffmpeg_wrapper
]]
install(TARGETS ffmpeg_wrapper libavinc
        EXPORT ffmpeg_wrapperTargets
        LIBRARY 
                DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE 
                DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME 
                DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES 
                DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        PUBLIC_HEADER 
                DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ffmpeg_wrapper # include/SomeLibrary
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

#[[
BUILD_TESTING is a cache variable that defaults to true which was defined by the CTest module
If we wish to run testing, we should add the tests subdirectory.
]]
if (BUILD_TESTING)
  add_subdirectory(tests)
endif()


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
