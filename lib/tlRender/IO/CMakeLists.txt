# PNG is unconditional: the codec comes from feather-tk, which is always
# there, so every configuration has one image format.
set(HEADERS
    Decode.h
    Export.h
    IO.h
    IOInline.h
    PNG.h
    Plugin.h
    Read.h
    SeqDecode.h
    SeqIO.h
    System.h
    SystemInline.h
    Write.h)
set(HEADERS_PRIVATE
    RequestQueuePrivate.h)

set(SOURCE
    Decode.cpp
    IO.cpp
    PNG.cpp
    PNGRead.cpp
    PNGWrite.cpp
    Plugin.cpp
    Read.cpp
    SeqDecode.cpp
    SeqIO.cpp
    SeqIOWrite.cpp
    System.cpp
    Write.cpp)

if(TLRENDER_EXR)
    list(APPEND HEADERS EXR.h)
    list(APPEND HEADERS_PRIVATE EXRPrivate.h)
    list(APPEND SOURCE EXR.cpp EXRRead.cpp EXRWrite.cpp)
endif()
if(EMSCRIPTEN)
    list(APPEND HEADERS WebCodecs.h)
    list(APPEND SOURCE WebCodecs.cpp)
endif()
if(TLRENDER_FFMPEG_PLUGIN)
    # The command line is a path within the plugin rather than a plugin of
    # its own, so it is built with it: every configuration that tests FFmpeg
    # now compiles the command line reader as well.
    list(APPEND HEADERS FFmpeg.h FFmpegCmd.h)
    list(APPEND HEADERS_PRIVATE FFmpegPrivate.h FFmpegReadPrivate.h
        FFmpegCmdPrivate.h)
    list(APPEND SOURCE FFmpeg.cpp FFmpegRead.cpp FFmpegReadAudio.cpp
        FFmpegReadVideo.cpp FFmpegWrite.cpp FFmpegCmd.cpp FFmpegCmdRead.cpp
        FFmpegCmdWrite.cpp)
endif()
if(TLRENDER_OIIO)
    list(APPEND HEADERS OIIO.h)
    list(APPEND SOURCE OIIO.cpp OIIORead.cpp OIIOWrite.cpp)
endif()
if(TLRENDER_SVG)
    list(APPEND HEADERS SVG.h)
    list(APPEND SOURCE SVG.cpp SVGRead.cpp)
endif()

add_library(tlIO ${HEADERS})
# Private when building, so this library's own API is dllexport here and
# dllimport to everything else. Public when static, which a consumer does
# have to know: the API then carries no declspec at all.
if(BUILD_SHARED_LIBS)
    target_compile_definitions(tlIO PRIVATE TL_IO_EXPORTS)
else()
    target_compile_definitions(tlIO PUBLIC TL_IO_STATIC)
endif()
target_sources(tlIO PRIVATE ${HEADERS_PRIVATE} ${SOURCE})

target_include_directories(tlIO
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tlIO PUBLIC tlCore ftk::ftkGL)

if(TLRENDER_EXR)
    target_compile_definitions(tlIO PUBLIC TLRENDER_EXR)
    target_link_libraries(tlIO PRIVATE OpenEXR::OpenEXR)
endif()
if(TLRENDER_FFMPEG_PLUGIN)
    target_compile_definitions(tlIO PUBLIC TLRENDER_FFMPEG_PLUGIN)
    target_link_libraries(tlIO PRIVATE FFmpeg::FFmpeg)
endif()
if(TLRENDER_OIIO)
    target_compile_definitions(tlIO PUBLIC TLRENDER_OIIO)
    target_link_libraries(tlIO PRIVATE OpenImageIO::OpenImageIO)
endif()
if(TLRENDER_SVG)
    target_compile_definitions(tlIO PUBLIC TLRENDER_SVG)
    target_link_libraries(tlIO PRIVATE lunasvg::lunasvg)
endif()

set_target_properties(tlIO PROPERTIES FOLDER lib)
set_target_properties(tlIO PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(
    TARGETS tlIO
    EXPORT tlIOTargets
    ARCHIVE DESTINATION lib COMPONENT dev
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/tlRender/IO COMPONENT dev)
install(
    EXPORT tlIOTargets
    FILE tlIOTargets.cmake
    DESTINATION "lib/cmake/tlRender"
    NAMESPACE tlRender::
    COMPONENT dev)
