#include "library.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QDateTime>
#include <QHash>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>

#include "exif.h"

namespace AppPaths {

bool isPortable()
{
    return QFile::exists(QCoreApplication::applicationDirPath() + QStringLiteral("/portable.txt"));
}

QString dataDir()
{
    static QString dir;
    if (dir.isEmpty()) {
        if (isPortable())
            dir = QCoreApplication::applicationDirPath() + QStringLiteral("/data");
        else
            dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/LumioPhoto");
        QDir().mkpath(dir);
    }
    return dir;
}

QString thumbsDir()
{
    const QString d = dataDir() + QStringLiteral("/thumbs");
    QDir().mkpath(d);
    return d;
}

QString originalsDir()
{
    const QString d = dataDir() + QStringLiteral("/originals");
    QDir().mkpath(d);
    return d;
}

QSettings &settings()
{
    static QSettings *s = new QSettings(dataDir() + QStringLiteral("/settings.ini"), QSettings::IniFormat);
    return *s;
}

} // namespace AppPaths

namespace Library {

namespace {

bool exec(QSqlQuery &q)
{
    if (!q.exec()) {
        qWarning("SQL error: %s", qPrintable(q.lastError().text()));
        return false;
    }
    return true;
}

bool execSql(const QString &sql)
{
    QSqlQuery q;
    if (!q.exec(sql)) {
        qWarning("SQL error: %s (%s)", qPrintable(q.lastError().text()), qPrintable(sql));
        return false;
    }
    return true;
}

QString idList(const QVector<qint64> &ids)
{
    QStringList parts;
    parts.reserve(ids.size());
    for (qint64 id : ids)
        parts << QString::number(id);
    return parts.join(QLatin1Char(','));
}

} // namespace

QString dbPath()
{
    return AppPaths::dataDir() + QStringLiteral("/library.db");
}

bool open(QString *error)
{
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    db.setDatabaseName(dbPath());
    if (!db.open()) {
        if (error)
            *error = db.lastError().text();
        return false;
    }
    execSql(QStringLiteral("PRAGMA journal_mode=WAL"));
    execSql(QStringLiteral("PRAGMA synchronous=NORMAL"));
    execSql(QStringLiteral("PRAGMA busy_timeout=5000"));
    execSql(QStringLiteral("PRAGMA foreign_keys=ON"));
    execSql(QStringLiteral("CREATE TABLE IF NOT EXISTS roots (path TEXT PRIMARY KEY)"));
    execSql(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS photos ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " path TEXT UNIQUE NOT NULL, folder TEXT NOT NULL, name TEXT NOT NULL,"
        " size INTEGER DEFAULT 0, mtime INTEGER DEFAULT 0, taken INTEGER DEFAULT 0,"
        " width INTEGER DEFAULT 0, height INTEGER DEFAULT 0,"
        " starred INTEGER DEFAULT 0, tags TEXT DEFAULT '', caption TEXT DEFAULT '', edits TEXT DEFAULT '')"));
    execSql(QStringLiteral("CREATE INDEX IF NOT EXISTS photos_folder ON photos(folder)"));
    execSql(QStringLiteral("CREATE TABLE IF NOT EXISTS albums (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL)"));
    execSql(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS album_items ("
        " album INTEGER NOT NULL REFERENCES albums(id) ON DELETE CASCADE,"
        " photo INTEGER NOT NULL REFERENCES photos(id) ON DELETE CASCADE,"
        " PRIMARY KEY (album, photo))"));
    return true;
}

QStringList roots()
{
    QStringList out;
    QSqlQuery q(QStringLiteral("SELECT path FROM roots ORDER BY path"));
    while (q.next())
        out << q.value(0).toString();
    return out;
}

void addRoot(const QString &path)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("INSERT OR IGNORE INTO roots(path) VALUES (?)"));
    q.addBindValue(QDir::cleanPath(QDir::fromNativeSeparators(path)));
    exec(q);
}

void removeRoot(const QString &path)
{
    const QString p = QDir::cleanPath(QDir::fromNativeSeparators(path));
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM roots WHERE path = ?"));
    q.addBindValue(p);
    exec(q);
    QSqlQuery d;
    d.prepare(QStringLiteral("DELETE FROM photos WHERE folder = ? OR folder LIKE ? ESCAPE '\\'"));
    QString like = p;
    like.replace(QLatin1String("\\"), QLatin1String("\\\\")).replace(QLatin1String("%"), QLatin1String("\\%"))
        .replace(QLatin1String("_"), QLatin1String("\\_"));
    d.addBindValue(p);
    d.addBindValue(like + QStringLiteral("/%"));
    exec(d);
}

