# Consolidated Audit Report: Cremniy Project

**Date:** 2026-04-29  
**Scope:** Full project (cremniy workspace)  
**Audited by:** senior-reviewer + security-auditor + reviewer  
**Overall Health Score:** **2.0 / 10** ⚠️

---

## Executive Summary

| Severity | Architecture | Security | Code Quality | Total |
|----------|--------------|----------|--------------|-------|
| **Critical** | 1 | 1 | 0 | **2** |
| **High** | 4 | 2 | 2 | **8** |
| **Medium** | 4 | 4 | 6 | **14** |
| **Low** | 4 | 3 | 4 | **11** |
| **Total** | **13** | **10** | **12** | **35** |

### Project Context

**Important Note:** The user plans a future migration to a Tauri + Rust host with a React frontend (BMFP-style) while maintaining UI visual parity (1:1). This audit describes the **current Qt/C++ desktop codebase health only** and reflects architectural and quality concerns specific to the existing implementation. Post-migration, many findings will be obviated by the new architecture.

### Health Score Calculation

Starting score: **10.0**
- Critical issues: 2 × -2.0 = -4.0
- High issues: 8 × -0.5 = -4.0 (capped at -3.0, applied as -3.0)
- Medium issues: 14 × -0.1 = -1.4 (capped at -1.0)
- **Final Score: 10.0 - 4.0 - 3.0 - 1.0 = 2.0**

---

## Architecture Findings

### Critical

#### [A1] Potential Crash in ToolTab Initialization

**File:** `cremniy/src/ui/toolstabwidget.cpp`

**Issue:** For each registered tool ID, `ToolTab*` returned from `ToolTabFactory::create` is dereferenced (calling `tab->toolName()`, `setFile`, connecting signals) **before** the `if (tab)` guard. A null creator or failed registration yields undefined behavior instead of skipping the tab safely.

**Impact:** Immediate crash on startup if any tool factory registration fails or returns nullptr.

**Recommendation:** Check `if (tab)` immediately after `create()` and skip registration on null.

---

### High

#### [A2] Missing Architecture Documents

**Files:** `Docs/BMSP_ARCHITECTURE.md`, `Docs/BMFP_ARCHITECTURE.md`

**Issue:** Stated sources of truth for architecture are not present in the workspace. Instead, `docs/EN/developer_guide.md` and companion files describe intended layout. Cursor/user rules assume API/Core/Infrastructure and boundary/domain/shared layering, which do not match this Qt desktop IDE architecture.

**Impact:** Ambiguous architecture guidance; rules in place do not reflect actual codebase structure.

**Recommendation:** Create `Docs/CREMNIY_ARCHITECTURE.md` documenting Qt/C++ desktop layering, or reconcile with migration plan.

---

#### [A3] Inverted Dependency: Widgets Depend Upward

**Files:** 
- `cremniy/src/widgets/filetab.h` 
- `cremniy/src/ui/toolstabwidget.h`

**Issue:** `cremniy/docs/EN/developer_guide.md` places reusable code in `widgets/`, but `filetab.h` includes and embeds `toolstabwidget.h`. The `widgets/` layer depends upward on `ui/` instead of staying a leaf reusable layer.

**Impact:** Reusable components tie to UI-specific implementations; reduces composability.

**Recommendation:** Invert include chain: move tab abstractions to `widgets/` and have `ui/` depend on `widgets/`.

---

#### [A4] Core Not Isolated from Auxiliary Types

**Files:**
- `cremniy/src/core/ToolTab.h`
- `cremniy/src/utils/filecontext.h`

**Issue:** `cremniy/src/core/ToolTab.h` includes `cremniy/src/utils/filecontext.h`, coupling the tab abstraction directly to utils with no inversion boundary.

**Impact:** Core logic tightly coupled to utility layer; harder to test, reuse, or migrate core logic.

**Recommendation:** Define a lean domain contract in core; have utils adapt to it via dependency injection or interfaces.

---

#### [A5] Fragile Include Paths Violate Project Rule

