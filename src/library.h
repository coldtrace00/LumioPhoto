#pragma once

#include "photo.h"

#include <QSet>
#include <QStringList>

class QSettings;

// Де програма тримає свої дані. Звичайний режим: %LOCALAPPDATA%\LumioPhoto,
// портативний (поруч з .exe лежить portable.txt): папка data поруч із програмою.
namespace AppPaths {
bool isPortable();
QString dataDir();
QString thumbsDir();
QString originalsDir(); // резервні копії оригіналів перед «Зберегти у файл»
QSettings &settings();
} // namespace AppPaths

// Бібліотека фото в локальній базі SQLite. Нічого не відправляється в мережу
// і нічого не записується у файли фотографій.
namespace Library {

struct Album {
    qint64 id = 0;
    QString name;
    int count = 0;
};

bool open(QString *error = nullptr);
QString dbPath();

QStringList roots();
void addRoot(const QString &path);
void removeRoot(const QString &path); // прибирає з бібліотеки (файли не чіпає)

PhotoList loadAll();
// Фото поза папками бібліотеки (відкриті з Провідника) теж мають запис у базі,
// щоб зірочки й правки працювали, але в переглядах бібліотеки їх не видно.
bool isUnderRoots(const QString &path, const QStringList &roots);
// Повертає записи для цих файлів (у тому ж порядку), за потреби додає їх у базу.
PhotoList ensurePhotos(const QStringList &paths);
void setStarred(const QVector<qint64> &ids, bool starred);
void setTags(qint64 id, const QString &tags);
void setCaption(qint64 id, const QString &caption);
void setEdits(qint64 id, const QString &editsJson);
void updateFileInfo(qint64 id, qint64 size, qint64 mtime, int width, int height);
void remove(qint64 id);
void rename(qint64 id, const QString &newPath);

QVector<Album> albums();
qint64 createAlbum(const QString &name);
void renameAlbum(qint64 id, const QString &name);
void deleteAlbum(qint64 id);
void addToAlbum(qint64 album, const QVector<qint64> &ids);
void removeFromAlbum(qint64 album, const QVector<qint64> &ids);
QSet<qint64> albumPhotos(qint64 album);
QStringList albumsOf(qint64 photoId);

} // namespace Library
