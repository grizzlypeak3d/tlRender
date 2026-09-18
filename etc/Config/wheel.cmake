# Python wheels: the tlrender package with the libraries it loads, and the
# headers and CMake package to build against them.
#
# Read by the main project when scikit-build-core runs it (see
# pyproject.toml), before the options, the way "cmake -C" would be. There is
# no local.cmake here: a wheel should build the same on every machine.

set(TLRENDER_PYTHON ON CACHE BOOL "")
set(ftk_PYTHON_STABLE_ABI ON CACHE BOOL "")
set(TLRENDER_PROGRAMS OFF CACHE BOOL "")
set(TLRENDER_EXAMPLES OFF CACHE BOOL "")
set(TLRENDER_TESTS OFF CACHE BOOL "")
set(TLRENDER_USD OFF CACHE BOOL "")

# feather-tk is the feather-tk wheel, installed beside this one: its
# libraries are loaded from there rather than carried again, so that one
# process has one of each. The package is the wheel's install directory
# (wheel.install-dir), so the module goes at its top and the libraries in
# lib beside it, and both name the feather_tk package next door.
set(TLRENDER_FTK_PACKAGE ON CACHE BOOL "")
set(TLRENDER_PYTHON_INSTALL_DIR "." CACHE STRING "")
set(TLRENDER_PYTHON_FTK_RPATH "../feather_tk/lib" CACHE STRING "")
set(TLRENDER_INSTALL_RPATH_EXTRA "../../feather_tk/lib" CACHE STRING "")

# A package: FFmpeg with only the codecs that can be shipped without a
# patent license, as DJV's package.cmake explains. The super build reads it.
set(TLRENDER_FFMPEG_MINIMAL ON CACHE BOOL "")

# Shared, for the reason default.cmake gives.
set(BUILD_SHARED_LIBS ON CACHE BOOL "")

# Homebrew is /opt/homebrew on Apple silicon and /usr/local on Intel, and
# only the first is ignored by default. An Intel runner has a Homebrew
# libjpeg, which OpenImageIO found in place of the one built here; the
# wheel then carried a library built for macOS 14 and the repair tool
# refused it against the 10.15 the wheel is tagged for.
if(APPLE)
    set(TLRENDER_IGNORE_PREFIX_PATH "/opt/homebrew;/usr/local" CACHE STRING "")
endif()

# libGL rather than the GLVND libraries FindOpenGL prefers: libGL is among the
# libraries a manylinux wheel may take from the system, and libOpenGL is not.
set(OpenGL_GL_PREFERENCE LEGACY CACHE STRING "")
