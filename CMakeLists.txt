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

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(enableAddressSanitizer "Enable Address Sanitizer" OFF)

include(set_rpath)
include(enable_sanitizers)
include(iwyu)

#[[
Manifest mode from VCPKG will make all dependancies specified in vcpkg.json be associated and installed
with this particular project
]]

set(VCPKG_MANIFEST_MODE ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

#=============================
# Externally Provided Content
#=============================

add_subdirectory(dependencies)

#=============================
# Main Build Targets
#=============================

add_subdirectory(src)

#=============================
# Testing
#=============================

#[[
BUILD_TESTING is a cache variable that defaults to true which was defined by the CTest module
If we wish to run testing, we should add the tests subdirectory.
]]
if (BUILD_TESTING)
  add_subdirectory(tests)
endif()

#=============================
# Packaging
#=============================

add_subdirectory(packaging)
