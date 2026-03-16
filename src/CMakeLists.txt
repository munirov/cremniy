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

add_subdirectory(components/codeeditor)
add_subdirectory(components/hexview)

# Explicitly invoke moc on disassemblerworker.h.
# AUTOMOC can miss headers that live in subdirectories and are included
# only indirectly (via another header). qt_wrap_cpp guarantees the
# meta-object code is always generated and linked.
qt_wrap_cpp(DISASSEMBLERWORKER_MOC_SRCS
    widgets/disassemblerworker.h
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

        widgets/codeeditortab.h widgets/codeeditortab.cpp
        widgets/hexviewtab.h widgets/hexviewtab.cpp
        widgets/disassemblertab.h widgets/disassemblertab.cpp
        widgets/disassemblerworker.h widgets/disassemblerworker.cpp

        # Generated moc source for DisassemblerWorker
        ${DISASSEMBLERWORKER_MOC_SRCS}

        include/toolwidget.hpp
        include/tooltab.h

        utils/utils.h
        utils/utils.cpp
        utils/globalwidgetsmanager.h

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

target_link_libraries(${PROJECT_NAME} PRIVATE codeeditor)
target_link_libraries(${PROJECT_NAME} PRIVATE hexview)

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
