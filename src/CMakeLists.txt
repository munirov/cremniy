cmake_minimum_required(VERSION 3.16)

project(cremniy VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8.2 REQUIRED COMPONENTS Widgets Test)

set(PROJECT_SOURCES
        main.cpp
)

qt_add_executable(${PROJECT_NAME}
    MANUAL_FINALIZATION
    main.cpp

    # App
    app/IDEWindow/idewindow.cpp
    app/IDEWindow/idewindow.h
    app/WelcomeWindow/welcomeform.cpp
    app/WelcomeWindow/welcomeform.h

    # Dialogs
    dialogs/filecreatedialog.cpp
    dialogs/filecreatedialog.h
    dialogs/settingsdialog.cpp
    dialogs/settingsdialog.h
    dialogs/keyboardscancodesdialog.cpp
    dialogs/keyboardscancodesdialog.h

    # Widgets
    widgets/keyboardscancodevizwidget.cpp
    widgets/keyboardscancodevizwidget.h
    widgets/clickablelineedit.h
    widgets/filetab.cpp
    widgets/filetab.h
    widgets/filetreeview.cpp
    widgets/filetreeview.h
    widgets/terminal/terminalwidget.cpp
    widgets/terminal/terminalwidget.h

    # Resources
    resources/cremniy_res.qrc

    # Utils
    utils/utils.h
    utils/utils.cpp

    # Core
    core/file/filecontext.h
    core/file/FileDataBuffer.cpp
    core/file/FileDataBuffer.h
    core/file/filemanager.cpp
    core/file/filemanager.h
    core/project/projectshistorymanager.cpp
    core/project/projectshistorymanager.h
    core/settings/appsettings.cpp
    core/settings/appsettings.h

    # UI
    ui/FilesTabWidget/filestabwidget.cpp
    ui/FilesTabWidget/filestabwidget.h
    ui/ToolsTabWidget/toolstabwidget.cpp
    ui/ToolsTabWidget/toolstabwidget.h
    ui/ToolsTabWidget/ToolTab.h
    ui/ToolsTabWidget/ToolTabFactory.cpp
    ui/ToolsTabWidget/ToolTabFactory.h

    # UI Menubar
    ui/MenuBar/menubarbuilder.h ui/MenuBar/menubarbuilder.cpp
    ui/MenuBar/menufactory.h ui/MenuBar/menufactory.cpp
    ui/MenuBar/basemenu.h

)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/core/file
    ${CMAKE_CURRENT_SOURCE_DIR}/core/project
    ${CMAKE_CURRENT_SOURCE_DIR}/core/settings
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/ToolsTabWidget
    ${CMAKE_CURRENT_SOURCE_DIR}/FilesTabWidget
)


# - - Libs - -
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/CodeEditor/ ${CMAKE_BINARY_DIR}/libs/CodeEditor/)
target_link_libraries(${PROJECT_NAME} PRIVATE CodeEditor)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/HexEditor/ ${CMAKE_BINARY_DIR}/libs/HexEditor/)
target_link_libraries(${PROJECT_NAME} PRIVATE HexEditor)

# - - Tools - -
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Tools/References/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Tools/Tabs/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Tools/Windows/)

# - - Menus - -
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ui/MenuBar/Menus/)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets)

find_program(WINDEPLOYQT_EXECUTABLE windeployqt)

if(WIN32 AND WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${WINDEPLOYQT_EXECUTABLE}
                $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Deploying Qt dependencies with windeployqt"
    )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_finalize_executable(${PROJECT_NAME})
