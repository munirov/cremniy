target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/disassemblertab.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/disassemblertab.h
    ${CMAKE_CURRENT_SOURCE_DIR}/disassemblerworker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/disassemblerworker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/disasm/disasmsyntaxdelegate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/disasm/disasmsyntaxdelegate.h
    ${CMAKE_CURRENT_SOURCE_DIR}/disasm/disasmtexthighlighter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/disasm/disasmtexthighlighter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/disasm/backends/radare2backend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/disasm/backends/radare2backend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/instructionhelpservice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/instructionhelpservice.h
)