#pragma once

#include <QString>

// Двомовний інтерфейс: українська та англійська.
// Мова вибирається при першому запуску за налаштуваннями Windows
// і змінюється в меню (після перезапуску програми).
namespace I18n {
bool isUkrainian();
void setUkrainian(bool on); // зберігається, діє після перезапуску
} // namespace I18n

// Рядок інтерфейсу в обох мовах: tr2("Українською", "In English")
inline QString tr2(const char *uk, const char *en)
{
    return QString::fromUtf8(I18n::isUkrainian() ? uk : en);
}
