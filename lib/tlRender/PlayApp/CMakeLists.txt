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

add_library(tlPlayApp ${HEADERS} ${SOURCE})

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
