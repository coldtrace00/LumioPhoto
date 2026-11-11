#pragma once

#include <QImage>
#include <QString>

namespace Util {

QString humanSize(qint64 bytes);
// «Природне» порівняння імен, як у Провіднику: IMG_2 < IMG_10
bool naturalLess(const QString &a, const QString &b);
bool moveToTrash(const QString &path, QString *error = nullptr);
void showInFolder(const QString &path);
void copyImage(const QImage &img);
bool setWallpaper(const QImage &img, QString *error = nullptr);

// Записати зображення БЕЗ жодних метаданих (EXIF, GPS, коментарів тощо) —
// у файл потрапляють лише пікселі. Формат — за розширенням.
bool saveImage(const QImage &img, const QString &path, int jpegQuality = 92, QString *error = nullptr);

// Резервна копія оригіналу перед «Зберегти у файл» (у папці програми, не поруч з фото)
QString backupPathFor(const QString &photoPath);
bool hasBackup(const QString &photoPath);

} // namespace Util
