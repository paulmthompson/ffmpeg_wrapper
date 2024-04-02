cmake_minimum_required(VERSION 3.24)

project(ffmpeg_wrapper_test_suite)

add_executable(ffmpeg_wrapper_test_suite
    test_main.cpp 
)

set_target_properties(ffmpeg_wrapper_test_suite PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/video-encoder-tests
)

target_link_libraries(ffmpeg_wrapper_test_suite
    PRIVATE
    ffmpeg_wrapper
    libavinc
)

target_include_directories(ffmpeg_wrapper_test_suite
    PRIVATE
    $<BUILD_INTERFACE:${ffmpeg_wrapper_source_dir}>/headers
    $<BUILD_INTERFACE:${ffmpeg_wrapper_source_dir}>/libavinc/include
)

# define tests
add_test(
  NAME ffmpeg_wrapper_test_suite
  COMMAND $<TARGET_FILE:ffmpeg_wrapper_test_suite>
  )

#On windows, I can't seem to find the top level path with the ffmpeg_wrapper.dll
#This was in Testing section of Craig Scott's Professional CMake book
if (WIN32)
    set(ffmpeg_wrapper_dir "$<SHELL_PATH:$<TARGET_FILE_DIR:ffmpeg_wrapper>>")

    set_property(TEST ffmpeg_wrapper_test_suite APPEND PROPERTY
            ENVIRONMENT_MODIFICATION
                PATH=path_list_prepend:${ffmpeg_wrapper_dir}
    )
endif()
#install(TARGETS ffmpeg_wrapper_test_suite
#  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
#)