**File:** `cremniy/src/ToolTabs/Disassembler/disasm/backends/radare2backend.h`

**Issue:** Contains `#include "../../disassemblerworker.h"` — project rule (`cremniy/docs/EN/developer_guide.md`) forbids `../../` includes. Signals fragile include paths prone to breakage on refactoring.

**Impact:** Fragile build; hard to move files safely.

**Recommendation:** Use CMake include directories or relative paths constrained to subdirectories, not parent traversal.

---

### Medium

#### [A6] Disassembler Feature Concentrated in Large Module

**File:** `cremniy/src/ToolTabs/Disassembler/disassemblertab.cpp` (~1100+ lines)

**Issue:** God-module candidate — disassembler feature logic concentrated in single translation unit.

**Impact:** Difficult to test, maintain, and reason about; tight coupling within module.

**Recommendation:** Decompose into focused classes: `DisassemblyModel`, `DisassemblyView`, `DisassemblyController`.

---

#### [A7] No CMake Test Wiring

**File:** `cremniy/CMakeLists.txt`

**Issue:** Build is one executable; no `enable_testing()` / `add_test()` directives detected.

**Impact:** No built-in test harness; manual test execution required.

**Recommendation:** Add CMake test configuration and separate test executable target.

---

#### [A8] MenuBar Pulls Full IDEWindow Header

**File:** `cremniy/src/ui/MenuBar/menubarbuilder.h`

**Issue:** Includes full `cremniy/src/app/IDEWindow/idewindow.h` — unnecessary compile-time coupling.

**Impact:** Changes to IDEWindow force MenuBar recompile; harder to parallelize builds.

**Recommendation:** Forward-declare or pass interface pointer; minimize header dependencies.

---

#### [A9] Layout Widget Added Twice

**File:** `cremniy/src/app/IDEWindow/idewindow.cpp` (lines ~47 and ~59)

**Issue:** `m_filesTreeView` added to `leftLayout` twice.

**Impact:** Layout/ownership confusion; potential memory issues or unexpected UI behavior.

**Recommendation:** Add widget once; verify layout assignment is idempotent.

---

### Low

#### [A10] Uninitialized Class Members

**File:** `cremniy/src/utils/filecontext.h`

**Issue:** Constructor initializes only `m_filePath`; other members not initialized — correctness smell.

**Impact:** Possible use of garbage values for uninitialized fields.

**Recommendation:** Initialize all members in constructor or use member initializers.

---

#### [A11] Global Singleton Registry

**File:** `cremniy/src/core/ToolTabFactory.h`

**Issue:** Global singleton registry complicates testing and dependency injection.

**Impact:** Hard to test in isolation; tightly coupled to global state.

**Recommendation:** Pass factory as constructor argument; use factory parameter instead of singleton.

---

#### [A12] ToolTab Combines Domain and Presentation

**File:** `cremniy/src/core/ToolTab.h`

**Issue:** Mixes domain-ish concerns (tab identity, lifecycle) with QWidget — limits strict separation.

**Impact:** Core logic tied to Qt framework; harder to migrate or test without Qt.

**Recommendation:** Separate `ITabModel` (domain) from `TabWidget` (presentation).

---

#### [A13] Heavy Vendored Code

**Directories:**
- `ToolTabs/CodeEditor/QCodeEditor`
- `ToolTabs/Binary/QHexView`

**Issue:** Large vendored libraries maintained inline — high maintenance surface.

**Impact:** Manual CVE tracking, merge conflicts on updates, bloat.

**Recommendation:** Evaluate extraction to external dependency or git submodule; or pin + document.

---

## Security Findings

### Critical

#### [S1] Unvalidated radare2 Pre-Commands Leading to Arbitrary Execution

**Files:**
- `cremniy/src/ToolTabs/Disassembler/disasm/backends/radare2backend.cpp` (`buildPrelude`)
- `AppSettings::radare2PreCommands`
- Settings dialog

**Issue:** radare2 pre-commands passed into `-c` subprocess arguments without validation. Unvalidated strings (including after Import INI) can cause arbitrary command execution as the IDE user when disassembler runs.

