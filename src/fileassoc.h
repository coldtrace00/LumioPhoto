#pragma once

#include <QStringList>

// Реєстрація Lumio Photo у Windows як програми для відкриття фото.
// Усе пишеться лише в HKEY_CURRENT_USER (без прав адміністратора);
// інсталятор/деінсталятор прибирає ці ключі.
namespace FileAssoc {

QStringList extensions();     // ".jpg", ".png", …
bool isSupported();           // лише Windows
void registerApp();           // ProgID, «Відкрити за допомогою», «Програми за замовчуванням»
void ensureRegistered();      // при запуску: оновити, якщо програму перенесли
void openDefaultAppsSettings(); // відкрити налаштування Windows для вибору програми за замовчуванням

} // namespace FileAssoc
