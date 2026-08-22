cmake_minimum_required(VERSION 3.16)
project(CodeEditor LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_library(CodeEditor STATIC
    include/core/toolwidget.hpp
    include/core/LineCache.h
    include/core/LineIndex.h
    include/core/UTF8Decoder.h
    include/widgets/CustomCodeEditor.h
    include/widgets/EditorEditOperations.h
    include/widgets/EditorLanguageSupport.h
    include/widgets/EditorTypingBehaviors.h
    include/widgets/LineNumberArea.h
    include/widgets/BlameTooltip.h
    include/widgets/BlameLineInfo.h
    include/languages/LanguageDefinition.h
    include/languages/LanguageRegistry.h
    include/languages/LanguageRuleHelpers.h
    src/core/LineCache.cpp
    src/core/LineIndex.cpp
    src/core/UTF8Decoder.cpp
    src/widgets/CustomCodeEditor.cpp
    src/widgets/EditorEditOperations.cpp
    src/widgets/EditorLanguageSupport.cpp
    src/widgets/EditorTypingBehaviors.cpp
    src/widgets/LineNumberArea.cpp
    src/widgets/BlameTooltip.cpp
    src/widgets/highlighters/AsmHighlighter.h
    src/widgets/highlighters/AsmHighlighter.cpp
    src/widgets/highlighters/MakefileHighlighter.h
    src/widgets/highlighters/MakefileHighlighter.cpp
    src/widgets/highlighters/MarkdownHighlighter.h
    src/widgets/highlighters/MarkdownHighlighter.cpp
    src/widgets/highlighters/RuleBasedHighlighter.h
    src/widgets/highlighters/RuleBasedHighlighter.cpp
    src/widgets/highlighters/XmlLanguageHighlighter.h
    src/widgets/highlighters/XmlLanguageHighlighter.cpp
    src/languages/LanguageRegistry.cpp
    src/languages/AsmLanguage.cpp
    src/languages/CBraceFamilyLanguages.cpp
    src/languages/CppLanguage.cpp
    src/languages/GlslLanguage.cpp
    src/languages/IniLanguage.cpp
    src/languages/JavaScriptLanguage.cpp
    src/languages/JsonLanguage.cpp
    src/languages/LuaLanguage.cpp
    src/languages/MakeLanguage.cpp
    src/languages/MarkdownLanguage.cpp
    src/languages/PythonLanguage.cpp
    src/languages/RustLanguage.cpp
    src/languages/ShellLanguage.cpp
    src/languages/SlnLanguage.cpp
    src/languages/SqlLanguage.cpp
    src/languages/TomlLanguage.cpp
    src/languages/XmlLanguage.cpp
    src/languages/YamlLanguage.cpp
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

target_include_directories(CodeEditor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/core
    ${CMAKE_CURRENT_SOURCE_DIR}/include/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/QCodeEditor/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../core/file
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/QCodeEditor/src
)

target_link_libraries(CodeEditor PUBLIC Qt6::Widgets)