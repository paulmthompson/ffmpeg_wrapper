if(PROJECT_IS_TOP_LEVEL)
    option(ENABLE_IWYU "Enable include-what-you-use" OFF)
    if(ENABLE_IWYU)
        set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
        find_program(INCLUDE_WHAT_YOU_USE_EXECUTABLE
                include-what-you-use
                REQUIRED
        )
        set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE
                ${INCLUDE_WHAT_YOU_USE_EXECUTABLE}
                -Xiwyu --mapping_file=${CMAKE_CURRENT_SOURCE_DIR}/cmake/iwyu_mapping.imp
                CACHE STRING "Include-what-you-use command"
        )
    endif()
endif()