PhotoList loadAll()
{
    PhotoList out;
    QSqlQuery q;
    q.setForwardOnly(true);
    q.exec(QStringLiteral("SELECT id, path, folder, name, size, mtime, taken, width, height, starred, tags, caption, edits "
                          "FROM photos"));
    while (q.next()) {
        Photo p;
        p.id = q.value(0).toLongLong();
        p.path = q.value(1).toString();
        p.folder = q.value(2).toString();
        p.name = q.value(3).toString();
        p.size = q.value(4).toLongLong();
        p.mtime = q.value(5).toLongLong();
        p.taken = q.value(6).toLongLong();
        p.width = q.value(7).toInt();
        p.height = q.value(8).toInt();
        p.starred = q.value(9).toInt() != 0;
        p.tags = q.value(10).toString();
        p.caption = q.value(11).toString();
        p.edits = q.value(12).toString();
        out.append(p);
    }
    return out;
}

namespace {

Qt::CaseSensitivity pathCase()
{
#ifdef Q_OS_WIN
    return Qt::CaseInsensitive;
#else
    return Qt::CaseSensitive;
#endif
}

QString pathKey(const QString &p)
{
    return pathCase() == Qt::CaseInsensitive ? p.toLower() : p;
}

Photo photoFromQuery(const QSqlQuery &q)
{
    Photo p;
    p.id = q.value(0).toLongLong();
    p.path = q.value(1).toString();
    p.folder = q.value(2).toString();
    p.name = q.value(3).toString();
    p.size = q.value(4).toLongLong();
    p.mtime = q.value(5).toLongLong();
    p.taken = q.value(6).toLongLong();
    p.width = q.value(7).toInt();
    p.height = q.value(8).toInt();
    p.starred = q.value(9).toInt() != 0;
    p.tags = q.value(10).toString();
    p.caption = q.value(11).toString();
    p.edits = q.value(12).toString();
    return p;
}

const char *kPhotoColumns = "id, path, folder, name, size, mtime, taken, width, height, starred, tags, caption, edits";

} // namespace

bool isUnderRoots(const QString &path, const QStringList &roots)
{
    for (const QString &r : roots) {
        const QString prefix = r.endsWith(QLatin1Char('/')) ? r : r + QLatin1Char('/');
        if (path.startsWith(prefix, pathCase()))
            return true;
    }
    return false;
}

PhotoList ensurePhotos(const QStringList &paths)
{
    PhotoList out;
    if (paths.isEmpty())
        return out;

    // усе, що вже є в базі для цих папок
    QHash<QString, Photo> known;
    QSet<QString> folders;
    for (const QString &p : paths)
        folders.insert(QFileInfo(p).absolutePath());
    for (const QString &folder : std::as_const(folders)) {
        QSqlQuery q;
        q.setForwardOnly(true);
        q.prepare(QStringLiteral("SELECT %1 FROM photos WHERE folder = ?%2")
                      .arg(QLatin1String(kPhotoColumns),
                           pathCase() == Qt::CaseInsensitive ? QStringLiteral(" COLLATE NOCASE") : QString()));
        q.addBindValue(folder);
        if (exec(q))
            while (q.next()) {
                const Photo p = photoFromQuery(q);
                known.insert(pathKey(p.path), p);
            }
    }

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();
    QSqlQuery ins;
    ins.prepare(QStringLiteral("INSERT OR IGNORE INTO photos(path, folder, name, size, mtime, taken, width, height) "
                               "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    for (const QString &raw : paths) {
        const QString path = QDir::cleanPath(QDir::fromNativeSeparators(raw));
        const auto k = known.constFind(pathKey(path));
        if (k != known.constEnd()) {
            out.append(*k);
            continue;
        }
        const QFileInfo fi(path);
        Photo p;
        p.path = path;
        p.folder = fi.absolutePath();
        p.name = fi.fileName();
        p.size = fi.size();
        p.mtime = fi.lastModified().toSecsSinceEpoch();
        p.taken = p.mtime;
        QImageReader reader(path);
        reader.setAutoTransform(true);
        QSize dim = reader.size();
        if (reader.transformation() & QImageIOHandler::TransformationRotate90)
            dim.transpose();
        p.width = dim.width();
        p.height = dim.height();
        const QString suffix = fi.suffix().toLower();
        if (suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg") || suffix == QLatin1String("jfif")) {
            const ExifData ex = Exif::read(path);
            if (ex.taken.isValid())
                p.taken = ex.taken.toSecsSinceEpoch();
        }
        ins.addBindValue(p.path);
        ins.addBindValue(p.folder);
        ins.addBindValue(p.name);
        ins.addBindValue(p.size);
        ins.addBindValue(p.mtime);
        ins.addBindValue(p.taken);
        ins.addBindValue(p.width);
        ins.addBindValue(p.height);
        if (exec(ins))
            p.id = ins.lastInsertId().toLongLong();
        if (p.id <= 0) { // запис уже був (інший регістр у шляху тощо)
            QSqlQuery q;
            q.prepare(QStringLiteral("SELECT %1 FROM photos WHERE path = ?").arg(QLatin1String(kPhotoColumns)));
            q.addBindValue(p.path);
            if (exec(q) && q.next())
                p = photoFromQuery(q);
        }
        if (p.id > 0)
            out.append(p);
    }
    db.commit();
    return out;
}

void setStarred(const QVector<qint64> &ids, bool starred)
{
    if (ids.isEmpty())
        return;
    execSql(QStringLiteral("UPDATE photos SET starred = %1 WHERE id IN (%2)").arg(starred ? 1 : 0).arg(idList(ids)));
}

static void setText(qint64 id, const char *column, const QString &value)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("UPDATE photos SET %1 = ? WHERE id = ?").arg(QLatin1String(column)));
    q.addBindValue(value);
    q.addBindValue(id);
    exec(q);
}

