cmake_minimum_required(VERSION 3.16)

project(cremniy VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Test)

set(PROJECT_SOURCES
        main.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(${PROJECT_NAME}
        MANUAL_FINALIZATION
        main.cpp

        app/IDEWindow/idewindow.cpp
        app/IDEWindow/idewindow.h
        app/WelcomeWindow/welcomeform.cpp
        app/WelcomeWindow/welcomeform.h

        dialogs/filecreatedialog.cpp
        dialogs/filecreatedialog.h
        dialogs/settingsdialog.cpp
        dialogs/settingsdialog.h
        dialogs/reversecalculatordialog.cpp
        dialogs/reversecalculatordialog.h

        widgets/clickablelineedit.cpp
        widgets/clickablelineedit.h
        widgets/filetab.cpp
        widgets/filetab.h
        widgets/filetreeview.cpp
        widgets/filetreeview.h

        widgets/terminal/terminalwidget.cpp
        widgets/terminal/terminalwidget.h

        utils/iconprovider.cpp
        utils/iconprovider.h

        resources/cremniy_res.qrc

        utils/utils.h
        utils/utils.cpp
        utils/appsettings.h
        utils/appsettings.cpp
        utils/instructionhelpservice.h
        utils/instructionhelpservice.cpp
        utils/filecontext.h
        utils/filemanager.h
        utils/filemanager.cpp
        utils/projectshistorymanager.cpp
        utils/projectshistorymanager.h

        core/ToolTab.h
        core/ToolTabFactory.cpp
        core/ToolTabFactory.h
        core/FileDataBuffer.h
        core/FileDataBuffer.cpp

        ui/MenuBar/menubarbuilder.h ui/MenuBar/menubarbuilder.cpp
        ui/MenuBar/menufactory.h ui/MenuBar/menufactory.cpp
        ui/MenuBar/Menus/File/filemenu.h ui/MenuBar/Menus/File/filemenu.cpp
        ui/MenuBar/basemenu.h
        ui/MenuBar/Menus/Edit/editmenu.h ui/MenuBar/Menus/Edit/editmenu.cpp
        ui/MenuBar/Menus/View/viewmenu.h ui/MenuBar/Menus/View/viewmenu.cpp
        ui/MenuBar/Menus/Build/buildmenu.h ui/MenuBar/Menus/Build/buildmenu.cpp
        ui/MenuBar/Menus/Tools/toolsmenu.h ui/MenuBar/Menus/Tools/toolsmenu.cpp
        ui/MenuBar/Menus/References/referencesmenu.h ui/MenuBar/Menus/References/referencesmenu.cpp
        ui/filestabwidget.cpp
        ui/filestabwidget.h
        ui/toolstabwidget.cpp
        ui/toolstabwidget.h

        ui/MenuBar/Menus/References/referencewindow.h
        ui/MenuBar/Menus/References/Windows/AsciiChars/asciicharsref.cpp
        ui/MenuBar/Menus/References/Windows/AsciiChars/asciicharsref.h
        ui/MenuBar/Menus/References/referencewindowfactory.h ui/MenuBar/Menus/References/referencewindowfactory.cpp



    )
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/widgets
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/utils
        ${CMAKE_CURRENT_SOURCE_DIR}/ui
        ${CMAKE_CURRENT_SOURCE_DIR}/terminal/ptyqt/core)

else()
    if(ANDROID)
        add_library(${PROJECT_NAME} SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(${PROJECT_NAME}
            ${PROJECT_SOURCES}
        )
    endif()
endif()


# - - Tool Tabs - -
file(GLOB TOOLTAB_DIRS CONFIGURE_DEPENDS ToolTabs/*)
foreach(dir ${TOOLTAB_DIRS})
    if(IS_DIRECTORY ${dir})
        add_subdirectory(${dir})
    endif()
endforeach()

target_link_libraries(${PROJECT_NAME} PRIVATE 
    Qt${QT_VERSION_MAJOR}::Widgets
)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt)

if(WIN32 AND QT_VERSION_MAJOR EQUAL 6 AND WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${WINDEPLOYQT_EXECUTABLE}
                $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Deploying Qt dependencies with windeployqt"
    )
endif()

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.${PROJECT_NAME})
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

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROJECT_NAME})
endif()
