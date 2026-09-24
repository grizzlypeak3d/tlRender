# The notices for what tlRender itself brings into a build: its own license
# and those of the components it builds. Each notice lives in one repository
# only -- the one whose build ships the component -- so feather-tk's own
# (zlib, libpng, FreeType, the fonts, ...) are feather-tk's to keep, and a
# project that builds on this one takes these from here rather than keeping
# copies.
set(LEGAL
    LICENSE_aom.txt
    LICENSE_coi-serviceworker.txt
    LICENSE_expat.txt
    LICENSE_FFmpeg.txt
    LICENSE_LibRaw.txt
    LICENSE_libjpeg-turbo.txt
    LICENSE_libjpeg.txt
    LICENSE_libtiff.txt
    LICENSE_minizip-ng.txt
    LICENSE_mp4box.txt
    LICENSE_openapv.txt
    LICENSE_OpenColorIO.txt
    LICENSE_OpenEXR.txt
    LICENSE_OpenImageIO.md
    LICENSE_OpenJPH.txt
    LICENSE_OpenTimelineIO.txt
    LICENSE_pystring.txt
    LICENSE_subprocess.txt
    LICENSE_svt-av1.txt
    LICENSE_yaml-cpp.txt)

# NVIDIA's headers are built for FFmpeg on Linux only, where NVDEC is how
# NVIDIA hardware decodes (etc/SuperBuild/cmake/Modules/BuildFFmpeg.cmake).
if(NOT WIN32 AND NOT APPLE)
    list(APPEND LEGAL LICENSE_nv-codec-headers.txt)
endif()

# The USD notices ride only when USD does: what ships needs a
# notice, and only what ships.
if(TLRENDER_USD)
    list(APPEND LEGAL
        LICENSE_boost.txt
        LICENSE_MaterialX.txt
        LICENSE_OpenSubdiv.txt
        LICENSE_OpenUSD.txt
        LICENSE_oneTBB.txt)
endif()

# Installed beside the package, which is where a project that finds tlRender
# as a package reads them from (tlRender_LEGAL_DIR in tlRenderConfig.cmake).
# The project's own license is LICENSE.txt at the top of the repository,
# installed under the name the notices are known by.
install(
    FILES ${LEGAL}
    DESTINATION share/tlRender/Legal)
install(
    FILES ${PROJECT_SOURCE_DIR}/LICENSE.txt
    DESTINATION share/tlRender/Legal
    RENAME LICENSE_tlRender.txt)

# And for a project that builds tlRender as a subdirectory, the same list as
# full paths, collected in a global property beside feather-tk's: "Name=path"
# for the license whose file name does not say what it is, a path for the
# rest. DJV writes its licenses page from this (docs/Licenses.cmake).
set(ENTRIES "tlRender=${PROJECT_SOURCE_DIR}/LICENSE.txt")
foreach(FILE ${LEGAL})
    list(APPEND ENTRIES "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}")
endforeach()
set_property(GLOBAL APPEND PROPERTY LEGAL_NOTICES "${ENTRIES}")
