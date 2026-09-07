set(LEGAL
    LICENSE_aom.txt
    LICENSE_CMake.txt
    LICENSE_coi-serviceworker.txt
    LICENSE_expat.txt
    LICENSE_FFmpeg.txt
    LICENSE_feather-tk.txt
    LICENSE_LibRaw.txt
    LICENSE_libjpeg-turbo.txt
    LICENSE_libjpeg.txt
    LICENSE_libpng.txt
    LICENSE_libtiff.txt
    LICENSE_minizip-ng.txt
    LICENSE_mp4box.txt
    LICENSE_nlohmann_json.txt
    LICENSE_nv-codec-headers.txt
    LICENSE_OFL.txt
    LICENSE_OpenColorIO.txt
    LICENSE_OpenEXR.txt
    LICENSE_OpenImageIO.md
    LICENSE_OpenJPH.txt
    LICENSE_OpenTimelineIO.txt
    LICENSE_pystring.txt
    LICENSE_subprocess.txt
    LICENSE_svt-av1.txt
    LICENSE_tlRender.txt
    LICENSE_yaml-cpp.txt
    LICENSE_zlib.txt)

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

install(
    FILES ${LEGAL}
    DESTINATION share/tlRender/Legal)
