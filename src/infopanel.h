#pragma once

#include "photo.h"

#include <QWidget>

class QLabel;
class QLineEdit;

// Панель «Інформація»: дані файлу та EXIF (лише читання), теги й підпис
// (зберігаються в базі програми, не у файлі).
class InfoPanel : public QWidget
{
    Q_OBJECT
public:
    explicit InfoPanel(QWidget *parent = nullptr);
    void setPhoto(const Photo &p);
    void setMultiple(int count, qint64 totalBytes);
    void clear();

signals:
    void tagsEdited(qint64 id, const QString &tags);
    void captionEdited(qint64 id, const QString &caption);

private:
    qint64 m_id = 0;
    QLabel *m_title;
    QLabel *m_details;
    QLineEdit *m_tags;
    QLineEdit *m_caption;
    QLabel *m_albums;
    QWidget *m_editors;
};
