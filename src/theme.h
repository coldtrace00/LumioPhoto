#pragma once

#include <QColor>

class QApplication;

// Дві теми: темна (за замовчуванням) і світла. Вибір — в інсталяторі або в меню ⚙
// (діє після перезапуску).
namespace Theme {
void apply(QApplication &app);
bool isDark();
void setDark(bool dark);
QColor iconColor();   // колір іконок інтерфейсу
QColor canvasColor(); // фон під фото в переглядачі
} // namespace Theme
