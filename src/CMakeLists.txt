cmake_minimum_required(VERSION 3.16)

project(cremniy VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8.2 REQUIRED COMPONENTS Widgets LinguistTools Test Svg)

set(PROJECT_SOURCES
        main.cpp
)

qt_add_executable(${PROJECT_NAME}
    MANUAL_FINALIZATION
    main.cpp

    # App
    app/IDEWindow/idewindow.cpp
    app/IDEWindow/idewindow.h
    app/WelcomeWindow/WelcomeForm/welcome_form.cpp
    app/WelcomeWindow/WelcomeForm/welcome_form.h
    app/WelcomeWindow/RecentProjectsPage/recent_projects_page.cpp
    app/WelcomeWindow/RecentProjectsPage/recent_projects_page.h
    app/WelcomeWindow/CreateProjectPage/create_project_page.cpp
    app/WelcomeWindow/CreateProjectPage/create_project_page.h
    app/WelcomeWindow/ProjectCard/project_card.cpp
    app/WelcomeWindow/ProjectCard/project_card.h

    # Dialogs
    dialogs/filecreatedialog.cpp
    dialogs/filecreatedialog.h
    dialogs/settingsdialog.cpp
    dialogs/settingsdialog.h

    # Widgets
    widgets/clickablelineedit.h
    widgets/filetab.cpp
    widgets/filetab.h
    widgets/filetreepanel.cpp
    widgets/filetreepanel.h
    widgets/terminal/terminalwidget.cpp
    widgets/terminal/terminalwidget.h

    # Resources
    resources/cremniy_res.qrc

    # Utils
    utils/utils.h
    utils/utils.cpp
        core/locale/LanguageManager.cpp
        core/locale/LanguageManager.h

    # Core
    core/file/filecontext.h
    core/file/FileDataBuffer.cpp
    core/file/FileDataBuffer.h
    core/file/filemanager.cpp
    core/file/filemanager.h

    core/project/projects_history_manager.cpp
    core/project/projects_history_manager.h

    core/settings/appsettings.cpp
    core/settings/appsettings.h

    core/icons/iconprovider.cpp
    core/icons/iconprovider.h

    core/settings/exclusionfilterproxymodel.cpp
    core/settings/exclusionfilterproxymodel.h

    core/modules/ModuleManager.h
    core/modules/ModuleManager.cpp
    core/modules/TabBase.h
    core/modules/WindowBase.h
    core/modules/ReferenceBase.h

    # UI
    ui/FilesTabWidget/filestabwidget.cpp
    ui/FilesTabWidget/filestabwidget.h
    ui/ToolsTabWidget/toolstabwidget.cpp
    ui/ToolsTabWidget/toolstabwidget.h

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
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/ToolsTabWidget
    ${CMAKE_CURRENT_SOURCE_DIR}/FilesTabWidget
    ${CMAKE_CURRENT_SOURCE_DIR}/app/WelcomeWindow/WelcomeForm
    ${CMAKE_CURRENT_SOURCE_DIR}/app/WelcomeWindow/RecentProjectsPage
    ${CMAKE_CURRENT_SOURCE_DIR}/app/WelcomeWindow/CreateProjectPage
    ${CMAKE_CURRENT_SOURCE_DIR}/app/WelcomeWindow/ProjectCard
)

file(GLOB_RECURSE PHOICONS_FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/phoicons/*"
)

qt_add_resources(${PROJECT_NAME} "phoicons"
    PREFIX "/icons"
    BASE "resources/icons"
    FILES ${PHOICONS_FILES}
)

# - - Libs - -
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/CodeEditor/ ${CMAKE_BINARY_DIR}/libs/CodeEditor/)
target_link_libraries(${PROJECT_NAME} PRIVATE CodeEditor)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/HexEditor/ ${CMAKE_BINARY_DIR}/libs/HexEditor/)
target_link_libraries(${PROJECT_NAME} PRIVATE HexEditor)

# - - Modules - -
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Modules/References/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Modules/Tabs/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Modules/Windows/)

# - - Menus - -
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ui/MenuBar/Menus/)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Svg)

set(APP_LANGUAGES
        en
        ru
)

list(JOIN APP_LANGUAGES ";" APP_LANGUAGES_STR)

configure_file(
        ${CMAKE_SOURCE_DIR}/config.h.in
        ${CMAKE_BINARY_DIR}/config.h
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_BINARY_DIR}
)

set(TS_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/locale/translations/app_ru.ts
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/locale/translations/qtbase_ru.ts
)

qt6_add_translation(QM_FILES ${TS_FILES})

add_custom_target(translations ALL DEPENDS ${QM_FILES})
add_dependencies(${PROJECT_NAME} translations)

if (APPLE)
    set(TRANSLATIONS_DIR
            "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.app/Contents/Resources/translations"
    )
else ()
    set(TRANSLATIONS_DIR
            "${CMAKE_BINARY_DIR}/Resources/translations"
    )
endif ()

file(MAKE_DIRECTORY ${TRANSLATIONS_DIR})

foreach(QM_FILE ${QM_FILES})
    get_filename_component(QM_FILENAME ${QM_FILE} NAME)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QM_FILE}"
            "${TRANSLATIONS_DIR}/${QM_FILENAME}"
    )
endforeach()

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
