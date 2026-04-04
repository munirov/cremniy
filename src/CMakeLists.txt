cmake_minimum_required(VERSION 3.16)

project(cremniy VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CREMNIY_ENABLE_ASAN "Enable AddressSanitizer for supported debug builds" OFF)
option(CREMNIY_BUILD_TESTS "Build projectsearch unit tests (needs Qt Test; disable if that target fails to compile)" ON)

if(CREMNIY_ENABLE_ASAN)
    if(MSVC)
        add_compile_options($<$<CONFIG:Debug>:/fsanitize=address>)
        add_link_options($<$<CONFIG:Debug>:/INCREMENTAL:NO> $<$<CONFIG:Debug>:/fsanitize=address>)
    else()
        add_compile_options($<$<CONFIG:Debug>:-fsanitize=address> $<$<CONFIG:Debug>:-fno-omit-frame-pointer>)
        add_link_options($<$<CONFIG:Debug>:-fsanitize=address>)
    endif()
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
if(CREMNIY_BUILD_TESTS)
    find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS Test)
endif()

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
        dialogs/projectsearchdialog.cpp
        dialogs/projectsearchdialog.h
        dialogs/projectsearchresultdelegate.cpp
        dialogs/projectsearchresultdelegate.h

        utils/projectsearch/projectsearchengine.cpp
        utils/projectsearch/projectsearchengine.h
        utils/projectsearch/projectsearchworker.cpp
        utils/projectsearch/projectsearchworker.h

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
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/cremniy-codeeditor/include
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

set(BUILD_EXAMPLE OFF CACHE BOOL "Disable vendored code editor example" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/cremniy-codeeditor ${CMAKE_BINARY_DIR}/libs/cremniy-codeeditor)


# - - Tool Tabs - -
file(GLOB TOOLTAB_DIRS CONFIGURE_DEPENDS ToolTabs/*)
foreach(dir ${TOOLTAB_DIRS})
    if(IS_DIRECTORY ${dir})
        add_subdirectory(${dir})
    endif()
endforeach()

target_link_libraries(${PROJECT_NAME} PRIVATE 
    Qt${QT_VERSION_MAJOR}::Widgets
    cremniy-codeeditor
)

if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${PROJECT_NAME} PRIVATE "-Wa,-mbig-obj")
endif()

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

if(CREMNIY_BUILD_TESTS AND TARGET Qt${QT_VERSION_MAJOR}::Test)
    enable_testing()
    qt_add_executable(projectsearch_tests
        tests/test_projectsearchengine.cpp
        utils/projectsearch/projectsearchengine.cpp
    )
    target_include_directories(projectsearch_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(projectsearch_tests PRIVATE Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Core)
    add_test(NAME projectsearch_tests COMMAND projectsearch_tests)
elseif(CREMNIY_BUILD_TESTS)
    message(STATUS "Qt Test not available — skipping projectsearch_tests (application still builds). "
                   "Install Qt Test or configure with -DCREMNIY_BUILD_TESTS=OFF.")
endif()
