set(HEADERS
    Decode.h
    IO.h
    IOInline.h
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
if(TLRENDER_FFMPEG_PLUGIN)
    list(APPEND HEADERS FFmpeg.h)
    list(APPEND HEADERS_PRIVATE FFmpegPrivate.h FFmpegReadPrivate.h)
    list(APPEND SOURCE FFmpeg.cpp FFmpegRead.cpp FFmpegReadAudio.cpp
        FFmpegReadVideo.cpp FFmpegWrite.cpp)
endif()
if(TLRENDER_FFMPEG_CMD)
    list(APPEND HEADERS FFmpegCmd.h)
    list(APPEND HEADERS_PRIVATE FFmpegCmdPrivate.h)
    list(APPEND SOURCE FFmpegCmd.cpp FFmpegCmdRead.cpp)
endif()
if(TLRENDER_OIIO)
    list(APPEND HEADERS OIIO.h)
    list(APPEND SOURCE OIIO.cpp OIIORead.cpp OIIOWrite.cpp)
endif()
if(TLRENDER_USD)
    list(APPEND HEADERS USD.h)
    list(APPEND HEADERS_PRIVATE USDPrivate.h)
    list(APPEND SOURCE USD.cpp USDRead.cpp USDRender.cpp)
endif()

add_library(tlIO ${HEADERS})
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
if(TLRENDER_FFMPEG_CMD)
    target_compile_definitions(tlIO PUBLIC TLRENDER_FFMPEG_CMD)
endif()
if(TLRENDER_OIIO)
    target_compile_definitions(tlIO PUBLIC TLRENDER_OIIO)
    target_link_libraries(tlIO PRIVATE OpenImageIO::OpenImageIO)
    if(TLRENDER_TIFF AND BUILD_SHARED_LIBS)
        # \bug OIIO link bug?
        target_link_libraries(tlIO PRIVATE TIFF::TIFF)
    endif()
endif()
if(TLRENDER_USD)
    target_compile_definitions(tlIO PUBLIC TLRENDER_USD)
    target_link_libraries(tlIO PRIVATE tf usd usdGeom usdImagingGL usdAppUtils hd)
endif()

set_target_properties(tlIO PROPERTIES FOLDER lib)
set_target_properties(tlIO PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(
    TARGETS tlIO
    EXPORT tlIOTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/tlRender/IO)
install(
    EXPORT tlIOTargets
    FILE tlIOTargets.cmake
    DESTINATION "lib/cmake/tlRender"
    NAMESPACE tlRender::)
