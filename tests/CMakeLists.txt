cmake_minimum_required(VERSION 3.21)

add_executable(ffmpeg_wrapper_test_suite
    test_main.cpp
)

target_link_libraries(ffmpeg_wrapper_test_suite
    PRIVATE
        ffmpeg_wrapper
        libavinc
)

target_include_directories(ffmpeg_wrapper_test_suite
    PRIVATE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>/headers
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>/libavinc/include
)

# define tests
add_test(
  NAME ffmpeg_wrapper_test_suite
  COMMAND $<TARGET_FILE:ffmpeg_wrapper_test_suite>
  )

install(TARGETS ffmpeg_wrapper_test_suite
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)