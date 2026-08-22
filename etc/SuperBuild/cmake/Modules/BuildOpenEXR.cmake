include(ExternalProject)

set(OpenEXR_GIT_REPOSITORY "https://github.com/AcademySoftwareFoundation/openexr.git")
set(OpenEXR_GIT_TAG "v3.4.15")

set(OpenEXR_ARGS
    ${TLRENDER_EXTERNAL_ARGS}
    -DOPENEXR_BUILD_TOOLS=OFF
    -DOPENEXR_BUILD_EXAMPLES=OFF
    -DBUILD_TESTING=OFF
    -DOPENEXR_FORCE_INTERNAL_DEFLATE=ON
    # OFF is the default, said out loud because it is what makes OpenEXR
    # look for the OpenJPH built above rather than quietly building its
    # own vendored copy.
    -DOPENEXR_FORCE_INTERNAL_OPENJPH=OFF)

ExternalProject_Add(
    OpenEXR
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/OpenEXR
    DEPENDS Imath OpenJPH
    GIT_REPOSITORY ${OpenEXR_GIT_REPOSITORY}
    GIT_TAG ${OpenEXR_GIT_TAG}
    LIST_SEPARATOR |
    CMAKE_ARGS ${OpenEXR_ARGS})
