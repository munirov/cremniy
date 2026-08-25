#include "languages/LanguageRegistration.h"

// One extern declaration per language, matching the *_forceLink thunk that
// CREMNIY_REGISTER_LANGUAGE(<fn>) generates in each languages/*.cpp file.
// These are deliberately declared here rather than in a shared header,
// since each is only ever called once, from registerAllLanguages() below.
extern void registerAsmLanguage_forceLink();
extern void registerCSharpLanguage_forceLink();
extern void registerCppLanguage_forceLink();
extern void registerGlslLanguage_forceLink();
extern void registerGoLanguage_forceLink();
extern void registerIniLanguage_forceLink();
extern void registerJavaLanguage_forceLink();
extern void registerJavaScriptLanguage_forceLink();
extern void registerJsonLanguage_forceLink();
extern void registerLuaLanguage_forceLink();
extern void registerMakeLanguage_forceLink();
extern void registerMarkdownLanguage_forceLink();
extern void registerPhpLanguage_forceLink();
extern void registerPythonLanguage_forceLink();
extern void registerRustLanguage_forceLink();
extern void registerShellLanguage_forceLink();
extern void registerSlnLanguage_forceLink();
extern void registerSqlLanguage_forceLink();
extern void registerTomlLanguage_forceLink();
extern void registerXmlLanguage_forceLink();
extern void registerYamlLanguage_forceLink();

void registerAllLanguages()
{
    // NOTE for contributors adding a new language: add one line here calling
    // your language's <registerFn>_forceLink(), plus the matching extern
    // declaration above. This is the one explicit step required in addition
    // to creating src/languages/<name>/<Name>Language.cpp (its own directory,
    // registered via its own CMakeLists.txt) and CREMNIY_REGISTER_LANGUAGE
    // there; everything else about adding a language is unchanged.
    // See docs/adding_a_language.md for the full explanation.
    registerAsmLanguage_forceLink();
    registerCSharpLanguage_forceLink();
    registerCppLanguage_forceLink();
    registerGlslLanguage_forceLink();
    registerGoLanguage_forceLink();
    registerIniLanguage_forceLink();
    registerJavaLanguage_forceLink();
    registerJavaScriptLanguage_forceLink();
    registerJsonLanguage_forceLink();
    registerLuaLanguage_forceLink();
    registerMakeLanguage_forceLink();
    registerMarkdownLanguage_forceLink();
    registerPhpLanguage_forceLink();
    registerPythonLanguage_forceLink();
    registerRustLanguage_forceLink();
    registerShellLanguage_forceLink();
    registerSlnLanguage_forceLink();
    registerSqlLanguage_forceLink();
    registerTomlLanguage_forceLink();
    registerXmlLanguage_forceLink();
    registerYamlLanguage_forceLink();
}