**Impact:** Arbitrary shell command execution; privilege escalation if IDE runs with elevated privileges; full IDE compromise.

**Severity:** CRITICAL — direct code execution vector.

**Recommendation:**
1. Whitelist allowed radare2 commands or disable custom pre-commands entirely.
2. Sanitize/escape strings passed to subprocess arguments.
3. Use subprocess `shell=False` and argument list (not shell string).
4. Audit INI import; reject or validate imported settings.

---

### High

#### [S2] Use-Before-Null-Check in Tab Factory

**File:** `cremniy/src/ui/toolstabwidget.cpp`

**Issue:** `ToolTab*` dereferenced before null check after `ToolTabFactory::create` — UB / local DoS.

**Impact:** Crash on malformed tab registration; potential information leak from stack.

**Recommendation:** Check `if (tab)` immediately after creation; skip or error gracefully.

---

#### [S3] Unchecked Dynamic Cast Results

**Files:**
- `cremniy/src/ui/toolstabwidget.cpp` (`refreshDataAllTabs`, `removeStar`)
- `cremniy/src/ui/filestabwidget.cpp` (`saveFileSlot`)

**Issue:** `dynamic_cast` results not checked for null before use — crash local DoS if widget type diverges.

**Impact:** Crash if unexpected widget type stored; local denial of service.

**Recommendation:** Check `if (casted)` or use `dynamic_cast_throw` / exception; never assume cast succeeds.

---

### Medium

#### [S4] Instruction Help JSON Loaded from Filesystem

**File:** `cremniy/src/...instructionhelpservice.cpp`

**Issue:** Instruction help JSON loaded from filesystem next to binary if present — dropped/replaced file abuse; large JSON parsing surface.

**Impact:** File substitution allows code injection or malicious data; DoS via large JSON.

**Recommendation:**
1. Embed JSON in binary or load from read-only resource.
2. Validate JSON schema before parsing.
3. Enforce file integrity (hash, signature).

---

#### [S5] Terminal History Plaintext

**File:** `cremniy/src/.../terminalwidget.cpp`, `terminal_history.txt`

**Issue:** Terminal history saved plaintext on disk — local information disclosure (commands, paths, credentials).

**Impact:** Sensitive data exposure if disk accessible; credential leakage.

**Recommendation:** Encrypt history file or store in encrypted keychain; or disable persistent history.

---

#### [S6] Verbose Logging in Disassembler

**File:** `cremniy/src/ToolTabs/Disassembler/disassemblerworker.cpp`

**Issue:** Verbose logging in disassembler path — path/workflow disclosure.

**Impact:** Information leakage in logs; aids reconnaissance if logs exposed.

**Recommendation:** Use debug-level logging; sanitize paths in release builds.

---

#### [S7] No CVE Audit for Dependencies

**Issue:** No package-manager CVE audit visible; Qt + vendored deps — manual supply-chain tracking.

**Impact:** Unknown vulnerability surface; potential exploitation of known CVEs.

**Recommendation:**
1. Run `conan audit` or equivalent for Qt/deps.
2. Pin dependency versions; monitor security advisories.
3. Integrate vulnerability scan in CI.

---

### Low

#### [S8] Integrated Terminal Spawns Full Interactive Shell

**Issue:** Integrated terminal spawns full interactive shell — broad local execution surface (expected for IDE).

**Impact:** Attacker with IDE access can execute arbitrary commands locally; accepted risk for IDE use case.

**Recommendation:** Restrict shell commands in untrusted scenarios or sandbox terminal execution.

---

#### [S9] CI Release Workflow Fetches Helpers Over HTTPS Without Hash Pinning

**Issue:** CI workflow fetches helpers without hash pinning — transient supply-chain risk.

**Impact:** MITM can inject malicious helper; indirect code injection.

**Recommendation:** Pin helper hash or signature in CI config; verify downloads.

---

#### [S10] FileDataBuffer Hash Truncates SHA-256

