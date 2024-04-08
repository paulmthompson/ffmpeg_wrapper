#=============================
# Preamble
#=============================
cmake_minimum_required(VERSION 3.24)
project(ffmpeg_wrapper VERSION 0.1.0)
#[[
The CTest module helps with several testing functions.
It defines the BUILD_TESTING cache variable that defaults to true, and also automatically calls enable_testing(), so this does not need to be called manually
CTest should be included early in the top level CMakeLists.txt file
Professional CMake 25.10.3
]]
include(CTest)


#=============================
# Project Wide Setup
#=============================

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

#=============================
# Externally Provided Content
#=============================

add_subdirectory(dependencies)

#=============================
# Main Build Targets
#=============================

add_subdirectory(src)



#[[
BUILD_TESTING is a cache variable that defaults to true which was defined by the CTest module
If we wish to run testing, we should add the tests subdirectory.
]]
if (BUILD_TESTING)
  add_subdirectory(tests)
endif()

add_subdirectory(packaging)
