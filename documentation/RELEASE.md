# Release Guide

## Автоматический релиз

Проект настроен на автоматическую сборку и публикацию релизов через GitHub Actions.

### Как создать новый релиз

1. **Убедитесь, что все изменения закоммичены**
   ```bash
   git status
   ```

2. **Создайте тег версии**
   ```bash
   git tag v0.1.0
   ```
   
   Формат тега: `v<major>.<minor>.<patch>` (например, `v0.1.0`, `v1.0.0`, `v1.2.3`)

3. **Отправьте тег на GitHub**
   ```bash
   git push origin v0.1.0
   ```

4. **Автоматическая сборка**
   
   GitHub Actions автоматически:
   - Соберет приложение для Linux, Windows и macOS
   - Упакует бинарные файлы
   - Создаст релиз на GitHub
   - Прикрепит собранные файлы к релизу

5. **Проверьте релиз**
   
   Перейдите на страницу [Releases](https://github.com/igmunv/cremniy/releases) и убедитесь, что релиз создан успешно.

### Структура артефактов

После сборки будут доступны следующие файлы:

- `cremniy-linux.tar.gz` - сборка для Linux (tar.gz архив)
- `cremniy-linux.AppImage` - сборка для Linux (AppImage, запускается на любом дистрибутиве)
- `cremniy-windows.zip` - сборка для Windows (включает все необходимые DLL)
- `cremniy-macos.tar.gz` - сборка для macOS

### Ручной запуск сборки

Вы также можете запустить сборку вручную без создания релиза:

1. Перейдите на вкладку [Actions](https://github.com/igmunv/cremniy/actions)
2. Выберите workflow "Build and Release"
3. Нажмите "Run workflow"
4. Выберите ветку и нажмите "Run workflow"

Артефакты будут доступны в разделе "Artifacts" запущенного workflow.

### Continuous Integration

При каждом push в ветки `main`, `master` или `develop`, а также при создании Pull Request, автоматически запускается CI сборка для проверки, что код компилируется на всех платформах.

---

## Automatic Release

The project is configured for automatic build and release through GitHub Actions.

### How to create a new release

1. **Make sure all changes are committed**
   ```bash
   git status
   ```

2. **Create a version tag**
   ```bash
   git tag v0.1.0
   ```
   
   Tag format: `v<major>.<minor>.<patch>` (e.g., `v0.1.0`, `v1.0.0`, `v1.2.3`)

3. **Push the tag to GitHub**
   ```bash
   git push origin v0.1.0
   ```

4. **Automatic build**
   
   GitHub Actions will automatically:
   - Build the application for Linux, Windows, and macOS
   - Package the binaries
   - Create a release on GitHub
   - Attach the built files to the release

5. **Check the release**
   
   Go to the [Releases](https://github.com/igmunv/cremniy/releases) page and verify the release was created successfully.

### Artifact structure

After building, the following files will be available:

- `cremniy-linux.tar.gz` - Linux build
- `cremniy-windows.zip` - Windows build (includes all necessary DLLs)
- `cremniy-macos.tar.gz` - macOS build

### Manual workflow run

You can also run the build manually without creating a release:

1. Go to the [Actions](https://github.com/igmunv/cremniy/actions) tab
2. Select the "Build and Release" workflow
3. Click "Run workflow"
4. Select a branch and click "Run workflow"

Artifacts will be available in the "Artifacts" section of the workflow run.

### Continuous Integration

On every push to `main`, `master`, or `develop` branches, as well as on Pull Request creation, a CI build is automatically triggered to verify that the code compiles on all platforms. 
