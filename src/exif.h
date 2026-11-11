#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>

// Мінімальний і обережний читач EXIF (лише ЧИТАННЯ, для показу інформації).
// Програма ніколи не записує метадані у файли.
struct ExifData {
    bool valid = false;
    int orientation = 1;
    QDateTime taken;
    QString make;
    QString model;
    QString lens;
    double exposure = 0; // секунди
    double fnumber = 0;
    double focal = 0;    // мм
    int iso = 0;
    bool hasGps = false;
    double latitude = 0;
    double longitude = 0;
};

namespace Exif {

// Читає EXIF з JPEG-файлу (дивиться лише на початок файлу).
ExifData read(const QString &path);
// Розбирає блок TIFF/EXIF (те, що йде після "Exif\0\0").
ExifData parseTiff(const QByteArray &tiff);

QString describeExposure(double seconds); // "1/250 с"

} // namespace Exif