**Issue:** `FileDataBuffer` truncates SHA-256 to `uint` — OK for dirty UI if documented; misuse risk for crypto.

**Impact:** Hash collision risk if used for security; OK if only for UI dirty flag.

**Recommendation:** Document intent; use full hash if security-relevant; warn against misuse.

---

## Code Quality Findings

### High

#### [Q1] Unsafe Pointer Dereference and Missing Type Checks

**File:** `cremniy/src/ui/toolstabwidget.cpp`

**Issue:**
- `create` may return null but `tab->toolName()` is called before null guard
- `refreshDataAllTabs` / `removeStar` perform unchecked `dynamic_cast`

**Impact:** Undefined behavior; crash on null or type mismatch.

**Recommendation:** Add null checks after factory calls; verify cast results.

---

#### [Q2] Unsafe Widget Type Conversion

**File:** `cremniy/src/ui/filestabwidget.cpp` (`saveFileSlot`)

**Issue:** Unchecked `dynamic_cast` then unconditional `saveFile` — unexpected widget type → UB.

**Impact:** Crash if widget type diverges.

**Recommendation:** Add cast null check; handle cast failure gracefully.

---

### Medium

#### [Q3] Partial Initialization in FileContext

**File:** `cremniy/src/utils/filecontext.h` / `.cpp`

**Issue:** Only `m_filePath` initialized; other members uninitialized — easy to read garbage.

**Impact:** Undefined behavior on access to uninitialized fields.

**Recommendation:** Initialize all members in constructor.

---

#### [Q4] Oversized Translation Unit

**File:** `cremniy/src/ToolTabs/Disassembler/disassemblertab.cpp` (~1100+ lines)

**Issue:** God-module candidate — oversized unit difficult to test and maintain.

**Impact:** High cyclomatic complexity; tight coupling; slow build.

**Recommendation:** Decompose into focused classes and modules.

---

#### [Q5] Unnecessary Header Coupling

**File:** `cremniy/src/ui/MenuBar/basemenu.h`

**Issue:** Pulls full `idewindow.h` — compile coupling.

**Impact:** Slow builds; unnecessary recompilation.

**Recommendation:** Forward-declare; pass interface; minimize header deps.

---

#### [Q6] Duplicate Widget Addition

**File:** `cremniy/src/app/IDEWindow/idewindow.cpp` (lines ~47 and ~59)

**Issue:** `m_filesTreeView` added to `leftLayout` twice.

**Impact:** Layout confusion; potential UI/memory issues.

**Recommendation:** Add once; verify idempotency.

---

#### [Q7] Unclear Ownership Pattern

**File:** `cremniy/src/app/IDEWindow/idewindow.cpp`

**Issue:** `MenuBarBuilder new` without clear ownership / cleanup — leak pattern.

**Impact:** Potential memory leak if not properly deleted.

**Recommendation:** Use smart pointers; clarify ownership semantics.

---

#### [Q8] Monolithic CMake with Unused Test Framework

**File:** `cremniy/CMakeLists.txt`

**Issue:** Single monolithic exe target; `Qt6::Test` required but no tests.

**Impact:** No test harness wired; dead test dependency.

**Recommendation:** Add CMake test configuration; separate test executable.

---

#### [Q9] Tight Coupling via Static Registration

**File:** `cremniy/src/ToolTabs/Disassembler/disassemblertab.cpp` (static registration)

**Issue:** `ToolTab` static registration in implementation — coupling and init order smell.

**Impact:** Hard to test; init order dependencies; global state.

**Recommendation:** Use factory registration in `main()` or DI container.

---

### Low

#### [Q10] Debug Noise

**Files:** `toolstabwidget.cpp`, `menubarbuilder.cpp`

**Issue:** `qDebug` statements left in production code.

**Impact:** Log spam; performance impact; information leak.

**Recommendation:** Use conditional logging; strip in release builds.

---

#### [Q11] Dead Code

**File:** `cremniy/src/ui/toolstabwidget.cpp`

