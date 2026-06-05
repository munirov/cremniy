/**
 * Tiny in-process i18n. Two locales (en/ru), no dependencies, no async load —
 * the dictionary is bundled. Extend the `TRANSLATIONS` map as more strings
 * get internationalised; an unknown key falls back to the English value or
 * the key itself.
 *
 * Why DIY instead of i18next/react-intl? Cremniy ships in a single binary,
 * has ~50 user-visible strings, and Qt's i18n was a one-file `.ts` lookup —
 * matching scope.
 */

export type LocaleId = 'en' | 'ru';

export const LOCALES: readonly { id: LocaleId; label: string }[] = [
  { id: 'en', label: 'English' },
  { id: 'ru', label: 'Русский' },
];

type Dictionary = Readonly<Record<string, string>>;

const EN: Dictionary = {
  'menu.file': 'File',
  'menu.edit': 'Edit',
  'menu.view': 'View',
  'menu.build': 'Build',
  'menu.tools': 'Tools',
  'menu.references': 'References',
  'menu.terminal': 'Terminal',
  'menu.help': 'Help',
  'file.newProject': 'New project…',
  'file.openFolder': 'Open folder…',
  'file.openFile': 'Open file…',
  'file.save': 'Save',
  'file.saveAs': 'Save as…',
  'file.preferences': 'Preferences…',
  'file.closeEditorTab': 'Close editor',
  'file.closeWorkspace': 'Close workspace',
  'view.fileTree': 'File tree',
  'view.terminal': 'Terminal panel',
  'view.wordWrap': 'Word wrap',
  'view.insertSpaces': 'Insert spaces instead of tab',
  'common.close': 'Close',
  'common.cancel': 'Cancel',
  'common.save': 'Save',
  'common.copy': 'Copy',
  'common.delete': 'Delete',
  'common.rename': 'Rename',
  'settings.language': 'Language',
};

const RU: Dictionary = {
  'menu.file': 'Файл',
  'menu.edit': 'Правка',
  'menu.view': 'Вид',
  'menu.build': 'Сборка',
  'menu.tools': 'Инструменты',
  'menu.references': 'Справочники',
  'menu.terminal': 'Терминал',
  'menu.help': 'Справка',
  'file.newProject': 'Новый проект…',
  'file.openFolder': 'Открыть папку…',
  'file.openFile': 'Открыть файл…',
  'file.save': 'Сохранить',
  'file.saveAs': 'Сохранить как…',
  'file.preferences': 'Настройки…',
  'file.closeEditorTab': 'Закрыть редактор',
  'file.closeWorkspace': 'Закрыть проект',
  'view.fileTree': 'Дерево файлов',
  'view.terminal': 'Терминал',
  'view.wordWrap': 'Перенос строк',
  'view.insertSpaces': 'Пробелы вместо табов',
  'common.close': 'Закрыть',
  'common.cancel': 'Отмена',
  'common.save': 'Сохранить',
  'common.copy': 'Копировать',
  'common.delete': 'Удалить',
  'common.rename': 'Переименовать',
  'settings.language': 'Язык',
};

const TRANSLATIONS: Readonly<Record<LocaleId, Dictionary>> = { en: EN, ru: RU };

export function translate(key: string, locale: LocaleId): string {
  const dict = TRANSLATIONS[locale];
  return dict[key] ?? EN[key] ?? key;
}