void setTags(qint64 id, const QString &tags) { setText(id, "tags", tags); }
void setCaption(qint64 id, const QString &caption) { setText(id, "caption", caption); }
void setEdits(qint64 id, const QString &edits) { setText(id, "edits", edits); }

void updateFileInfo(qint64 id, qint64 size, qint64 mtime, int width, int height)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("UPDATE photos SET size = ?, mtime = ?, width = ?, height = ? WHERE id = ?"));
    q.addBindValue(size);
    q.addBindValue(mtime);
    q.addBindValue(width);
    q.addBindValue(height);
    q.addBindValue(id);
    exec(q);
}

void remove(qint64 id)
{
    execSql(QStringLiteral("DELETE FROM photos WHERE id = %1").arg(id));
}

void rename(qint64 id, const QString &newPath)
{
    const QFileInfo fi(newPath);
    QSqlQuery q;
    q.prepare(QStringLiteral("UPDATE photos SET path = ?, folder = ?, name = ? WHERE id = ?"));
    q.addBindValue(fi.absoluteFilePath());
    q.addBindValue(fi.absolutePath());
    q.addBindValue(fi.fileName());
    q.addBindValue(id);
    exec(q);
}

QVector<Album> albums()
{
    QVector<Album> out;
    QSqlQuery q(QStringLiteral("SELECT a.id, a.name, COUNT(i.photo) FROM albums a "
                               "LEFT JOIN album_items i ON i.album = a.id GROUP BY a.id ORDER BY a.name COLLATE NOCASE"));
    while (q.next())
        out.append({q.value(0).toLongLong(), q.value(1).toString(), q.value(2).toInt()});
    return out;
}

qint64 createAlbum(const QString &name)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("INSERT INTO albums(name) VALUES (?)"));
    q.addBindValue(name);
    if (!exec(q))
        return 0;
    return q.lastInsertId().toLongLong();
}

void renameAlbum(qint64 id, const QString &name)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("UPDATE albums SET name = ? WHERE id = ?"));
    q.addBindValue(name);
    q.addBindValue(id);
    exec(q);
}

void deleteAlbum(qint64 id)
{
    execSql(QStringLiteral("DELETE FROM album_items WHERE album = %1").arg(id));
    execSql(QStringLiteral("DELETE FROM albums WHERE id = %1").arg(id));
}

void addToAlbum(qint64 album, const QVector<qint64> &ids)
{
    QSqlDatabase::database().transaction();
    QSqlQuery q;
    q.prepare(QStringLiteral("INSERT OR IGNORE INTO album_items(album, photo) VALUES (?, ?)"));
    for (qint64 id : ids) {
        q.addBindValue(album);
        q.addBindValue(id);
        exec(q);
    }
    QSqlDatabase::database().commit();
}

void removeFromAlbum(qint64 album, const QVector<qint64> &ids)
{
    if (ids.isEmpty())
        return;
    execSql(QStringLiteral("DELETE FROM album_items WHERE album = %1 AND photo IN (%2)").arg(album).arg(idList(ids)));
}

QSet<qint64> albumPhotos(qint64 album)
{
    QSet<qint64> out;
    QSqlQuery q(QStringLiteral("SELECT photo FROM album_items WHERE album = %1").arg(album));
    while (q.next())
        out.insert(q.value(0).toLongLong());
    return out;
}

QStringList albumsOf(qint64 photoId)
{
    QStringList out;
    QSqlQuery q(QStringLiteral("SELECT a.name FROM albums a JOIN album_items i ON i.album = a.id "
                               "WHERE i.photo = %1 ORDER BY a.name COLLATE NOCASE").arg(photoId));
    while (q.next())
        out << q.value(0).toString();
    return out;
}

} // namespace Library
