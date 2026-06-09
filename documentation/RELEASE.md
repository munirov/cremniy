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
   - Собирает приложение для Linux, Windows и macOS с помощью `npm run tauri:build`
   - Создаёт релиз на GitHub
   - Прикрепляет собранные файлы к релизу

5. **Проверьте релиз**
   
   Перейдите на страницу [Releases](https://github.com/munirov/cremniy/releases) и убедитесь, что релиз создан успешно.

### Структура артефактов

После сборки будут доступны следующие файлы из `source/backend/target/release/bundle/`:

| Платформа | Файлы |
|-----------|-------|
| Linux | `.AppImage` (AppImage), `.deb` (Debian/Ubuntu) |
| Windows | `.msi` (MSI), `.exe` (NSIS) |
| macOS | `.dmg` |

### Ручной запуск сборки

Вы также можете запустить сборку вручную без создания релиза:

1. Перейдите на вкладку [Actions](https://github.com/munirov/cremniy/actions)
2. Выберите workflow "Build and Release"
3. Нажмите "Run workflow"
4. Выберите ветку и нажмите "Run workflow"

Артефакты будут доступны в разделе "Artifacts" запущенного workflow.

### Continuous Integration

При push и Pull Request в ветки `main` или `dev` автоматически запускается CI: тесты и сборка фронтенда плюс `cargo check` бэкенда (на Linux) — проверка, что код в порядке.

---

## Automatic Release

The project is configured for automatic build and release through GitHub Actions.
Workflow file: [`.github/workflows/release.yml`](../.github/workflows/release.yml)

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
   - Build the application for Linux, Windows, and macOS using `npm run tauri:build`
   - Create a release on GitHub
   - Attach the built bundle files to the release

5. **Check the release**
   
   Go to the [Releases](https://github.com/munirov/cremniy/releases) page and verify the release was created successfully.

### Artifact structure

Build outputs from `source/backend/target/release/bundle/`:

| Platform | Files |
|----------|-------|
| Linux | `.AppImage`, `.deb` |
| Windows | `.msi`, `.exe` (NSIS) |
| macOS | `.dmg` |

### Manual workflow run

You can also run the build manually without creating a release:

1. Go to the [Actions](https://github.com/munirov/cremniy/actions) tab
2. Select the "Build and Release" workflow
3. Click "Run workflow"
4. Select a branch and click "Run workflow"

Artifacts will be available in the "Artifacts" section of the workflow run.

### Continuous Integration

Pushes and pull requests to `main` or `dev` trigger CI: the frontend tests and build plus a backend `cargo check` (on Linux) to verify the code stays healthy.