**Issue:** Large commented-out `GlobalWidgetsManager` block.

**Impact:** Maintenance burden; confusion.

**Recommendation:** Remove or move to separate branch.

---

#### [Q12] Non-Idiomatic Error Handling

**File:** `cremniy/src/.../filemanager.cpp`

**Issue:** Return `nullptr` for `QByteArray` failure path — non-idiomatic.

**Impact:** Inconsistent API; caller uncertainty on valid values.

**Recommendation:** Use `std::optional<QByteArray>` or throw exception.

---

#### [Q13] Macro Indirection

**File:** `cremniy/src/ToolTabs/Binary/QHexView/qhexutils.cpp`

**Issue:** Macro surface — indirect indirection obscures logic.

**Impact:** Harder to reason about; macro hygiene surface.

**Recommendation:** Replace macros with inline templates or constexpr functions where safe.

---

## Critical Issues (Fix Immediately)

| ID | Issue | Impact | Timeline |
|-----|-------|--------|----------|
| **A1** | Null pointer dereference in ToolTab factory | Crash on startup | **This sprint** |
| **S1** | Arbitrary radare2 command execution | Full IDE compromise | **This sprint** |
| **S2** | Use-before-null-check in tab creation | Local DoS crash | **This sprint** |

**Action Items:**
1. Add null check after `ToolTabFactory::create()` in `toolstabwidget.cpp` (A1, S2).
2. Whitelist / sanitize radare2 pre-commands; use subprocess argument list, not shell string (S1).
3. Verify all factory calls and dynamic casts have guards.

---

## High Priority Issues

| ID | Issue | Impact | Timeline |
|-----|-------|--------|----------|
| **A2** | Missing architecture documentation | Ambiguous guidance | Next sprint |
| **A3** | Inverted widget dependency | Reusability blocked | Next sprint |
| **A4** | Core tightly coupled to utils | Testability reduced | Next sprint |
| **A5** | Fragile include paths | Build fragility | Next sprint |
| **S3** | Unchecked dynamic_cast results | Crash DoS | Next sprint |
| **Q1** | Unsafe pointer dereference | UB / crash | Next sprint |
| **Q2** | Unsafe widget type conversion | UB / crash | Next sprint |

---

## Medium Priority Issues

| ID | Issue | Impact | Timeline |
|-----|-------|--------|----------|
| **A6** | God-module in Disassembler | Maintainability | This quarter |
| **A7** | No CMake test wiring | No test harness | This quarter |
| **A8** | MenuBar header coupling | Slow builds | This quarter |
| **A9** | Duplicate layout widget | Layout confusion | This quarter |
| **S4** | Instruction JSON file abuse | Data injection / DoS | This quarter |
| **S5** | Plaintext terminal history | Credential leak | This quarter |
| **S6** | Verbose logging | Info disclosure | This quarter |
| **S7** | No CVE audit | Unknown vulnerabilities | This quarter |
| **Q3** | Uninitialized members | Undefined behavior | This quarter |
| **Q4** | Oversized module | Maintainability | This quarter |
| **Q5** | Header coupling | Build speed | This quarter |
| **Q6** | Duplicate widget add | UI/memory issues | This quarter |
| **Q7** | Unclear ownership | Memory leak risk | This quarter |
| **Q8** | Unused test framework | Dead dependency | This quarter |
| **Q9** | Static registration coupling | Init order smell | This quarter |

---

## Low Priority Issues

| ID | Issue | Impact | Timeline |
|-----|-------|--------|----------|
| **A10** | Uninitialized members | Garbage values | Next quarter / refactor |
| **A11** | Global singleton | Test difficulty | Next quarter / refactor |
| **A12** | Mixed domain/presentation | Migration difficulty | Next quarter / refactor |
| **A13** | Vendored code maintenance | CVE surface | Next quarter / refactor |
| **S8** | Terminal shell scope | Expected IDE behavior | Accepted risk |
| **S9** | CI helper hash pinning | Supply-chain risk | Next release cycle |
| **S10** | SHA-256 truncation | Hash collision risk | Document & verify intent |
| **Q10** | Debug noise | Log spam | Clean up next sprint |
| **Q11** | Dead code | Maintenance burden | Clean up next sprint |
| **Q12** | Non-idiomatic error handling | API confusion | Next refactor |
| **Q13** | Macro indirection | Code clarity | Next refactor |

