include(ExternalProject)

set(pystring_GIT_REPOSITORY "https://github.com/imageworks/pystring.git")
set(pystring_GIT_TAG "v1.1.4")

# A shared build on Windows needs something to export. This library carries no
# __declspec(dllexport) of its own, so it builds a DLL that exports nothing and
# the linker writes no import library beside it; the first thing to ask for one
# stops:
#
#     LINK : fatal error LNK1104: cannot open file 'Debug\pystring.lib'
#
# CMake writes a module definition file listing what the objects hold instead.
# Set here rather than for every dependency: the ones that carry export macros
# already do not need it, and turning it on for them breaks the ones with
# assembly, where the generator cannot read the objects:
#
#     unrecognized file format in '.../intrapred_asm_sse2.obj'
#
# It covers functions and not data, so if a global ever has to be exported this
# has to become a static build instead. Ignored everywhere but Windows.
set(pystring_ARGS
    ${TLRENDER_EXTERNAL_ARGS}
    -DCMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=${BUILD_SHARED_LIBS}
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5)

ExternalProject_Add(
    pystring
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/pystring
    GIT_REPOSITORY ${pystring_GIT_REPOSITORY}
    GIT_TAG ${pystring_GIT_TAG}
    PATCH_COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/pystring-patch/CMakeLists.txt
        ${CMAKE_CURRENT_BINARY_DIR}/pystring/src/pystring/CMakeLists.txt
    LIST_SEPARATOR |
    CMAKE_ARGS ${pystring_ARGS})
