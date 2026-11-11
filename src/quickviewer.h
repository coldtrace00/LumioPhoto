#pragma once

#include "edits.h"

#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QPointer>
#include <QStringList>
#include <QWidget>

class QLabel;
class QThreadPool;
class QToolButton;

// Швидкий переглядач фото (як Picasa Photo Viewer): відкривається з Провідника
// подвійним кліком, показує лише фото на затемненому тлі й кнопки
// «попереднє / наступне», масштаб, поворот, видалення, «Редагувати».
//
// Не відкриває бібліотеку й не сканує папки — тому стартує майже миттєво.
// Поворот неруйнівний: зберігається в базі Lumio Photo, файл не змінюється.
class QuickViewer : public QWidget
{
    Q_OBJECT
public:
    explicit QuickViewer(QWidget *parent = nullptr);
    ~QuickViewer() override;

    void openPath(const QString &path); // показати фото і його сусідів з тієї ж папки
    void showOnScreen();
    QString currentPath() const;
    int count() const { return m_files.size(); }
    int index() const { return m_index; }
    QSize displayedSize() const { return m_logical; }

signals:
    void editRequested(const QString &path); // відкрити в Lumio Photo для редагування

protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;

private:
    struct Loaded {
        QImage image;
        QSize logical; // розмір фото після правок (у пікселях оригіналу)
        bool full = false;
    };

    void scanFolder();
    void loadEdits();
    void go(int delta);
    void goTo(int index);
    void showCurrent();
    void setLoaded(const Loaded &l, bool keepView);
    void requestLoad(int index, bool full);
    void onLoaded(int gen, const QString &path, const Loaded &l, bool full);
    void prefetch();
    void maybeLoadFull();
    EditState editsFor(const QString &path) const;

    void rotate(bool clockwise);
    void deleteCurrent();
    void edit();

    void fit();
    void zoomTo(double zoom, QPointF anchor = QPointF(-1, -1));
    double fitZoom() const;
    QRectF imageRect() const;
    void clampPan();
    void updateUi();
    int maxSide() const;

    QStringList m_files;
    int m_index = 0;
    QString m_startPath;
    bool m_scanned = false;
    QHash<QString, QString> m_edits; // pathKey → JSON правок (лише з бази)
    bool m_editsLoaded = false;

    QPixmap m_pixmap;
    QSize m_logical;
    bool m_full = false;
    bool m_failed = false;
    double m_zoom = 1.0; // екранних пікселів на піксель фото
    bool m_fit = true;
    QPointF m_pan;
    bool m_dragging = false;
    bool m_pressOnImage = false;
    QPointF m_pressPos, m_lastMouse;

    QHash<QString, Loaded> m_cache; // зменшені копії поточного й сусідніх фото
    QSet<QString> m_pending;
    int m_gen = 0;
    QThreadPool *m_pool;

    QWidget *m_bar;
    QLabel *m_title;
    QLabel *m_zoomLabel;
    QToolButton *m_prev, *m_next;
};
