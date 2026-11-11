#pragma once

#include "photo.h"

#include <QCache>
#include <QHash>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QThreadPool>

// Кеш мініатюр: пам'ять (QCache) + диск (%LOCALAPPDATA%\LumioPhoto\thumbs).
// Мініатюри робляться у фонових потоках; останні запитані — першими
// (щоб при швидкому прокручуванні спершу з'являлось те, що на екрані).
class ThumbCache : public QObject
{
    Q_OBJECT
public:
    static constexpr int kSize = 360; // найбільша сторона мініатюри

    static ThumbCache *instance();

    // Повертає мініатюру, якщо вона вже є в пам'яті; інакше ставить у чергу і повертає порожню.
    QPixmap get(const Photo &p);
    void invalidate(const QString &path);
    void clearMemory();
    void cancelPending();

signals:
    void ready(const QString &path);

private:
    explicit ThumbCache(QObject *parent = nullptr);
    void onLoaded(const QString &path, const QString &key, const QImage &img);
    static QString diskKey(const Photo &p);

    QCache<QString, QPixmap> m_memory;   // ключ — diskKey
    QHash<QString, QString> m_keyOfPath; // шлях -> актуальний ключ
    QSet<QString> m_pending;
    QThreadPool m_pool;
    int m_seq = 0;
};
