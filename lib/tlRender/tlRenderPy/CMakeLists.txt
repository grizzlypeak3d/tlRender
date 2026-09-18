# The module is private to the tlrender package, whose __init__.py gathers
# its API; the build tree lays the package out the way an install does, so
# PYTHONPATH can name either.
set(TLRENDER_PYTHON_PACKAGE_DIR ${CMAKE_BINARY_DIR}/python/tlrender)

set(STABLE_ABI)
if(ftk_PYTHON_STABLE_ABI)
    set(STABLE_ABI STABLE_ABI)
endif()
nanobind_add_module(_tlrender ${STABLE_ABI} NB_STATIC Bindings.cpp)

target_include_directories(_tlrender
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
        $<INSTALL_INTERFACE:include>)

# The binding objects go into the module directly rather than through
# their archives. nanobind's own static library is first on the link line,
# and GNU ld takes from an archive only what is wanted at the moment it is
# scanned: a binding archive scanned after it finds the enum and datetime
# helpers already passed over, and a Release build fails to link.
target_sources(_tlrender PRIVATE $<TARGET_OBJECTS:tlUIPy> $<TARGET_OBJECTS:tlTimelinePy> $<TARGET_OBJECTS:tlIOPy> $<TARGET_OBJECTS:tlCorePy>)
target_link_libraries(_tlrender PRIVATE tlUIPy tlTimelinePy tlIOPy tlCorePy)

# The generator expression keeps multi-config generators from adding a
# directory per configuration, which would leave the module outside the
# package.
set_target_properties(_tlrender PROPERTIES
    FOLDER lib
    LIBRARY_OUTPUT_DIRECTORY $<1:${TLRENDER_PYTHON_PACKAGE_DIR}>)
configure_file(__init__.py ${TLRENDER_PYTHON_PACKAGE_DIR}/__init__.py COPYONLY)

# The rpath convention of the executables names the interpreter's
# directory here, since the "executable" loading a Python module is
# Python itself: the module finds the libraries from where it is instead.
# That is <prefix>/lib from <prefix>/lib/tlrender, and lib inside the
# package in a wheel -- where feather-tk's libraries are in the feather_tk
# package beside it, found by TLRENDER_PYTHON_FTK_RPATH.
file(RELATIVE_PATH TLRENDER_PYTHON_LIB_RPATH
    /prefix/${TLRENDER_PYTHON_INSTALL_DIR} /prefix/lib)
set(RPATH_ORIGIN)
if(APPLE)
    set(RPATH_ORIGIN "@loader_path")
elseif(UNIX)
    set(RPATH_ORIGIN "$ORIGIN")
endif()
if(RPATH_ORIGIN)
    set(RPATHS "${RPATH_ORIGIN}/${TLRENDER_PYTHON_LIB_RPATH}")
    if(TLRENDER_PYTHON_FTK_RPATH)
        list(APPEND RPATHS "${RPATH_ORIGIN}/${TLRENDER_PYTHON_FTK_RPATH}")
    endif()
    set_target_properties(_tlrender PROPERTIES INSTALL_RPATH "${RPATHS}")
endif()

install(TARGETS _tlrender
    LIBRARY DESTINATION ${TLRENDER_PYTHON_INSTALL_DIR}
    RUNTIME DESTINATION ${TLRENDER_PYTHON_INSTALL_DIR})
install(
    FILES __init__.py
    DESTINATION ${TLRENDER_PYTHON_INSTALL_DIR})
