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
