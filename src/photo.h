#pragma once

#include <QString>
#include <QVector>
#include <QtGlobal>

// Один знімок у бібліотеці. Усе, що користувач додає (зірочка, теги, підпис,
// правки), зберігається ЛИШЕ в базі програми — у файли нічого не пишемо.
struct Photo {
    qint64 id = 0;
    QString path;    // повний шлях
    QString folder;  // папка, в якій лежить файл
    QString name;    // ім'я файлу
    qint64 size = 0;
    qint64 mtime = 0; // секунди від епохи
    qint64 taken = 0; // дата зйомки (EXIF) або mtime
    int width = 0;
    int height = 0;
    bool starred = false;
    QString tags;    // через кому
    QString caption;
    QString edits;   // JSON неруйнівних правок, "" — без змін
};

using PhotoList = QVector<Photo>;
