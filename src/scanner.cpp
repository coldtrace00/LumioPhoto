#include "scanner.h"

#include "exif.h"
#include "library.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHash>
#include <QImageReader>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

Scanner::Scanner(QObject *parent)
    : QThread(parent)
{
}

Scanner::~Scanner()
{
    cancel();
    wait();
}

QStringList Scanner::supportedSuffixes()
{
    static QStringList list;
    if (list.isEmpty()) {
        const QSet<QString> wanted = {QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
                                      QStringLiteral("bmp"), QStringLiteral("gif"), QStringLiteral("webp"),
                                      QStringLiteral("tif"), QStringLiteral("tiff"), QStringLiteral("heic"),
                                      QStringLiteral("heif"), QStringLiteral("avif"), QStringLiteral("jfif")};
        // лише ті формати, які реально вміє читати ця збірка Qt
        for (const QByteArray &f : QImageReader::supportedImageFormats()) {
            const QString s = QString::fromLatin1(f).toLower();
            if (wanted.contains(s))
                list << s;
        }
        if (list.contains(QStringLiteral("jpg")) && !list.contains(QStringLiteral("jfif")))
            list << QStringLiteral("jfif");
        list.removeDuplicates();
    }
    return list;
}

void Scanner::start(const QStringList &roots)
{
    if (isRunning())
        return;
    m_roots = roots;
    m_cancel = false;
    QThread::start(QThread::LowPriority);
}

void Scanner::run()
{
    const QString conn = QStringLiteral("scanner");
    int added = 0, updated = 0, removed = 0;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
        db.setDatabaseName(Library::dbPath());
        if (!db.open()) {
            emit finishedScan(0, 0, 0);
            return;
        }
        QSqlQuery(QStringLiteral("PRAGMA busy_timeout=10000"), db);

        // що вже є в базі
        struct Known {
            qint64 id;
            qint64 size;
            qint64 mtime;
        };
        QHash<QString, Known> known;
        {
            QSqlQuery q(db);
            q.setForwardOnly(true);
            q.exec(QStringLiteral("SELECT id, path, size, mtime FROM photos"));
            while (q.next())
                known.insert(q.value(1).toString(), {q.value(0).toLongLong(), q.value(2).toLongLong(), q.value(3).toLongLong()});
        }

        QStringList filters;
        for (const QString &s : supportedSuffixes())
            filters << QStringLiteral("*.") + s;

        QSqlQuery ins(db), upd(db);
        ins.prepare(QStringLiteral("INSERT OR IGNORE INTO photos(path, folder, name, size, mtime, taken, width, height) "
                                   "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
        upd.prepare(QStringLiteral("UPDATE photos SET size = ?, mtime = ?, taken = ?, width = ?, height = ? WHERE id = ?"));

        QSet<QString> seen;
        int found = 0, sinceCommit = 0;
        QString lastFolder;
        db.transaction();

        for (const QString &root : std::as_const(m_roots)) {
            if (m_cancel)
                break;
            if (!QFileInfo::exists(root))
                continue; // диск відключено — не видаляємо його фото з бібліотеки
            QDirIterator it(root, filters, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
            while (it.hasNext() && !m_cancel) {
                const QString path = QDir::cleanPath(it.next());
                const QFileInfo fi = it.fileInfo();
                // службові / приховані папки пропускаємо
                if (path.contains(QLatin1String("/.")) || path.contains(QLatin1String("/$RECYCLE.BIN"), Qt::CaseInsensitive))
                    continue;
                seen.insert(path);
                ++found;
                const QString folder = fi.absolutePath();
                if (folder != lastFolder || found % 200 == 0) {
                    lastFolder = folder;
                    emit progress(found, folder);
                }

                const qint64 size = fi.size();
                const qint64 mtime = fi.lastModified().toSecsSinceEpoch();
                const auto k = known.constFind(path);
                if (k != known.constEnd() && k->size == size && k->mtime == mtime)
                    continue; // без змін

                QImageReader reader(path);
                reader.setAutoTransform(true);
                QSize dim = reader.size();
                if (reader.transformation() & QImageIOHandler::TransformationRotate90)
                    dim.transpose();
                qint64 taken = mtime;
                const QString suffix = fi.suffix().toLower();
                if (suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg") || suffix == QLatin1String("jfif")) {
                    const ExifData ex = Exif::read(path);
                    if (ex.taken.isValid())
                        taken = ex.taken.toSecsSinceEpoch();
                }

                if (k == known.constEnd()) {
                    ins.addBindValue(path);
                    ins.addBindValue(folder);
                    ins.addBindValue(fi.fileName());
                    ins.addBindValue(size);
                    ins.addBindValue(mtime);
                    ins.addBindValue(taken);
                    ins.addBindValue(dim.width());
                    ins.addBindValue(dim.height());
                    if (ins.exec())
                        ++added;
                } else {
                    upd.addBindValue(size);
                    upd.addBindValue(mtime);
                    upd.addBindValue(taken);
                    upd.addBindValue(dim.width());
                    upd.addBindValue(dim.height());
                    upd.addBindValue(k->id);
                    if (upd.exec())
                        ++updated;
                }
                if (++sinceCommit >= 300) {
                    db.commit();
                    db.transaction();
                    sinceCommit = 0;
                }
            }
        }

        // Файли, яких більше немає (лише в доступних папках бібліотеки)
        if (!m_cancel) {
            QSqlQuery del(db);
            del.prepare(QStringLiteral("DELETE FROM photos WHERE id = ?"));
            for (auto it = known.constBegin(); it != known.constEnd(); ++it) {
                if (seen.contains(it.key()))
                    continue;
                bool underAvailableRoot = false, underAnyRoot = false;
                for (const QString &root : std::as_const(m_roots)) {
                    if (it.key().startsWith(root + QLatin1Char('/'))) {
                        underAnyRoot = true;
                        underAvailableRoot = QFileInfo::exists(root);
                        break;
                    }
                }
                // Файл зник з доступної папки. Фото поза бібліотекою (відкриті з Провідника)
                // зберігаємо, поки файл існує, — щоб не губити їхні зірочки й правки.
                // Прибрані з бібліотеки папки чистить Library::removeRoot.
                if ((underAnyRoot && underAvailableRoot && !QFileInfo::exists(it.key()))
                    || (!underAnyRoot && QFileInfo::exists(QFileInfo(it.key()).absolutePath()) && !QFileInfo::exists(it.key()))) {
                    del.addBindValue(it->id);
                    if (del.exec())
                        ++removed;
                }
            }
        }
        db.commit();
        db.close();
    }
    QSqlDatabase::removeDatabase(conn);
    emit finishedScan(added, updated, removed);
}
