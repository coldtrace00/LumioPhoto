#include "photomodel.h"

#include "theme.h"
#include "i18n.h"

#include "icons.h"
#include "thumbcache.h"
#include "util.h"

#include <QDateTime>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QUrl>

PhotoModel::PhotoModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(ThumbCache::instance(), &ThumbCache::ready, this, &PhotoModel::onThumbReady);
}

void PhotoModel::setPhotos(const PhotoList &list)
{
    beginResetModel();
    m_list = list;
    rebuildIndex();
    endResetModel();
}

void PhotoModel::rebuildIndex()
{
    m_rowById.clear();
    m_rowByPath.clear();
    m_rowById.reserve(m_list.size());
    m_rowByPath.reserve(m_list.size());
    for (int i = 0; i < m_list.size(); ++i) {
        m_rowById.insert(m_list[i].id, i);
        m_rowByPath.insert(m_list[i].path, i);
    }
}

void PhotoModel::updatePhoto(const Photo &p)
{
    const int row = rowOfId(p.id);
    if (row < 0)
        return;
    if (m_list[row].path != p.path) {
        m_rowByPath.remove(m_list[row].path);
        m_rowByPath.insert(p.path, row);
    }
    m_list[row] = p;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
}

void PhotoModel::removeIds(const QSet<qint64> &ids)
{
    for (int i = m_list.size() - 1; i >= 0; --i) {
        if (ids.contains(m_list[i].id)) {
            beginRemoveRows(QModelIndex(), i, i);
            m_list.removeAt(i);
            endRemoveRows();
        }
    }
    rebuildIndex();
}

void PhotoModel::onThumbReady(const QString &path)
{
    const int row = m_rowByPath.value(path, -1);
    if (row >= 0) {
        const QModelIndex idx = index(row);
        emit dataChanged(idx, idx, {Qt::DecorationRole});
    }
}

int PhotoModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_list.size();
}

QVariant PhotoModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_list.size())
        return QVariant();
    const Photo &p = m_list.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return p.name;
    case Qt::ToolTipRole: {
        QString t = QStringLiteral("<b>%1</b><br>%2").arg(p.name.toHtmlEscaped(),
                                                           QDateTime::fromSecsSinceEpoch(p.taken).toString(QStringLiteral("dd.MM.yyyy HH:mm")));
        if (p.width > 0)
            t += QStringLiteral("<br>%1 × %2  •  %3").arg(p.width).arg(p.height).arg(Util::humanSize(p.size));
        if (!p.tags.isEmpty())
            t += tr2("<br>Теги: %1", "<br>Tags: %1").arg(p.tags.toHtmlEscaped());
        if (!p.caption.isEmpty())
            t += QStringLiteral("<br><i>%1</i>").arg(p.caption.toHtmlEscaped());
        return t;
    }
    case IdRole:
        return p.id;
    case PathRole:
        return p.path;
    case StarredRole:
        return p.starred;
    case EditedRole:
        return !p.edits.isEmpty();
    default:
        return QVariant();
    }
}

Qt::ItemFlags PhotoModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = QAbstractListModel::flags(index);
    if (index.isValid())
        f |= Qt::ItemIsDragEnabled;
    return f;
}

QStringList PhotoModel::mimeTypes() const
{
    return {QStringLiteral("text/uri-list")};
}

QMimeData *PhotoModel::mimeData(const QModelIndexList &indexes) const
{
    auto *md = new QMimeData;
    QList<QUrl> urls;
    for (const QModelIndex &i : indexes)
        urls << QUrl::fromLocalFile(m_list.at(i.row()).path);
    md->setUrls(urls);
    return md;
}

// ---------------------------------------------------------------------------

ThumbDelegate::ThumbDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QSize ThumbDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const
{
    return QSize(m_size + 14, m_size + 14);
}

void ThumbDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &index) const
{
    const auto *model = qobject_cast<const PhotoModel *>(index.model());
    if (!model)
        return;
    const Photo &ph = model->photo(index.row());
    const QRect cell = opt.rect.adjusted(4, 4, -4, -4);
    const bool selected = opt.state & QStyle::State_Selected;
    const bool hover = opt.state & QStyle::State_MouseOver;

    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setRenderHint(QPainter::SmoothPixmapTransform);

    if (selected) {
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(59, 130, 246, 70));
        p->drawRoundedRect(cell, 8, 8);
    } else if (hover) {
        p->setPen(Qt::NoPen);
        p->setBrush(Theme::isDark() ? QColor(255, 255, 255, 18) : QColor(0, 0, 0, 14));
        p->drawRoundedRect(cell, 8, 8);
    }

    const QRect area = cell.adjusted(6, 6, -6, -6);
    const QPixmap pm = ThumbCache::instance()->get(ph);
    if (pm.isNull() || pm.width() <= 1) {
        // ще завантажується (або файл пошкоджено)
        QSize s(ph.width > 0 ? ph.width : 4, ph.height > 0 ? ph.height : 3);
        s.scale(area.size(), Qt::KeepAspectRatio);
        QRect r(QPoint(0, 0), s);
        r.moveCenter(area.center());
        p->setPen(Qt::NoPen);
        const QColor tile = Theme::isDark() ? QColor(255, 255, 255) : QColor(0, 0, 0);
        p->setBrush(QColor(tile.red(), tile.green(), tile.blue(), pm.isNull() ? 14 : 30));
        p->drawRoundedRect(r, 4, 4);
        if (!pm.isNull()) {
            p->setPen(QColor(tile.red(), tile.green(), tile.blue(), 120));
            p->drawText(r, Qt::AlignCenter, QStringLiteral("?"));
        }
    } else {
        QSize s = pm.size();
        s.scale(area.size(), Qt::KeepAspectRatio);
        QRect r(QPoint(0, 0), s);
        r.moveCenter(area.center());
        // тінь
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0, 0, 0, Theme::isDark() ? 90 : 40));
        p->drawRoundedRect(r.translated(0, 2).adjusted(-1, -1, 1, 1), 4, 4);
        QPainterPath clip;
        clip.addRoundedRect(r, 3, 3);
        p->setClipPath(clip);
        p->drawPixmap(r, pm);
        p->setClipping(false);
        if (selected) {
            p->setPen(QPen(QColor(96, 165, 250), 2));
            p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(r.adjusted(-1, -1, 1, 1), 4, 4);
        }

        const int b = 22;
        if (ph.starred) {
            const QRect sr(r.left() + 4, r.bottom() - b - 3, b, b);
            p->setPen(Qt::NoPen);
            p->setBrush(QColor(0, 0, 0, 120));
            p->drawEllipse(sr);
            static const QPixmap star = Icons::pixmap(Icons::StarFilled, 32, QColor(255, 200, 60));
            p->drawPixmap(sr.adjusted(3, 3, -3, -3), star);
        }
        if (!ph.edits.isEmpty()) {
            const QRect er(r.right() - b - 3, r.bottom() - b - 3, b, b);
            p->setPen(Qt::NoPen);
            p->setBrush(QColor(0, 0, 0, 120));
            p->drawEllipse(er);
            static const QPixmap pen = Icons::pixmap(Icons::Edit, 32, QColor(255, 255, 255, 220));
            p->drawPixmap(er.adjusted(4, 4, -4, -4), pen);
        }
    }
    p->restore();
}
