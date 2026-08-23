set(HEADERS
    App.h
    CompareActions.h
    FileActions.h
    FilesModel.h
    MainWindow.h
    MenuBar.h
    PlaybackActions.h
    PlaybackBar.h
    SettingsModel.h
    SettingsWidget.h
    StatusBar.h
    TabBar.h
    ToolBars.h
    ViewActions.h
    WindowActions.h)

set(SOURCE
    App.cpp
    CompareActions.cpp
    FileActions.cpp
    FilesModel.cpp
    MainWindow.cpp
    MenuBar.cpp
    PlaybackActions.cpp
    PlaybackBar.cpp
    SettingsModel.cpp
    SettingsWidget.cpp
    StatusBar.cpp
    TabBar.cpp
    ToolBars.cpp
    ViewActions.cpp
    WindowActions.cpp)

set(LIBRARIES tlUI)

# Static. This is one program's code kept in a library so the tests can reach
# it, linked by tlplay and nothing else, and it carries no export macros. A
# shared build on Windows would give it a library that exports nothing and no
# import library to link against:
#
#     LINK : fatal error LNK1104: cannot open file 'tlPlayApp.lib'
#
# There is no third party to import this, so there is nothing to mark.
add_library(tlPlayApp STATIC ${HEADERS} ${SOURCE})
set_target_properties(tlPlayApp PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(tlPlayApp
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tlPlayApp ${LIBRARIES})

set_target_properties(tlPlayApp PROPERTIES FOLDER lib)
set_target_properties(tlPlayApp PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(TARGETS tlPlayApp
    ARCHIVE DESTINATION lib COMPONENT dev
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/tlRender/PlayApp COMPONENT dev)
