#pragma once

#include <QImage>
#include <QWidget>

// Полотно перегляду: масштаб (колесо), перетягування, режим обрізання та сітка для вирівнювання.
class ImageCanvas : public QWidget
{
    Q_OBJECT
public:
    explicit ImageCanvas(QWidget *parent = nullptr);

    // image — те, що малюємо; logicalSize — розмір «справжнього» зображення в пікселях
    // (image може бути зменшеною копією для швидкості)
    void setImage(const QImage &image, const QSize &logicalSize, bool keepView = false);
    const QImage &image() const { return m_image; }
    QSize logicalSize() const { return m_logical; }

    void fitToWindow();
    void zoomTo(double zoom, const QPointF &anchor = QPointF(-1, -1));
    double zoom() const { return m_zoom; }
    bool isFit() const { return m_fit; }
    double fitZoom() const;

    void setGrid(bool on);
    void setMessage(const QString &text);

    // Обрізання (нормалізовані 0..1 координати поточного зображення)
    void setCropMode(bool on, const QRectF &initial = QRectF());
    bool cropMode() const { return m_cropMode; }
    void setCropAspect(double aspect); // 0 — вільно
    QRectF cropRect() const { return m_crop; }

signals:
    void zoomChanged(double zoom);
    void prevRequested();
    void nextRequested();
    void escapePressed();
    void cropChanged();

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
    QRectF imageRect() const;               // де на екрані лежить зображення
    QPointF toNorm(const QPointF &w) const; // з координат віджета в 0..1
    QRectF cropScreenRect() const;
    int cropHandleAt(const QPointF &pos) const;
    void clampPan();
    void applyAspect(int handle);

    QImage m_image;
    QSize m_logical;
    double m_zoom = 1.0; // екранних пікселів на логічний піксель
    bool m_fit = true;
    QPointF m_pan;       // зсув центру зображення від центру віджета
    bool m_panning = false;
    QPointF m_lastMouse;
    bool m_grid = false;
    QString m_message;

    bool m_cropMode = false;
    QRectF m_crop;
    double m_aspect = 0;
    int m_cropDrag = -1; // 0..7 ручки, 8 — переміщення, 9 — нове виділення
    QPointF m_cropStart;
    QRectF m_cropAtPress;
};
