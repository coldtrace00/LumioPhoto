#pragma once
// Параметри інсталятора для Lumio Photo (решта коду setup.cpp — спільна з Lumio).

#include <windows.h>

struct AppInfo {
    const wchar_t *name;
    const wchar_t *exe;
    const wchar_t *regKey;            // підрозділ Uninstall і назва в автозапуску
    const wchar_t *dirName;           // папка встановлення за замовчуванням
    const wchar_t *portableDirName;
    const wchar_t *version;
    const wchar_t *const *killNames;  // процеси, які треба закрити перед встановленням (nullptr у кінці)
    bool hasAutostart;
    const wchar_t *autostartArgs;
    const wchar_t *legacyRunName;     // стара назва в автозапуску, яку треба прибрати (або nullptr)
    COLOR16 grad[3][3];               // кольори смужки в шапці (як у лого)
    const wchar_t *taglineUk, *taglineEn;
    const wchar_t *descUk, *descEn;   // опис ярлика
    const wchar_t *portableHintUk, *portableHintEn;
    const char *portableFileText;
    bool settingsInLocalAppData;      // false — %APPDATA%, true — %LOCALAPPDATA%
    const wchar_t *settingsFile;      // відносно (Local)AppData
    const wchar_t *portableSettingsFile; // відносно папки програми
    const wchar_t *dataDirName;       // дані, які можна видалити разом з програмою (nullptr — немає)
    const wchar_t *deleteDataUk, *deleteDataEn;
    const wchar_t *keepUk, *keepEn;   // нагадування при видаленні
    bool hasThemeChoice;              // вибір світлої / темної теми в інсталяторі
    // реєстрація як програми для відкриття файлів (прибирається деінсталятором; nullptr — немає)
    const wchar_t *assocProgId;
    const wchar_t *const *assocExts;
    const wchar_t *registeredAppName;
    const wchar_t *const *cleanupKeys; // розділи HKCU, які видалити повністю
};

// Версія приходить з кореневого CMakeLists.txt (project(... VERSION x.y.z)) через згенерований version.h
#if __has_include("version.h")
#include "version.h"
#endif
#ifndef APP_VERSION_W
#define APP_VERSION_W L"0.0.0"
#endif

static const wchar_t *const kKillNames[] = {L"LumioPhoto.exe", nullptr};
static const wchar_t *const kAssocExts[] = {L".jpg", L".jpeg", L".jfif", L".png", L".bmp", L".gif", L".webp",
                                            L".tif", L".tiff", L".heic", L".heif", L".avif", nullptr};
static const wchar_t *const kCleanupKeys[] = {L"Software\\Classes\\LumioPhoto.Image",
                                              L"Software\\Classes\\Applications\\LumioPhoto.exe",
                                              L"Software\\LumioPhoto", nullptr};

static const AppInfo kApp = {
    L"Lumio Photo",
    L"LumioPhoto.exe",
    L"LumioPhoto",
    L"Lumio Photo",
    L"Lumio Photo Portable",
    APP_VERSION_W,
    kKillNames,
    false,
    L"",
    nullptr,
    {{0x2D00, 0xE000, 0xC000}, {0x3B00, 0x8200, 0xF600}, {0x8B00, 0x4B00, 0xFF00}},
    L"Ваші фото — швидко й приватно.",
    L"Your photos — fast and private.",
    L"Lumio Photo — ваші фото",
    L"Lumio Photo — your photos",
    L"Портативна версія: бібліотека, налаштування й мініатюри лежать в одній папці, її можна носити на флешці. "
    L"Нічого не записується в систему.",
    L"Portable version: the library, settings and thumbnails live in one folder that you can carry on a USB stick. "
    L"Nothing is written to the system.",
    "Lumio Photo portable mode: library database, settings and thumbnails are stored in the data folder next to LumioPhoto.exe\r\n",
    true,
    L"LumioPhoto\\settings.ini",
    L"data\\settings.ini",
    L"LumioPhoto",
    L"Видалити також бібліотеку Lumio Photo (зірочки, теги, альбоми, правки та мініатюри)?\n\n"
    L"Самі фото це не зачепить. Папку буде переміщено в кошик.",
    L"Also delete the Lumio Photo library (stars, tags, albums, edits and thumbnails)?\n\n"
    L"Your photos are not affected. The folder will be moved to the Recycle Bin.",
    L"Ваші фото залишаться на місці.",
    L"Your photos will stay where they are.",
    true,
    L"LumioPhoto.Image",
    kAssocExts,
    L"Lumio Photo",
    kCleanupKeys,
};
