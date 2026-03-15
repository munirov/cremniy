<div align="center">
  <img src="src/resources/icons/icon.png" width="250" alt="logo">
  <br>
  <h3>Cremniy</h3>
  <h6>Среда разработки для низкоуровневого программирования, объединяющая все низкоуровневые инструменты в одном приложении</h6>

[![License](https://img.shields.io/github/license/igmunv/cremniy?color=orange&style=flat-square)](LICENSE)
[![Contributions Welcome](https://img.shields.io/badge/Contributions-Welcome-brightgreen?style=flat-square)](CONTRIBUTING.md)
[![Community](https://img.shields.io/badge/Community-Telegram-blue?logo=telegram&style=flat-square)](https://t.me/cremniy_com)

[English](README.md) • Русский

</div>

<br>

## 📌 О проекте

**Cremniy** — среда разработки для низкоуровневого программирования.

Она объединяет инструменты работы с бинарными файлами, памятью и системным кодом в одном приложении.

### Проект ориентирован на

- разработчиков системного ПО
- reverse-инженеров
- специалистов по информационной безопасности
- разработчиков embedded-систем

## ✨ Возможности

### Текущие

- Редактор кода
- HEX-редактор

### Планируется

- Дизассемблер
- Отладчик
- Визуализация памяти

## 📦 Зависимости

| Зависимость | Мин. версия |
| ----------- | ----------- |
| **CMake**   | 3.16        |
| **Qt**      | 6.x         |
| **C++**     | 17          |

### Установка зависимостей

<details>
<summary><b>Windows</b></summary>

1. Установите [Qt 6](https://www.qt.io/download-qt-installer-oss) — при установке выберите компонент **Qt Widgets**.
2. Установите [CMake](https://cmake.org/download/) (≥ 3.16) или используйте тот, который идёт в комплекте с Qt.
3. Компилятор с поддержкой C++17: [Visual Studio 2019+](https://visualstudio.microsoft.com/) (MSVC) или [MinGW](https://www.mingw-w64.org/).

> [!TIP]
> При использовании Visual Studio убедитесь, что установлена рабочая нагрузка «Разработка классических приложений на C++».

</details>

<details>
<summary><b>Linux (Ubuntu / Debian)</b></summary>

```bash
sudo apt update
sudo apt install cmake g++ qt6-base-dev
```

Если пакет `qt6-base-dev` недоступен в вашем дистрибутиве, используйте [официальный установщик Qt](https://www.qt.io/download-qt-installer-oss).

</details>

<details>
<summary><b>macOS</b></summary>

С помощью [Homebrew](https://brew.sh/):

```bash
brew install cmake qt@6
```

</details>

## 🛠️ Сборка

```bash
git clone https://github.com/igmunv/cremniy.git
cd cremniy

mkdir build
cd build
cmake ../src

cmake --build .
```

Для сборки в режиме Release:

```bash
cmake ../src -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## 🤝 Контрибьюция

Контрибьюции **приветствуются**.

Все контрибьюторы будут добавлены в [ACKNOWLEDGEMENTS.md](ACKNOWLEDGEMENTS.md)
и указаны в конце каждого видеоролика на [YouTube-канале](https://www.youtube.com/@igmunv).
