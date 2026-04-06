# Developer Guide

Проект перестраивается на модульную архитектуру.

На данный момент модульность реализована только для **вкладок инструментов** (**Tool Tabs**). 


## Структура проекта


- `app/` - главные окна для отображения
- `core/` - базовые интерфейсы
- `ToolTabs/` - модули: вкладки инструментов
- `widgets/` - переиспользуемые виджеты
- `utils/` - вспомогательный код



## Вкладка инструмента для работы с файлом (Tool Tab)

### Описание

**ToolTab** - это вкладка инструмента для работы пользователя с файлом. Примеры **ToolTab**:
- **Редактор кода**<br>
<img width="400" height="300" alt="code_tooltab" src="https://github.com/user-attachments/assets/f4d9d9c5-acbb-42ee-a3e1-a2d773c94d89" /><br>
- **HEX-редактор**<br>
<img width="400" height="300" alt="hex_tooltab" src="https://github.com/user-attachments/assets/cb66ffa9-1852-4200-809e-77ece6687d32" /><br>
- **Дизассемблер**<br>
<img width="400" height="300" alt="dasm_tooltab" src="https://github.com/user-attachments/assets/d72c642b-9b02-4dc8-8f03-0224490b4d3e" /><br>

Каждый **ToolTab** является независимым модулем и регистрируется через компонент **ToolTabFactory**.

### Работа вкладок инструментов

- Все **ToolTab** наследуются от класса `ToolTab`
- Каждая **ToolTab** регистрируется в `ToolTabFactory`
- `ToolTabWidget` автоматически загружает все **ToolTab** через `ToolTabFactory`

```
ToolTab (интерфейс)
    ↓
Модуль (CodeEditorTab, HexEditorTab, ...)
    ↓
registerTab()
    ↓
ToolTabFactory
    ↓
ToolTabWidget → create() → addTab()
```

Регистрация происходит автоматически при запуске приложения через `static initialization` (глобальные статические объекты).

### Добавление вкладки инструмента

1. Создать новую директорию в `ToolTabs/` для инструмента
2. Создать класс, наследующий `ToolTab`
3. Реализовать `toolName()` и `toolIcon()`
4. Зарегистрировать инструмент в `ToolTabFactory`
5. Добавить `CMakeLists.txt`

### Правила

- Модули **не должны зависеть** друг от друга напрямую
- Запрещено использовать include вида `../../` или `include/`
- Все зависимости подключаются **через CMake**
- ToolTab - **единственная** точка взаимодействия с вкладками
