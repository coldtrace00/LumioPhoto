#include "thumbcache.h"

#include "edits.h"
#include "library.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QImageWriter>
#include <QPointer>
#include <QRunnable>
#include <QThread>

ThumbCache *ThumbCache::instance()
{
    static ThumbCache *c = new ThumbCache(QCoreApplication::instance());
    return c;
}

ThumbCache::ThumbCache(QObject *parent)
    : QObject(parent)
{
    m_memory.setMaxCost(320 * 1024); // ~320 МБ у КБ
    m_pool.setMaxThreadCount(qMax(2, QThread::idealThreadCount() - 1));
}

QString ThumbCache::diskKey(const Photo &p)
{
    const QByteArray src = (p.path + QLatin1Char('|') + QString::number(p.mtime) + QLatin1Char('|')
                            + QString::number(p.size) + QLatin1Char('|') + p.edits + QLatin1Char('|')
                            + QString::number(kSize))
                               .toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(src, QCryptographicHash::Sha1).toHex());
}

QPixmap ThumbCache::get(const Photo &p)
{
    const QString key = diskKey(p);
    m_keyOfPath.insert(p.path, key);
    if (QPixmap *pm = m_memory.object(key))
        return *pm;
    if (m_pending.contains(key))
        return QPixmap();
    m_pending.insert(key);

    const QString path = p.path;
    const QString edits = p.edits;
    const QString file = AppPaths::thumbsDir() + QLatin1Char('/') + key + QStringLiteral(".jpg");
    QPointer<ThumbCache> self(this);
    m_pool.start(QRunnable::create([self, path, key, edits, file]() {
        QImage img;
        if (QFile::exists(file))
            img.load(file, "JPEG");
        if (img.isNull()) {
            img = Edits::load(path, kSize * 2); // з запасом, бо правки можуть обрізати
            if (!img.isNull()) {
                if (!edits.isEmpty())
                    img = Edits::apply(img, EditState::fromJson(edits));
                if (img.width() > kSize || img.height() > kSize)
                    img = img.scaled(kSize, kSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                // кеш мініатюр — лише в папці програми, не поруч із фото
                QImageWriter w(file, "JPEG");
                w.setQuality(86);
                w.write(img.convertToFormat(QImage::Format_RGB32));
            }
        }
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, path, key, img]() {
                if (self)
                    self->onLoaded(path, key, img);
            },
            Qt::QueuedConnection);
    }), ++m_seq); // більший пріоритет — новіші запити
    return QPixmap();
}

void ThumbCache::onLoaded(const QString &path, const QString &key, const QImage &img)
{
    m_pending.remove(key);
    QPixmap pm;
    if (img.isNull()) {
        pm = QPixmap(1, 1); // «битий» файл — щоб не пробувати безкінечно
        pm.fill(Qt::transparent);
    } else {
        pm = QPixmap::fromImage(img);
    }
    m_memory.insert(key, new QPixmap(pm), qMax(1, int(qint64(pm.width()) * pm.height() * 4 / 1024)));
    if (m_keyOfPath.value(path) == key)
        emit ready(path);
}

void ThumbCache::invalidate(const QString &path)
{
    const QString key = m_keyOfPath.take(path);
    if (!key.isEmpty())
        m_memory.remove(key);
}

void ThumbCache::clearMemory()
{
    m_memory.clear();
}

void ThumbCache::cancelPending()
{
    m_pool.clear();
    m_pending.clear();
}
