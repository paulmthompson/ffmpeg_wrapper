

#[[
libavinc is the c++ wrapper around ffmpeg and includes many convenience methods.
LIbavinc is a header-only wrapper around ffmpeg and defined as an interface library (meaning it is not built and doesn't create a unique object file)
While libraries consuming ffmpeg_wrapper will not use libavinc directly, ffmpeg_wrapper does use libavinc in its header files, and consequently
libavinc needs to be installed as a target as well, and the dependancy should be declared PUBLIC.
]]
add_subdirectory(libavinc)

add_subdirectory(boost)