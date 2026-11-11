#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>

// Лого й усі іконки малюються кодом — жодних сторонніх файлів-картинок.
namespace Icons {

enum Id {
    FolderAdd, Refresh, Star, StarFilled, RotateLeft, RotateRight, Export, Album, Slideshow, Copy, Trash,
    Info, Crop, Straighten, Magic, Undo, Back, Prev, Next, ZoomFit, Zoom100, Fullscreen, Edit, Save,
    Print, Wallpaper, Search, Plus, Gear, Photos, Folder, Clock, Close, Play, Pause, Open, Reset
};

QIcon icon(Id id, const QColor &color = QColor()); // порожній колір — колір поточної теми
QPixmap pixmap(Id id, int size, const QColor &color);
QPixmap appPixmap(int size);
QIcon appIcon();

} // namespace Icons
