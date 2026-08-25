set(HEADERS)
set(SOURCE)

# The icons go through ftk_icon_resources(): adding an icon is adding a
# file to etc/Icons, and init() registers the generated map.
ftk_icon_resources(SOURCE ${PROJECT_SOURCE_DIR}/etc/Icons tl_resource)

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

# The generated headers are found here; the build interface only, since
# the resources are an implementation detail rather than installed API.
target_include_directories(tlResource PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
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
