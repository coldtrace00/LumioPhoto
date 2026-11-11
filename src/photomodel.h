#pragma once

#include "photo.h"

#include <QAbstractListModel>
#include <QHash>
#include <QSet>
#include <QStyledItemDelegate>

class PhotoModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles { IdRole = Qt::UserRole + 1, PathRole, StarredRole, EditedRole };

    explicit PhotoModel(QObject *parent = nullptr);

    void setPhotos(const PhotoList &list);
    const PhotoList &photos() const { return m_list; }
    const Photo &photo(int row) const { return m_list.at(row); }
    int rowOfId(qint64 id) const { return m_rowById.value(id, -1); }
    void updatePhoto(const Photo &p);
    void removeIds(const QSet<qint64> &ids);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    Qt::DropActions supportedDragActions() const override { return Qt::CopyAction; }

private:
    void rebuildIndex();
    void onThumbReady(const QString &path);

    PhotoList m_list;
    QHash<qint64, int> m_rowById;
    QHash<QString, int> m_rowByPath;
};

class ThumbDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit ThumbDelegate(QObject *parent = nullptr);
    void setThumbSize(int s) { m_size = s; }
    int thumbSize() const { return m_size; }

    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &opt, const QModelIndex &index) const override;

private:
    int m_size = 170;
};
