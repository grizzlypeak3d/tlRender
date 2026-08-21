# What continuous integration builds everywhere: the default configuration,
# which is no Python bindings and static libraries. The -python
# configurations beside this one are the other half, and each platform is
# built both ways.
include("${CMAKE_CURRENT_LIST_DIR}/local.cmake" OPTIONAL)

# The command line FFmpeg plugin is in the platform configurations that do
# not run tests, not here: it registers ahead of the FFmpeg plugin and takes
# the movie reads, so a tested build would no longer be testing the library.
include("${CMAKE_CURRENT_LIST_DIR}/default.cmake")
