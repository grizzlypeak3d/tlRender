set(HEADERS)
set(SOURCE)
set(RESOURCES
    etc/Icons/ColorControls.svg
    etc/Icons/ColorPicker.svg
    etc/Icons/CompareA.svg
    etc/Icons/CompareB.svg
    etc/Icons/CompareButterfly.svg
    etc/Icons/CompareDifference.svg
    etc/Icons/CompareHorizontal.svg
    etc/Icons/CompareOverlay.svg
    etc/Icons/CompareTile.svg
    etc/Icons/CompareVertical.svg
    etc/Icons/CompareWipe.svg
    etc/Icons/Devices.svg
    etc/Icons/Export.svg
    etc/Icons/FileOpenAudio.svg
    etc/Icons/Files.svg
    etc/Icons/Hidden.svg
    etc/Icons/Info.svg
    etc/Icons/Magnify.svg
    etc/Icons/Messages.svg
    etc/Icons/PlaybackLoop.svg
    etc/Icons/PlaybackOnce.svg
    etc/Icons/PlaybackPingPong.svg
    etc/Icons/View.svg
    etc/Icons/Visible.svg
    etc/Icons/WindowSecondary.svg
    etc/Icons/tlRender.svg)
foreach(RESOURCE ${RESOURCES})
    get_filename_component(RESOURCE_BASE ${RESOURCE} NAME_WE)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${RESOURCE_BASE}.cpp
        COMMAND ftk-resource ${PROJECT_SOURCE_DIR}/${RESOURCE} ${RESOURCE_BASE} tl_resource
        DEPENDS ${PROJECT_SOURCE_DIR}/${RESOURCE})
    list(APPEND SOURCE ${CMAKE_CURRENT_BINARY_DIR}/${RESOURCE_BASE}.cpp)
endforeach()

# Static, whatever the rest of the build is. The resources are data at
# namespace scope with no export macro on them, so a shared build on Windows
# produces a library that exports nothing and no import library beside it:
#
#     LINK : fatal error LNK1104: cannot open file 'tlResource.lib'
#
# CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS does not answer this the way it does for
# the dependencies: it writes a module definition file from the functions the
# objects hold, and these hold none. Marking the data instead would mean the
# generator and every extern declaration that names it.
#
# Static suits them anyway. They are an implementation detail rather than API,
# and each resource is its own object file, so a consumer links only the ones
# it names -- its consumers take the ones they use and nothing more.
add_library(tlResource STATIC ${HEADERS} ${SOURCE})
set_target_properties(tlResource PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(tlResource ZLIB::ZLIB)

set_target_properties(tlResource PROPERTIES FOLDER lib)
set_target_properties(tlResource PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(
    TARGETS tlResource
    EXPORT tlResourceTargets
    ARCHIVE DESTINATION lib COMPONENT dev
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/tlRender/Resource COMPONENT dev)
install(
    EXPORT tlResourceTargets
    FILE tlResourceTargets.cmake
    DESTINATION "lib/cmake/tlRender"
    NAMESPACE tlRender::
    COMPONENT dev)
