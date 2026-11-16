# Developer Guide

The project is being rebuilt into a modular architecture.

Currently, modularity is implemented only for **Tool Tabs**. 


## Project structure


- `app/` - main windows for display
- `core/` - base interfaces
- `ToolTabs/` - modules: tool tabs
- `widgets/` - reusable widgets
- `utils/` - helper code



## Tool Tab for working with a file

### Description

A **ToolTab** is a tool tab for the user to work with a file. Examples of **ToolTab**:
- **Code Editor**<br>
<img width="400" height="300" alt="code_tooltab" src="https://github.com/user-attachments/assets/f4d9d9c5-acbb-42ee-a3e1-a2d773c94d89" /><br>
- **HEX Editor**<br>
<img width="400" height="300" alt="hex_tooltab" src="https://github.com/user-attachments/assets/cb66ffa9-1852-4200-809e-77ece6687d32" /><br>
- **Disassembler**<br>
<img width="400" height="300" alt="dasm_tooltab" src="https://github.com/user-attachments/assets/d72c642b-9b02-4dc8-8f03-0224490b4d3e" /><br>

Each **ToolTab** is an independent module and is registered through the **ToolTabFactory** component.

### Operation of tool tabs

- All **ToolTab** inherit from the `ToolTab` class
- Each **ToolTab** is registered in `ToolTabFactory`
- `ToolTabWidget` automatically loads all **ToolTab** through `ToolTabFactory`

```
ToolTab (interface)
    ↓
Module (CodeEditorTab, HexEditorTab, ...)
    ↓
registerTab()
    ↓
ToolTabFactory
    ↓
ToolTabWidget → create() → addTab()
```

Registration occurs automatically when the application starts via `static initialization` (global static objects).

### Adding a tool tab

1. Create a new directory in `ToolTabs/` for the tool
2. Create a class inheriting `ToolTab`
3. Implement `toolName()` and `toolIcon()`
4. Register the tool in `ToolTabFactory`
5. Add `CMakeLists.txt`

### Rules

- Modules **must not depend** on each other directly
- It is forbidden to use includes like `../../` or `include/`
- All dependencies are connected **through CMake**
- ToolTab is the **only** interaction point with the tabs

## Module settings pages

Modules may add a settings page without modifying `SettingsDialog`. Modules that
do not need settings simply do not register one. A page inherits `SettingsPage`
and implements this lifecycle: `load()` reads saved values, `validate()` checks
pending input without saving, and `apply()` persists it after the user accepts.

Register the page from the module's source file:

```cpp
SettingsRegistry::instance().registerModulePage("example", {
    "modules.example",
    "modules",
    []() { return QObject::tr("Modules"); },
    300,
    []() { return QCoreApplication::translate("ExampleModule", "Example"); },
    100,
    {},
    [](QWidget* parent) { return new ExampleSettingsPage(parent); }
});
```

Keep `pageId` and the module owner ID stable and independent of translations.
Do not write settings from widget signals: writing only in `apply()` ensures
that Cancel leaves persisted settings unchanged.
