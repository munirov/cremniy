cmake_minimum_required(VERSION 3.16)

find_package(Git QUIET)

if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if(GIT_TAG MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
        set(PROJECT_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
    endif()
endif()

if(NOT PROJECT_VERSION)
    set(PROJECT_VERSION "0.0.0")
endif()

project(cremniy VERSION ${PROJECT_VERSION} LANGUAGES CXX)

message(STATUS "Cremniy version: ${PROJECT_VERSION}")

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8.2 REQUIRED COMPONENTS Core Widgets Gui Concurrent LinguistTools Test Svg Network)

# libgit2 ships a CMake package on some platforms and only a pkg-config file or
# plain headers/libraries on others. Support all three layouts so developers do
# not have to pass platform-specific paths to CMake.
set(CREMNIY_LIBGIT2_TARGET "")
set(CREMNIY_LIBGIT2_INCLUDE_DIRS "")

get_filename_component(CREMNIY_COMPILER_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
get_filename_component(CREMNIY_TOOLCHAIN_PREFIX "${CREMNIY_COMPILER_BIN_DIR}" DIRECTORY)
set(CREMNIY_LIBGIT2_HINTS
    "$ENV{LIBGIT2_ROOT}"
    "$ENV{MINGW_PREFIX}"
    "${CREMNIY_TOOLCHAIN_PREFIX}"
    "$ENV{HOMEBREW_PREFIX}/opt/libgit2"
    "/opt/homebrew/opt/libgit2"
    "/usr/local/opt/libgit2"
)
list(FILTER CREMNIY_LIBGIT2_HINTS EXCLUDE REGEX "^$")

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBGIT2 QUIET IMPORTED_TARGET libgit2)
    if(TARGET PkgConfig::LIBGIT2)
        set(CREMNIY_LIBGIT2_TARGET PkgConfig::LIBGIT2)
    endif()
endif()

if(NOT CREMNIY_LIBGIT2_TARGET)
    find_package(libgit2 CONFIG QUIET HINTS ${CREMNIY_LIBGIT2_HINTS})
    if(TARGET libgit2::libgit2package)
        set(CREMNIY_LIBGIT2_TARGET libgit2::libgit2package)
    elseif(TARGET libgit2::libgit2)
        set(CREMNIY_LIBGIT2_TARGET libgit2::libgit2)
    endif()
endif()

if(NOT CREMNIY_LIBGIT2_TARGET)
    find_path(LIBGIT2_INCLUDE_DIR
        NAMES git2.h
        HINTS ${CREMNIY_LIBGIT2_HINTS}
        PATH_SUFFIXES include
    )
    find_library(LIBGIT2_LIBRARY
        NAMES git2 libgit2
        HINTS ${CREMNIY_LIBGIT2_HINTS}
        PATH_SUFFIXES lib lib64
    )
    if(NOT LIBGIT2_INCLUDE_DIR OR NOT LIBGIT2_LIBRARY)
        message(FATAL_ERROR
            "libgit2 was not found. Install the libgit2 development package "
            "or set LIBGIT2_ROOT to its installation prefix."
        )
    endif()
    set(CREMNIY_LIBGIT2_TARGET "${LIBGIT2_LIBRARY}")
    set(CREMNIY_LIBGIT2_INCLUDE_DIRS "${LIBGIT2_INCLUDE_DIR}")
endif()

if(NOT WIN32)
    find_package(ZLIB REQUIRED)
    find_package(OpenSSL REQUIRED)
    find_library(PCRE_LIB NAMES pcre pcre2-8)
    find_library(HTTP_PARSER_LIB NAMES http_parser REQUIRED)
endif()

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
    dialogs/pages/generalsettingspage.cpp
    dialogs/pages/generalsettingspage.h
    dialogs/pages/filessettingspage.cpp
    dialogs/pages/filessettingspage.h

    # Widgets
    widgets/clickablelineedit.h
    widgets/filetab.cpp
    widgets/filetab.h
    widgets/filetreepanel.cpp
    widgets/filetreepanel.h
    widgets/terminal/terminalwidget.cpp
    widgets/terminal/terminalwidget.h
    widgets/search/searchpanel.cpp
    widgets/search/searchpanel.h

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
    core/file/filemanager.cpp
    core/file/filemanager.h

    core/file/filesynccontroller.cpp
    core/file/filesynccontroller.h
    core/file/filesyncdecision.cpp
    core/file/filesyncdecision.h
    core/file/filesyncmonitor.cpp
    core/file/filesyncmonitor.h
    core/file/filemanager.h

    core/project/projects_history_manager.cpp
    core/project/projects_history_manager.h

    core/search/searchengine.cpp
    core/search/searchengine.h

    core/settings/appsettings.cpp
    core/settings/appsettings.h
    core/settings/settingspage.h
    core/settings/settingsregistry.cpp
    core/settings/settingsregistry.h

    core/update/updatechecker.cpp
    core/update/updatechecker.h

    core/icons/iconprovider.cpp
    core/icons/iconprovider.h

    core/settings/exclusionfilterproxymodel.cpp
    core/settings/exclusionfilterproxymodel.h

    core/modules/ModuleManager.h
    core/modules/ModuleManager.cpp
    core/modules/TabBase.h
    core/modules/WindowBase.h
    core/modules/ReferenceBase.h

    # Git
    core/git/gitmanager.cpp
    core/git/gitmanager.h
    core/git/gitblameworker.cpp
    core/git/gitblameworker.h

    # UI
    ui/FilesTabWidget/filestabwidget.cpp
    ui/FilesTabWidget/filestabwidget.h
    ui/ToolsTabWidget/toolstabwidget.cpp
    ui/ToolsTabWidget/toolstabwidget.h

    # UI Menubar
    ui/MenuBar/menubarbuilder.h ui/MenuBar/menubarbuilder.cpp
    ui/MenuBar/menufactory.h ui/MenuBar/menufactory.cpp
    ui/MenuBar/basemenu.h
    dialogs/configurebuild.h dialogs/configurebuild.cpp
    core/project/project_info_manager.h core/project/project_info_manager.cpp

    core/buildman/buildmanager.cpp
    core/buildman/buildmanager.cpp
    widgets/logView/logview.h widgets/logView/logview.cpp


    widgets/buildTab/buildtab.cpp widgets/buildTab/buildtab.h

)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/core/file
    ${CMAKE_CURRENT_SOURCE_DIR}/core/git
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

if(CREMNIY_LIBGIT2_INCLUDE_DIRS)
    target_include_directories(${PROJECT_NAME} PRIVATE ${CREMNIY_LIBGIT2_INCLUDE_DIRS})
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Concurrent
    Qt6::Svg
    Qt6::Network
    ${CREMNIY_LIBGIT2_TARGET}
)

if(NOT WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${HTTP_PARSER_LIB}
        ${PCRE_LIB}
        OpenSSL::SSL
        OpenSSL::Crypto
        ZLIB::ZLIB
    )
endif()

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
