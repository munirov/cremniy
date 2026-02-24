cmake_minimum_required(VERSION 3.16)

project(codeeditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# cpp
set(SRC
    src/QCodeEditor.cpp
    src/QLineNumberArea.cpp
    src/QCXXHighlighter.cpp
    src/QSyntaxStyle.cpp
    src/QStyleSyntaxHighlighter.cpp
    src/QLanguage.cpp
    src/QJSONHighlighter.cpp
    src/QCECompleter.cpp
    src/QFramedTextAttribute.cpp
)

# headers с Q_OBJECT
set(HDR
    include/QCodeEditor.hpp
    include/QLineNumberArea.hpp
    include/QSyntaxStyle.hpp
    include/QStyleSyntaxHighlighter.hpp
    include/QCXXHighlighter.hpp
    include/QLanguage.hpp
    include/QJSONHighlighter.hpp
    include/QCECompleter.hpp
    include/QFramedTextAttribute.hpp
    include/QHighlightRule.hpp
    include/QHighlightBlockRule.hpp
)

# .qrc resources
set(QRC

)

add_library(codeeditor STATIC
    ${SRC}
    ${HDR}   # moc headers
    ${QRC}
    resources/codeeditor_res.qrc   # rcc resources
)

target_include_directories(codeeditor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(codeeditor PUBLIC Qt6::Widgets)