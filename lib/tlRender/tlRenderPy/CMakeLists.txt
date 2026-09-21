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

# Type stubs: the signatures of a compiled module in a form editors and
# type checkers can read, since there is no Python source in it for them
# to parse. Written by nanobind from the bindings themselves, which imports
# the module that was just built -- through etc/Python/make_stub.py, which
# names the directories its libraries are in first, since on Windows they
# are not beside it and only os.add_dll_directory() finds them. Recursive,
# for the ui submodule: the stubs are a _tlrender directory beside the
# module. py.typed is what says they are to be trusted.
if(EXISTS ${NB_DIR}/src/stubgen.py)
    set(TLRENDER_STUBGEN ${NB_DIR}/src/stubgen.py)
else()
    set(TLRENDER_STUBGEN ${NB_DIR}/stubgen.py)
endif()
set(TLRENDER_PYTHON_STUB ${TLRENDER_PYTHON_PACKAGE_DIR}/_tlrender/__init__.pyi)
add_custom_command(
    OUTPUT ${TLRENDER_PYTHON_STUB}
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${TLRENDER_PYTHON_PACKAGE_DIR}/_tlrender
    COMMAND ${Python_EXECUTABLE}
        ${PROJECT_SOURCE_DIR}/etc/Python/make_stub.py
        --dll-dir $<TARGET_FILE_DIR:tlCore>
        --dll-dir $<TARGET_FILE_DIR:tlGL>
        --dll-dir $<TARGET_FILE_DIR:tlIO>
        --dll-dir $<TARGET_FILE_DIR:tlTimeline>
        --dll-dir $<TARGET_FILE_DIR:tlUI>
        # feather-tk's libraries too: built here as a sub-project, or in
        # the feather_tk package in a wheel build. The namespaced names
        # are both.
        --dll-dir $<TARGET_FILE_DIR:ftk::ftkCore>
        --dll-dir $<TARGET_FILE_DIR:ftk::ftkGL>
        --dll-dir $<TARGET_FILE_DIR:ftk::ftkUI>
        --dll-dir ${CMAKE_INSTALL_PREFIX}/bin
        --dll-dir ${CMAKE_INSTALL_PREFIX}/lib
        # The dependencies of a wheel build, where OpenTimelineIO's
        # libraries are in lib rather than bin.
        --dll-dir ${TLRENDER_FFMPEG_PREFIX}/bin
        --dll-dir ${TLRENDER_FFMPEG_PREFIX}/lib
        --
        ${TLRENDER_STUBGEN}
        -q
        -r
        -m tlrender._tlrender
        -i ${CMAKE_BINARY_DIR}/python
        # OpenTimelineIO, which the module imports: the super build
        # installs its Python package here, and a wheel build has it
        # in the build environment instead.
        -i ${CMAKE_INSTALL_PREFIX}/python
        -O ${TLRENDER_PYTHON_PACKAGE_DIR}
    # feather-tk's module as well where it is built here rather than
    # installed: importing this one imports that one, which has to exist
    # by then.
    DEPENDS
        _tlrender
        $<TARGET_NAME_IF_EXISTS:_feather_tk>
        ${TLRENDER_STUBGEN}
        ${PROJECT_SOURCE_DIR}/etc/Python/make_stub.py
    COMMENT "Generating the tlrender type stubs")
add_custom_target(_tlrender_stub ALL DEPENDS ${TLRENDER_PYTHON_STUB})
file(TOUCH ${TLRENDER_PYTHON_PACKAGE_DIR}/py.typed)

# What the stubs could not resolve is a C++ type the bindings never
# exposed; the build says so rather than leaving it in the files to be
# found. See etc/Python/check_stub.py.
add_custom_command(
    TARGET _tlrender_stub POST_BUILD
    COMMAND ${Python_EXECUTABLE}
        ${PROJECT_SOURCE_DIR}/etc/Python/check_stub.py
        ${TLRENDER_PYTHON_PACKAGE_DIR}/_tlrender/__init__.pyi
        ${TLRENDER_PYTHON_PACKAGE_DIR}/_tlrender/ui.pyi)

install(TARGETS _tlrender
    LIBRARY DESTINATION ${TLRENDER_PYTHON_INSTALL_DIR}
    RUNTIME DESTINATION ${TLRENDER_PYTHON_INSTALL_DIR})
install(
    FILES
        __init__.py
        ${TLRENDER_PYTHON_PACKAGE_DIR}/py.typed
    DESTINATION ${TLRENDER_PYTHON_INSTALL_DIR})
install(
    DIRECTORY ${TLRENDER_PYTHON_PACKAGE_DIR}/_tlrender
    DESTINATION ${TLRENDER_PYTHON_INSTALL_DIR})
