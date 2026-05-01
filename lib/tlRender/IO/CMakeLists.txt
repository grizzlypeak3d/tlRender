set(HEADERS
    IO.h
    IOInline.h
    Plugin.h
    Read.h
    SeqIO.h
    System.h
    SystemInline.h
    Write.h)
set(HEADERS_PRIVATE
    SeqIOReadPrivate.h)

set(SOURCE
    IO.cpp
    Plugin.cpp
    Read.cpp
    SeqIO.cpp
    SeqIORead.cpp
    SeqIOWrite.cpp
    System.cpp
    Write.cpp)

set(LIBRARIES)
set(LIBRARIES_PRIVATE)
if(TLRENDER_EXR)
    list(APPEND HEADERS EXR.h)
    list(APPEND HEADERS_PRIVATE EXRPrivate.h)
    list(APPEND SOURCE EXR.cpp EXRRead.cpp EXRWrite.cpp)
    list(APPEND LIBRARIES_PRIVATE OpenEXR::OpenEXR)
endif()
if(TLRENDER_FFMPEG_PLUGIN)
    list(APPEND HEADERS FFmpeg.h)
    list(APPEND HEADERS_PRIVATE FFmpegPrivate.h FFmpegReadPrivate.h)
    list(APPEND SOURCE FFmpeg.cpp FFmpegRead.cpp FFmpegReadAudio.cpp
        FFmpegReadVideo.cpp FFmpegWrite.cpp)
    list(APPEND LIBRARIES_PRIVATE tlRender::FFmpeg)
else()
    list(APPEND HEADERS FFmpegCmd.h)
    list(APPEND HEADERS_PRIVATE FFmpegCmdPrivate.h)
    list(APPEND SOURCE FFmpegCmd.cpp FFmpegCmdRead.cpp)
endif()
if(TLRENDER_OIIO)
    list(APPEND HEADERS OIIO.h)
    list(APPEND SOURCE OIIO.cpp OIIORead.cpp OIIOWrite.cpp)
    list(APPEND LIBRARIES_PRIVATE OpenImageIO::OpenImageIO)
    if(TLRENDER_TIFF AND BUILD_SHARED_LIBS)
        # \bug OIIO link bug?
        list(APPEND LIBRARIES_PRIVATE TIFF::TIFF)
    endif()
endif()
if(TLRENDER_USD)
    list(APPEND HEADERS USD.h)
    list(APPEND HEADERS_PRIVATE USDPrivate.h)
    list(APPEND SOURCE USD.cpp USDRead.cpp USDRender.cpp)
    list(APPEND LIBRARIES_PRIVATE tf usd usdGeom usdImagingGL usdAppUtils hd)
endif()
if(TLRENDER_WMF)
    list(APPEND HEADERS WMF.h)
    list(APPEND SOURCE WMF.cpp WMFRead.cpp)
    list(APPEND LIBRARIES_PRIVATE mf mfplat mfreadwrite mfuuid propsys wmcodecdspuuid)
endif()
if(TLRENDER_AVF)
    list(APPEND HEADERS AVF.h)
    list(APPEND SOURCE AVF.cpp AVFRead.mm)
    set_source_files_properties(AVFRead.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    list(APPEND LIBRARIES_PRIVATE
        "-framework AVFoundation"
        "-framework CoreMedia"
        "-framework CoreVideo"
        "-framework Foundation")
endif()
list(APPEND LIBRARIES tlCore ftk::ftkGL)

add_library(tlIO ${HEADERS})
target_sources(tlIO PRIVATE ${HEADERS_PRIVATE} ${SOURCE})
target_link_libraries(tlIO PUBLIC ${LIBRARIES} PRIVATE ${LIBRARIES_PRIVATE})
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