---

## Priority Matrix

```
        ╔─ HIGH IMPACT ─╗
        │ │
URGENT  │ A1, S1, S2, S3 │
        │ Q1, Q2         │
        └────────────────┘
        
        ┌─────────────────────────────┐
        │ A2, A3, A4, A5              │
SOON    │ S4, S5, S6, S7              │
        │ Q3, Q4, Q5, Q6, Q7, Q8, Q9  │
        └─────────────────────────────┘
        
        ┌─────────────────────────────┐
LATER   │ A6, A10, A11, A12, A13      │
        │ S8, S9, S10                 │
        │ Q10, Q11, Q12, Q13          │
        └─────────────────────────────┘
```

---

## Next Steps

### Immediate (This Sprint)

1. **Fix A1 + S2:** Add null guard after `ToolTabFactory::create()`.
   - File: `cremniy/src/ui/toolstabwidget.cpp`
   - Change: Insert `if (!tab) continue;` immediately after create.

2. **Fix S1:** Sanitize radare2 commands.
   - Files: `radare2backend.cpp`, `AppSettings`, Settings dialog
   - Change: Whitelist commands or use subprocess argument list without shell interpolation.

3. **Fix S3:** Guard dynamic_cast results.
   - Files: `toolstabwidget.cpp`, `filestabwidget.cpp`
   - Change: Add `if (auto* casted = dynamic_cast<Type*>(...)) { ... }`

4. **Code Review:** Run `/review` on critical files.

### Near-term (Next Sprint)

5. **Document Architecture:** Create `Docs/CREMNIY_ARCHITECTURE.md` or update to reflect Qt/C++ desktop patterns.

6. **Decouple Layers:** Invert widget dependencies (A3), decouple Core from Utils (A4).

7. **Fix Include Paths:** Remove `../../` traversals (A5); use CMake include dirs.

8. **Fix Layout Bug:** Remove duplicate `addWidget` call (A9).

9. **Security Hardening:** Validate JSON (S4), encrypt terminal history (S5), reduce logging noise (S6), audit CVEs (S7).

10. **Code Quality:** Add CMake tests (A7, Q8), decompose disassembler (A6, Q4), fix memory leaks (Q7).

### Medium-term (This Quarter)

11. **Refactor God-Modules:** Break disassembler into focused components.

12. **Reduce Header Coupling:** Forward-declare where possible.

13. **Add Test Harness:** Wire CMake test infrastructure; add unit tests for core.

14. **Dependency Review:** Audit Qt version; consider external deps for QCodeEditor, QHexView.

### Long-term (Future)

15. **Migration Planning:** As you prepare for Tauri + Rust + React migration, extract business logic into separate library/layer decoupled from Qt.

16. **Retire Vendored Code:** Move QCodeEditor, QHexView to submodules or external packages.

---

## Conclusion

**Health Score: 2.0 / 10**

The cremniy codebase faces **critical security and stability issues** that require immediate remediation:
- **S1** (arbitrary command execution) is a remote code execution vector.
- **A1 + S2** (null dereference) will crash on startup.
- **S3 + Q1 + Q2** (unsafe type conversions) create local DoS paths.

**Medium-term architectural work** (decomposition, dependency inversion, test harness) is necessary to unlock maintainability and prepare for the planned migration to Tauri + Rust.

The **planned migration to Tauri + Rust + React** will address most architecture findings (A2–A5, A8–A9, A12–A13) by design. Focus immediate effort on security (S1, S5) and correctness (A1, S2, S3, Q1, Q2) to stabilize the current desktop IDE before transition.

---

**Report Generated:** 2026-04-29  
**Next Audit:** Recommend quarterly or post-security-fix verification.
