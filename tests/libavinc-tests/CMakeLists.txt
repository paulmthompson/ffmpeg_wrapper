add_executable(libavinc_tests
    test_libavinc.cpp
)

target_link_libraries(libavinc_tests PRIVATE
        Catch2::Catch2WithMain
        libavinc
)

if (WIN32)
    catch_discover_tests(libavinc_tests
            DL_PATHS
            "$ENV{PATH}"
            "${CMAKE_BINARY_DIR}"
    )
else()
    catch_discover_tests(libavinc_tests)
endif()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/data)

function(copy_files TARGET_NAME SOURCE_DIR DEST_DIR FILES)
    foreach(FILE ${FILES})
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                ${SOURCE_DIR}/${FILE}
                ${DEST_DIR}/${FILE})
    endforeach()
endfunction()

set(DATA_FILES_TO_COPY
        "test_each_frame_number.mp4"
        "frame_0.bin"
        "frame_100.bin"
        "frame_200.bin"
        "frame_300.bin"
        "frame_400.bin"
)

copy_files(libavinc_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/data"
        "${CMAKE_CURRENT_BINARY_DIR}/data"
        "${DATA_FILES_TO_COPY}"
)
