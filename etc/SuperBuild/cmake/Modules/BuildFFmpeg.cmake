include(ExternalProject)

if(WIN32)
    # Build FFmpeg with MSYS2 on Windows.
    find_package(Msys REQUIRED)
endif()

# The GitHub mirror rather than ffmpeg.org, which has been failing to serve
# the releases. Still a download rather than a clone, so the build steps
# behind it are not disturbed. What it does not carry is the VERSION file the
# release tarballs have, nor a .git to read a tag from, so FFmpeg builds
# itself as version "unknown" -- av_version_info() is not read anywhere here,
# and the library versions come from the source.
#
# The library versions are written out in the install names below and in the
# packaging, so check them against libavutil/version.h and its siblings when
# this moves: a point release bumps them.
set(FFmpeg_URL https://github.com/FFmpeg/FFmpeg/archive/refs/tags/n9.0.1.tar.gz)

set(FFmpeg_DEPS)
if(TLRENDER_AOM)
    list(APPEND FFmpeg_DEPS aom)
endif()
if(TLRENDER_SVTAV1)
    list(APPEND FFmpeg_DEPS svt-av1)
endif()
if(TLRENDER_NASM)
    list(APPEND FFmpeg_DEPS NASM)
endif()
if(NOT WIN32 AND NOT APPLE)
    list(APPEND FFmpeg_DEPS nv-codec-headers)
endif()

set(FFmpeg_SHARED_LIBS ON)
set(FFmpeg_DEBUG OFF)
set(FFmpeg_CFLAGS "--extra-cflags=-I${CMAKE_INSTALL_PREFIX}/include")
set(FFmpeg_CXXFLAGS "--extra-cxxflags=-I${CMAKE_INSTALL_PREFIX}/include")
set(FFmpeg_OBJCFLAGS "--extra-objcflags=-I${CMAKE_INSTALL_PREFIX}/include")
set(FFmpeg_LDFLAGS)
if(WIN32)
    list(APPEND FFmpeg_LDFLAGS "--extra-ldflags=/LIBPATH:${CMAKE_INSTALL_PREFIX}/lib")
    if(CMAKE_BUILD_TYPE MATCHES "^Debug$")
        list(APPEND FFmpeg_CFLAGS "--extra-cflags=-MDd")
        list(APPEND FFmpeg_CXXFLAGS "--extra-cxxflags=-MDd")
        list(APPEND FFmpeg_LDFLAGS "--extra-ldflags=-MDd")
    else()
        list(APPEND FFmpeg_CFLAGS "--extra-cflags=-MD")
        list(APPEND FFmpeg_CXXFLAGS "--extra-cxxflags=-MD")
        list(APPEND FFmpeg_LDFLAGS "--extra-ldflags=-MD")
    endif()
elseif(APPLE)
    list(APPEND FFmpeg_LDFLAGS "--extra-ldflags=-L${CMAKE_INSTALL_PREFIX}/lib")
else()
    list(APPEND FFmpeg_LDFLAGS "--extra-ldflags=-L${CMAKE_INSTALL_PREFIX}/lib")
    list(APPEND FFmpeg_LDFLAGS "--extra-ldflags=-L${CMAKE_INSTALL_PREFIX}/lib64")
    # The libraries find each other and the codecs beside them; see the
    # rpath note in the super build's CMakeLists. Spelled for everything
    # between here and the linker: configure evaluates the value once, which
    # takes the backslashes; make expands it twice on the way to the
    # link, each turning "$$" into "$"; and the quotes keep the shell from
    # expanding what is left. Checked by reading the rpath out of the built
    # library -- fewer dollars give "RIGIN".
    list(APPEND FFmpeg_LDFLAGS "--extra-ldflags=-Wl,-rpath,'\\$\\$\\$\\$ORIGIN'")
endif()
if(APPLE AND CMAKE_OSX_DEPLOYMENT_TARGET)
    list(APPEND FFmpeg_CFLAGS "--extra-cflags=-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    list(APPEND FFmpeg_CXXFLAGS "--extra-cxxflags=-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    list(APPEND FFmpeg_OBJCFLAGS "--extra-objcflags=-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    list(APPEND FFmpeg_LDFLAGS "--extra-ldflags=-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()
if(FFmpeg_DEBUG)
    list(APPEND FFmpeg_CFLAGS "--extra-cflags=-g")
    list(APPEND FFmpeg_CXXFLAGS "--extra-cxxflags=-g")
    list(APPEND FFmpeg_OBJCFLAGS "--extra-objcflags=-g")
    list(APPEND FFmpeg_LDFLAGS "--extra-ldflags=-g")
endif()
set(FFmpeg_CONFIGURE_ARGS
    --prefix=${CMAKE_INSTALL_PREFIX}
    --disable-programs
    --disable-doc
    --disable-avfilter
    --enable-hwaccels
    --disable-devices
    --disable-filters
    --disable-alsa
    --disable-appkit
    --disable-avfoundation
    --disable-bzlib
    --disable-coreimage
    --disable-iconv
    --disable-libxcb
    --disable-libxcb-shm
    --disable-libxcb-xfixes
    --disable-libxcb-shape
    --disable-lzma
    --disable-metal
    --disable-sndio
    --disable-schannel
    --disable-sdl2
    --disable-securetransport
    --disable-vulkan
    --disable-xlib
    --enable-zlib
    --disable-amf
    --disable-audiotoolbox
    --disable-cuda-llvm
    --disable-d3d12va
    --disable-mediafoundation
    --disable-nvenc
    --disable-v4l2-m2m
    --disable-vdpau
    --enable-pic
    ${FFmpeg_CFLAGS}
    ${FFmpeg_CXXFLAGS}
    ${FFmpeg_OBJCFLAGS}
    ${FFmpeg_LDFLAGS})
if(WIN32 OR APPLE)
    # The native APIs cover these platforms.
    list(APPEND FFmpeg_CONFIGURE_ARGS
        --disable-cuvid
        --disable-ffnvcodec
        --disable-nvdec)
else()
    # NVIDIA hardware on Linux decodes through NVDEC; VAAPI means
    # nothing to its driver (#833). The headers are built above and
    # the driver libraries load at run time.
    list(APPEND FFmpeg_CONFIGURE_ARGS
        --enable-cuvid
        --enable-ffnvcodec
        --enable-nvdec)
endif()
# What the build carries, in two parts: the codecs that need no patent
# licence, which every build has, and the ones that do, which only a build
# that is not the minimal one gets.
#
# Neither is "whatever FFmpeg ships". A blanket build claims 225 file
# extensions -- Audible audiobooks, Commodore 64 video, ANSI art -- and
# every one of them reaches the file associations and the file browser.
# What is listed is what a review tool opens.
set(FFmpeg_FREE_ARGS
    --disable-decoders
    --disable-encoders
    --disable-demuxers
    --disable-muxers
    --disable-parsers
    --disable-protocols
    # AAC is left out deliberately, and it is the codec a QuickTime movie
    # defaults to, so a minimal build writes movies without audio and cannot
    # read the ones it would otherwise have written. Opus and Vorbis below
    # are what it has instead.
    --enable-decoder=apv
    --enable-decoder=av1
    --enable-decoder=cfhd
    --enable-decoder=flac
    --enable-decoder=mjpeg
    # Royalty free by design: the Xiph, Google and AOM codecs, and the
    # BBC's VC-2. Nothing here is in a patent pool.
    --enable-decoder=vp8
    --enable-decoder=theora
    --enable-decoder=dirac
    # Lossless, and none of them encumbered: FFV1 is what archives use,
    # the others are intermediates a facility hands around.
    --enable-decoder=ffv1
    --enable-decoder=utvideo
    --enable-decoder=huffyuv
    --enable-decoder=ffvhuff
    --enable-decoder=magicyuv
    --enable-decoder=qtrle
    --enable-decoder=png
    --enable-decoder=apng
    # Audio. AAC is deliberately absent (see above), so these are what a
    # minimal build has: Opus and Vorbis for lossy, the rest lossless.
    --enable-decoder=opus
    --enable-decoder=vorbis
    --enable-decoder=speex
    --enable-decoder=alac
    --enable-decoder=wavpack
    --enable-decoder=tta
    --enable-decoder=mp3
    --enable-decoder=mpeg2video
    --enable-decoder=mpeg4
    --enable-decoder=pcm_alaw
    --enable-decoder=pcm_bluray
    --enable-decoder=pcm_dvd
    --enable-decoder=pcm_f16le
    --enable-decoder=pcm_f24le
    --enable-decoder=pcm_f32be
    --enable-decoder=pcm_f32le
    --enable-decoder=pcm_f64be
    --enable-decoder=pcm_f64le
    --enable-decoder=pcm_lxf
    --enable-decoder=pcm_mulaw
    --enable-decoder=pcm_s16be
    --enable-decoder=pcm_s16be_planar
    --enable-decoder=pcm_s16le
    --enable-decoder=pcm_s16le_planar
    --enable-decoder=pcm_s24be
    --enable-decoder=pcm_s24daud
    --enable-decoder=pcm_s24le
    --enable-decoder=pcm_s24le_planar
    --enable-decoder=pcm_s32be
    --enable-decoder=pcm_s32le
    --enable-decoder=pcm_s32le_planar
    --enable-decoder=pcm_s64be
    --enable-decoder=pcm_s64le
    --enable-decoder=pcm_s8
    --enable-decoder=pcm_s8_planar
    --enable-decoder=pcm_sga
    --enable-decoder=pcm_u16be
    --enable-decoder=pcm_u16le
    --enable-decoder=pcm_u24be
    --enable-decoder=pcm_u24le
    --enable-decoder=pcm_u32be
    --enable-decoder=pcm_u32le
    --enable-decoder=pcm_u8
    --enable-decoder=pcm_vidc
    --enable-decoder=rawvideo
    --enable-decoder=v210
    --enable-decoder=v210x
    # Not v308, v408 or v410: FFmpeg 9 dropped the codecs for the
    # uncompressed 4:4:4 QuickTime tags and maps those fourccs to
    # rawvideo, which is enabled below. Naming them here only produced
    # a configure warning.
    --enable-decoder=vp9
    --enable-decoder=yuv4
    --enable-encoder=cfhd
    --enable-encoder=flac
    --enable-encoder=mjpeg
    # The writable half of the above. There is no VP8, Theora or Dirac
    # encoder without an external library, and no AV1 one without the
    # aom or SVT builds added further down.
    --enable-encoder=vc2
    --enable-encoder=ffv1
    --enable-encoder=utvideo
    --enable-encoder=huffyuv
    --enable-encoder=ffvhuff
    --enable-encoder=magicyuv
    --enable-encoder=qtrle
    --enable-encoder=png
    --enable-encoder=apng
    --enable-encoder=opus
    --enable-encoder=vorbis
    --enable-encoder=alac
    --enable-encoder=wavpack
    --enable-encoder=tta
    --enable-encoder=mpeg2video
    --enable-encoder=mpeg4
    --enable-encoder=pcm_alaw
    --enable-encoder=pcm_bluray
    --enable-encoder=pcm_dvd
    --enable-encoder=pcm_f32be
    --enable-encoder=pcm_f32le
    --enable-encoder=pcm_f64be
    --enable-encoder=pcm_f64le
    --enable-encoder=pcm_mulaw
    --enable-encoder=pcm_s16be
    --enable-encoder=pcm_s16be_planar
    --enable-encoder=pcm_s16le
    --enable-encoder=pcm_s16le_planar
    --enable-encoder=pcm_s24be
    --enable-encoder=pcm_s24daud
    --enable-encoder=pcm_s24le
    --enable-encoder=pcm_s24le_planar
    --enable-encoder=pcm_s32be
    --enable-encoder=pcm_s32le
    --enable-encoder=pcm_s32le_planar
    --enable-encoder=pcm_s64be
    --enable-encoder=pcm_s64le
    --enable-encoder=pcm_s8
    --enable-encoder=pcm_s8_planar
    --enable-encoder=pcm_u16be
    --enable-encoder=pcm_u16le
    --enable-encoder=pcm_u24be
    --enable-encoder=pcm_u24le
    --enable-encoder=pcm_u32be
    --enable-encoder=pcm_u32le
    --enable-encoder=pcm_u8
    --enable-encoder=pcm_vidc
    --enable-encoder=rawvideo
    --enable-encoder=v210
    --enable-encoder=yuv4
    --enable-demuxer=aiff
    --enable-demuxer=apv
    --enable-demuxer=av1
    --enable-demuxer=flac
    --enable-demuxer=m4v
    --enable-demuxer=matroska
    --enable-demuxer=mjpeg
    # Also MP4: there is no mp4 demuxer, mov reads both.
    --enable-demuxer=mov
    --enable-demuxer=mp3
    --enable-demuxer=mxf
    # The containers those codecs arrive in. Ogg carries Theora, Vorbis,
    # Opus and Speex; AVI and NUT are where the lossless intermediates
    # tend to be.
    --enable-demuxer=ogg
    --enable-demuxer=avi
    --enable-demuxer=nut
    --enable-demuxer=apng
    --enable-demuxer=pcm_alaw
    --enable-demuxer=pcm_f32be
    --enable-demuxer=pcm_f32le
    --enable-demuxer=pcm_f64be
    --enable-demuxer=pcm_f64le
    --enable-demuxer=pcm_mulaw
    --enable-demuxer=pcm_s16be
    --enable-demuxer=pcm_s16le
    --enable-demuxer=pcm_s24be
    --enable-demuxer=pcm_s24le
    --enable-demuxer=pcm_s32be
    --enable-demuxer=pcm_s32le
    --enable-demuxer=pcm_s8
    --enable-demuxer=pcm_u16be
    --enable-demuxer=pcm_u16le
    --enable-demuxer=pcm_u24be
    --enable-demuxer=pcm_u24le
    --enable-demuxer=pcm_u32be
    --enable-demuxer=pcm_u32le
    --enable-demuxer=pcm_u8
    --enable-demuxer=pcm_vidc
    --enable-demuxer=rawvideo
    --enable-demuxer=v210
    --enable-demuxer=v210x
    --enable-demuxer=wav
    --enable-demuxer=yuv4mpegpipe
    --enable-muxer=aiff
    --enable-muxer=apv
    --enable-muxer=flac
    --enable-muxer=m4v
    --enable-muxer=mjpeg
    --enable-muxer=mov
    --enable-muxer=mp4
    --enable-muxer=mpeg2video
    --enable-muxer=mxf
    --enable-muxer=ogg
    --enable-muxer=avi
    --enable-muxer=nut
    --enable-muxer=webm
    --enable-muxer=apng
    --enable-muxer=pcm_alaw
    --enable-muxer=pcm_f32be
    --enable-muxer=pcm_f32le
    --enable-muxer=pcm_f64be
    --enable-muxer=pcm_f64le
    --enable-muxer=pcm_mulaw
    --enable-muxer=pcm_s16be
    --enable-muxer=pcm_s16le
    --enable-muxer=pcm_s24be
    --enable-muxer=pcm_s24le
    --enable-muxer=pcm_s32be
    --enable-muxer=pcm_s32le
    --enable-muxer=pcm_s8
    --enable-muxer=pcm_u16be
    --enable-muxer=pcm_u16le
    --enable-muxer=pcm_u24be
    --enable-muxer=pcm_u24le
    --enable-muxer=pcm_u32be
    --enable-muxer=pcm_u32le
    --enable-muxer=pcm_u8
    --enable-muxer=pcm_vidc
    --enable-muxer=rawvideo
    --enable-muxer=wav
    --enable-muxer=yuv4mpegpipe
    --enable-parser=apv
    --enable-parser=av1
    --enable-parser=flac
    --enable-parser=mjpeg
    --enable-parser=mpeg4video
    --enable-parser=mpegaudio
    --enable-parser=mpegvideo
    --enable-parser=vp9
    --enable-parser=vp8
    --enable-parser=opus
    --enable-parser=vorbis
    --enable-parser=dirac
    --enable-parser=png
    --enable-protocol=file
    # For reading a byte range of a file in place -- media stored in
    # an OTIOZ bundle. A protocol, not a codec, so it carries no
    # licensing weight.
    --enable-protocol=subfile)

# Codecs that need a patent licence. DJV Studio and a build from source have
# them; the packages do not. Adding a decoder here also adds whatever file
# extensions its demuxer claims, so the containers below are the ones a
# review tool is handed, not everything that could carry the codec.
set(FFmpeg_LICENSED_ARGS
    --enable-decoder=h264
    --enable-decoder=hevc
    --enable-decoder=vc1
    --enable-decoder=prores
    --enable-decoder=prores_raw
    --enable-decoder=dnxhd
    --enable-decoder=dvvideo
    --enable-decoder=jpeg2000
    --enable-decoder=mjpegb
    --enable-decoder=mpeg1video
    --enable-decoder=aac
    --enable-decoder=ac3
    --enable-decoder=eac3
    --enable-decoder=mp2
    --enable-decoder=dca
    --enable-decoder=truehd
    # No software h264 or hevc encoder: those need x264 and x265, which are
    # GPL. Writing them is VideoToolbox's job on the platforms that have it.
    --enable-encoder=prores
    --enable-encoder=prores_ks
    --enable-encoder=prores_aw
    --enable-encoder=dnxhd
    --enable-encoder=dvvideo
    --enable-encoder=jpeg2000
    --enable-encoder=mpeg1video
    --enable-encoder=aac
    --enable-encoder=ac3
    --enable-encoder=eac3
    --enable-encoder=mp2
    --enable-demuxer=mpegts
    --enable-demuxer=mpegps
    --enable-demuxer=dv
    --enable-demuxer=h264
    --enable-demuxer=hevc
    --enable-demuxer=aac
    --enable-demuxer=ac3
    --enable-demuxer=eac3
    --enable-demuxer=dts
    --enable-muxer=mpegts
    --enable-muxer=dv
    --enable-muxer=ac3
    --enable-muxer=eac3
    --enable-parser=h264
    --enable-parser=hevc
    --enable-parser=vc1
    --enable-parser=aac
    --enable-parser=ac3
    --enable-parser=dca
    --enable-parser=jpeg2000
    --enable-parser=dvaudio)

list(APPEND FFmpeg_CONFIGURE_ARGS ${FFmpeg_FREE_ARGS})
if(TLRENDER_FFMPEG_MINIMAL)
    # The blanket --enable-hwaccels above cannot survive here: enabling a
    # hardware decoder pulls in the software decoder it depends on, which
    # quietly put h264, hevc, and prores back into the minimal build. No
    # hardware decoding in the minimal packages is the trade.
    list(APPEND FFmpeg_CONFIGURE_ARGS --disable-hwaccels)
else()
    list(APPEND FFmpeg_CONFIGURE_ARGS ${FFmpeg_LICENSED_ARGS})
endif()
if(TLRENDER_AOM)
    list(APPEND FFmpeg_CONFIGURE_ARGS
        --enable-libaom
        --enable-decoder=libaom_av1)
endif()
if(TLRENDER_SVTAV1)
    list(APPEND FFmpeg_CONFIGURE_ARGS
        --enable-libsvtav1
        --enable-encoder=libsvtav1)
endif()
if(TLRENDER_NASM)
    list(APPEND FFmpeg_CONFIGURE_ARGS
        --x86asmexe=${CMAKE_INSTALL_PREFIX}/bin/nasm)
endif()
if(FFmpeg_SHARED_LIBS)
    list(APPEND FFmpeg_CONFIGURE_ARGS
        --disable-static
        --enable-shared)
endif()
if(FFmpeg_DEBUG)
    list(APPEND FFmpeg_CONFIGURE_ARGS
        --disable-optimizations
        --disable-stripping
        --enable-debug=3
        --assert-level=2)
endif()

include(ProcessorCount)
ProcessorCount(FFmpeg_BUILD_JOBS)
if(WIN32)
    list(APPEND FFmpeg_CONFIGURE_ARGS
        --arch=x86_64
        --toolchain=msvc)
    set(FFmpeg_MSYS2
        ${MSYS_CMD}
        -use-full-path
        -defterm
        -no-start
        -here)

    set(FFmpeg_PKG_CONFIG ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)
    # \bug The colon in "C:" seems to be replaced with a space,
    # so replace "C:" with "/C".
    #string(REPLACE C: /C FFmpeg_PKG_CONFIG ${FFmpeg_PKG_CONFIG})
    string(SUBSTRING ${FFmpeg_PKG_CONFIG} 0 2 FFmpeg_PKG_CONFIG_DRIVE)
    string(SUBSTRING ${FFmpeg_PKG_CONFIG} 0 1 FFmpeg_PKG_CONFIG_DRIVE_LETTER)
    string(REPLACE ${FFmpeg_PKG_CONFIG_DRIVE} /${FFmpeg_PKG_CONFIG_DRIVE_LETTER} FFmpeg_PKG_CONFIG ${FFmpeg_PKG_CONFIG})

    list(JOIN FFmpeg_CONFIGURE_ARGS " " FFmpeg_CONFIGURE_ARGS_TMP)
    # pkgconf because libaom and libsvtav1 are the two dependencies FFmpeg
    # will only find through pkg-config; the rest are found here by the
    # include and library paths passed above. Without it PKG_CONFIG_PATH is
    # exported into a shell that has nothing to read it, and --enable-libaom
    # fails however well the libraries themselves were built.
    set(FFmpeg_CONFIGURE ${FFmpeg_MSYS2}
        -c "pacman -S diffutils make nasm pkgconf --noconfirm && \
        export PKG_CONFIG_PATH=${FFmpeg_PKG_CONFIG} && \
        ./configure ${FFmpeg_CONFIGURE_ARGS_TMP}")
    set(FFmpeg_BUILD ${FFmpeg_MSYS2} -c "make -j${FFmpeg_BUILD_JOBS}")
    set(FFmpeg_INSTALL ${FFmpeg_MSYS2} -c "make install"
        COMMAND ${FFmpeg_MSYS2} -c "mv ${CMAKE_INSTALL_PREFIX}/bin/avcodec.lib ${CMAKE_INSTALL_PREFIX}/lib"
        COMMAND ${FFmpeg_MSYS2} -c "mv ${CMAKE_INSTALL_PREFIX}/bin/avdevice.lib ${CMAKE_INSTALL_PREFIX}/lib"
        COMMAND ${FFmpeg_MSYS2} -c "mv ${CMAKE_INSTALL_PREFIX}/bin/avformat.lib ${CMAKE_INSTALL_PREFIX}/lib"
        COMMAND ${FFmpeg_MSYS2} -c "mv ${CMAKE_INSTALL_PREFIX}/bin/avutil.lib ${CMAKE_INSTALL_PREFIX}/lib"
        COMMAND ${FFmpeg_MSYS2} -c "mv ${CMAKE_INSTALL_PREFIX}/bin/swresample.lib ${CMAKE_INSTALL_PREFIX}/lib"
        COMMAND ${FFmpeg_MSYS2} -c "mv ${CMAKE_INSTALL_PREFIX}/bin/swscale.lib ${CMAKE_INSTALL_PREFIX}/lib")
else()
    # -lpthread because the configure check for SVT-AV1 links its static
    # library, whose pkg-config file lists no private libraries, and on glibc
    # before 2.34 the pthread and semaphore symbols it needs are not in libc.
    # These go in extra-libs rather than extra-ldflags so that they come last
    # on the link line, where static libraries can resolve against them.
    list(APPEND FFmpeg_CONFIGURE_ARGS --extra-libs=-lm --extra-libs=-lpthread)

    set(FFmpeg_CONFIGURE
        ${CMAKE_COMMAND} -E env PKG_CONFIG_PATH=${CMAKE_INSTALL_PREFIX}/lib/pkgconfig
        ./configure ${FFmpeg_CONFIGURE_ARGS})
    if(BSD)
        set(FFmpeg_BUILD gmake)
        set(FFmpeg_INSTALL gmake install)
    else()
        set(FFmpeg_BUILD make -j${FFmpeg_BUILD_JOBS})
        set(FFmpeg_INSTALL make install)
    endif()
    if(APPLE)
        list(APPEND FFmpeg_INSTALL
            COMMAND install_name_tool -id @rpath/libavcodec.63.1.101.dylib ${CMAKE_INSTALL_PREFIX}/lib/libavcodec.63.dylib
            COMMAND install_name_tool -id @rpath/libavdevice.63.1.101.dylib ${CMAKE_INSTALL_PREFIX}/lib/libavdevice.63.dylib
            COMMAND install_name_tool -id @rpath/libavformat.63.1.101.dylib ${CMAKE_INSTALL_PREFIX}/lib/libavformat.63.dylib
            COMMAND install_name_tool -id @rpath/libavutil.61.1.101.dylib ${CMAKE_INSTALL_PREFIX}/lib/libavutil.61.dylib
            COMMAND install_name_tool -id @rpath/libswresample.7.1.101.dylib ${CMAKE_INSTALL_PREFIX}/lib/libswresample.7.dylib
            COMMAND install_name_tool -id @rpath/libswscale.10.1.101.dylib ${CMAKE_INSTALL_PREFIX}/lib/libswscale.10.dylib
            COMMAND install_name_tool
                -change ${CMAKE_INSTALL_PREFIX}/lib/libswresample.7.dylib @rpath/libswresample.7.dylib
                -change ${CMAKE_INSTALL_PREFIX}/lib/libavutil.61.dylib @rpath/libavutil.61.dylib
                ${CMAKE_INSTALL_PREFIX}/lib/libavcodec.63.1.101.dylib
            COMMAND install_name_tool
                -change ${CMAKE_INSTALL_PREFIX}/lib/libswscale.10.dylib @rpath/libswscale.10.dylib
                -change ${CMAKE_INSTALL_PREFIX}/lib/libavformat.63.dylib @rpath/libavformat.63.dylib
                -change ${CMAKE_INSTALL_PREFIX}/lib/libavcodec.63.dylib @rpath/libavcodec.63.dylib
                -change ${CMAKE_INSTALL_PREFIX}/lib/libswresample.7.dylib @rpath/libswresample.7.dylib
                -change ${CMAKE_INSTALL_PREFIX}/lib/libavutil.61.dylib @rpath/libavutil.61.dylib
                ${CMAKE_INSTALL_PREFIX}/lib/libavdevice.63.1.101.dylib
            COMMAND install_name_tool
                -change ${CMAKE_INSTALL_PREFIX}/lib/libavcodec.63.dylib @rpath/libavcodec.63.dylib
                -change ${CMAKE_INSTALL_PREFIX}/lib/libswresample.7.dylib @rpath/libswresample.7.dylib
                -change ${CMAKE_INSTALL_PREFIX}/lib/libavutil.61.dylib @rpath/libavutil.61.dylib
                ${CMAKE_INSTALL_PREFIX}/lib/libavformat.63.1.101.dylib
            COMMAND install_name_tool
                -change ${CMAKE_INSTALL_PREFIX}/lib/libavutil.61.dylib @rpath/libavutil.61.dylib
                ${CMAKE_INSTALL_PREFIX}/lib/libswresample.7.1.101.dylib
            COMMAND install_name_tool
                -change ${CMAKE_INSTALL_PREFIX}/lib/libavutil.61.dylib @rpath/libavutil.61.dylib
                ${CMAKE_INSTALL_PREFIX}/lib/libswscale.10.1.101.dylib)
    endif()
endif()

# The subfile protocol clips a seek to thirty-two bits, so a bundled movie
# with its moov atom past 2 GB does not open through it (FFmpeg-patch/
# subfile.patch, sent upstream; drop it once a release carries the fix).
find_package(Git REQUIRED)

ExternalProject_Add(
    FFmpeg
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/FFmpeg
    DEPENDS ${FFmpeg_DEPS}
    URL ${FFmpeg_URL}
    PATCH_COMMAND ${CMAKE_COMMAND}
        -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
        -DPATCH_SOURCE_DIR=${CMAKE_CURRENT_BINARY_DIR}/FFmpeg/src/FFmpeg
        -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/FFmpeg-patch/subfile.patch
        -P ${CMAKE_CURRENT_LIST_DIR}/ApplyPatch.cmake
    CONFIGURE_COMMAND ${FFmpeg_CONFIGURE}
    BUILD_COMMAND ${FFmpeg_BUILD}
    INSTALL_COMMAND ${FFmpeg_INSTALL}
    BUILD_IN_SOURCE 1)
