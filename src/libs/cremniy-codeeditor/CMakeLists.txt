cmake_minimum_required(VERSION 3.16)

project(cremniy-codeeditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Test)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_library(cremniy-codeeditor STATIC
    include/core/FileDataBuffer.h
    include/core/LineCache.h
    include/core/LineIndex.h
    include/core/UTF8Decoder.h
    include/widgets/CustomCodeEditor.h
    include/widgets/EditorEditOperations.h
    include/widgets/EditorLanguageSupport.h
    include/widgets/EditorTypingBehaviors.h
    include/widgets/LineNumberArea.h
    src/core/FileDataBuffer.cpp
    src/core/LineCache.cpp
    src/core/LineIndex.cpp
    src/core/UTF8Decoder.cpp
    src/widgets/CustomCodeEditor.cpp
    src/widgets/EditorEditOperations.cpp
    src/widgets/EditorLanguageSupport.cpp
    src/widgets/EditorTypingBehaviors.cpp
    src/widgets/LineNumberArea.cpp
    third_party/QCodeEditor/include/QCECompleter.hpp
    third_party/QCodeEditor/include/QCXXHighlighter.hpp
    third_party/QCodeEditor/include/QCodeEditor.hpp
    third_party/QCodeEditor/include/QFramedTextAttribute.hpp
    third_party/QCodeEditor/include/QHighlightBlockRule.hpp
    third_party/QCodeEditor/include/QHighlightRule.hpp
    third_party/QCodeEditor/include/QJSONHighlighter.hpp
    third_party/QCodeEditor/include/QLanguage.hpp
    third_party/QCodeEditor/include/QLineNumberArea.hpp
    third_party/QCodeEditor/include/QStyleSyntaxHighlighter.hpp
    third_party/QCodeEditor/include/QSyntaxStyle.hpp
    third_party/QCodeEditor/include/toolwidget.hpp
    third_party/QCodeEditor/src/QCECompleter.cpp
    third_party/QCodeEditor/src/QCXXHighlighter.cpp
    third_party/QCodeEditor/src/QCodeEditor.cpp
    third_party/QCodeEditor/src/QFramedTextAttribute.cpp
    third_party/QCodeEditor/src/QJSONHighlighter.cpp
    third_party/QCodeEditor/src/QLanguage.cpp
    third_party/QCodeEditor/src/QLineNumberArea.cpp
    third_party/QCodeEditor/src/QStyleSyntaxHighlighter.cpp
    third_party/QCodeEditor/src/QSyntaxStyle.cpp
    third_party/QCodeEditor/resources/codeeditor_res.qrc
)

option(BUILD_EXAMPLE "Example building required" On)

if (${BUILD_EXAMPLE})
    message(STATUS "CodeEditor example will be built.")
    add_subdirectory(example)
endif()


target_include_directories(cremniy-codeeditor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/core
    ${CMAKE_CURRENT_SOURCE_DIR}/include/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/QCodeEditor/include
)

target_link_libraries(cremniy-codeeditor PUBLIC Qt6::Widgets)
