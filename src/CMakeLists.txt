cmake_minimum_required(VERSION 3.16)

project(cremniy VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

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
        widgets/filestabwidget.cpp
        widgets/filestabwidget.h
        widgets/filetab.cpp
        widgets/filetab.h
        widgets/filetreeview.cpp
        widgets/filetreeview.h
        widgets/tooltabwidget.cpp
        widgets/tooltabwidget.h

        utils/iconprovider.cpp
        utils/iconprovider.h

        resources/cremniy_res.qrc

        utils/utils.h
        utils/utils.cpp
        utils/appsettings.h
        utils/appsettings.cpp
        utils/globalwidgetsmanager.h

        utils/filecontext.h

        utils/filemanager.h
        utils/filemanager.cpp

        core/ToolTab.h
        core/ToolTabFactory.cpp
        core/ToolTabFactory.h
            utils/ProjectsHistoryManager.cpp
            utils/ProjectsHistoryManager.h


    )
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/widgets
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/utils)
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

target_link_libraries(${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

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
