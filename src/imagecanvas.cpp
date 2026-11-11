#include "imagecanvas.h"

#include "theme.h"
#include "i18n.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <QLineF>

#include <algorithm>
#include <cmath>

ImageCanvas::ImageCanvas(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(200, 150);
}

void ImageCanvas::setImage(const QImage &image, const QSize &logicalSize, bool keepView)
{
    m_image = image;
    m_logical = logicalSize.isValid() ? logicalSize : image.size();
    m_message.clear();
    if (!keepView || m_fit) {
        m_fit = true;
        m_zoom = fitZoom();
        m_pan = QPointF();
    }
    clampPan();
    update();
    emit zoomChanged(m_zoom);
}

double ImageCanvas::fitZoom() const
{
    if (m_logical.isEmpty())
        return 1.0;
    const double margin = 24;
    const double z = std::min((width() - margin) / m_logical.width(), (height() - margin) / m_logical.height());
    return std::min(z, 1.0) > 0 ? std::min(z, 1.0) : 0.01; // маленькі фото не розтягуємо
}

void ImageCanvas::fitToWindow()
{
    m_fit = true;
    m_zoom = fitZoom();
    m_pan = QPointF();
    update();
    emit zoomChanged(m_zoom);
}

void ImageCanvas::zoomTo(double zoom, const QPointF &anchor)
{
    zoom = std::clamp(zoom, 0.02, 16.0);
    const QPointF a = anchor.x() < 0 ? QPointF(width() / 2.0, height() / 2.0) : anchor;
    // точка під курсором залишається на місці
    const QPointF center(width() / 2.0 + m_pan.x(), height() / 2.0 + m_pan.y());
    const QPointF rel = (a - center) / m_zoom;
    m_zoom = zoom;
    m_pan = a - rel * m_zoom - QPointF(width() / 2.0, height() / 2.0);
    m_fit = false;
    clampPan();
    update();
    emit zoomChanged(m_zoom);
}

void ImageCanvas::setGrid(bool on)
{
    m_grid = on;
    update();
}

void ImageCanvas::setMessage(const QString &text)
{
    m_message = text;
    update();
}

void ImageCanvas::setCropMode(bool on, const QRectF &initial)
{
    m_cropMode = on;
    m_crop = initial.isEmpty() ? QRectF(0.1, 0.1, 0.8, 0.8) : initial;
    if (on)
        fitToWindow();
    setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
    update();
}

void ImageCanvas::setCropAspect(double aspect)
{
    m_aspect = aspect;
    if (m_cropMode && aspect > 0)
        applyAspect(4);
    update();
    emit cropChanged();
}

QRectF ImageCanvas::imageRect() const
{
    const QSizeF s(m_logical.width() * m_zoom, m_logical.height() * m_zoom);
    const QPointF c(width() / 2.0 + m_pan.x(), height() / 2.0 + m_pan.y());
    return QRectF(c.x() - s.width() / 2, c.y() - s.height() / 2, s.width(), s.height());
}

QPointF ImageCanvas::toNorm(const QPointF &w) const
{
    const QRectF r = imageRect();
    return QPointF(std::clamp((w.x() - r.left()) / r.width(), 0.0, 1.0),
                   std::clamp((w.y() - r.top()) / r.height(), 0.0, 1.0));
}

QRectF ImageCanvas::cropScreenRect() const
{
    const QRectF r = imageRect();
    return QRectF(r.left() + m_crop.x() * r.width(), r.top() + m_crop.y() * r.height(), m_crop.width() * r.width(),
                  m_crop.height() * r.height());
}

void ImageCanvas::clampPan()
{
    const QRectF r = imageRect();
    const double maxX = std::max(0.0, (r.width() - width()) / 2.0 + 40);
    const double maxY = std::max(0.0, (r.height() - height()) / 2.0 + 40);
    m_pan.setX(std::clamp(m_pan.x(), -maxX, maxX));
    m_pan.setY(std::clamp(m_pan.y(), -maxY, maxY));
}

void ImageCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Theme::canvasColor());
    if (m_image.isNull()) {
        p.setPen(Theme::isDark() ? QColor(200, 200, 205) : QColor(80, 86, 94));
        p.drawText(rect(), Qt::AlignCenter, m_message.isEmpty() ? tr2("Завантаження…", "Loading…") : m_message);
        return;
    }
    const QRectF r = imageRect();
    p.setRenderHint(QPainter::SmoothPixmapTransform, m_zoom < 2.0);
    p.drawImage(r, m_image);

    if (m_grid) {
        p.setPen(QPen(QColor(255, 255, 255, 90), 1));
        const int n = 8;
        for (int i = 1; i < n; ++i) {
            p.drawLine(QPointF(r.left() + r.width() * i / n, r.top()), QPointF(r.left() + r.width() * i / n, r.bottom()));
            p.drawLine(QPointF(r.left(), r.top() + r.height() * i / n), QPointF(r.right(), r.top() + r.height() * i / n));
        }
    }

    if (m_cropMode) {
        const QRectF c = cropScreenRect();
        QPainterPath outside;
        outside.addRect(r);
        outside.addRect(c);
        outside.setFillRule(Qt::OddEvenFill);
        p.fillPath(outside, QColor(0, 0, 0, 150));
        p.setPen(QPen(Qt::white, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRect(c);
        // правило третин
        p.setPen(QPen(QColor(255, 255, 255, 110), 1));
        for (int i = 1; i < 3; ++i) {
            p.drawLine(QPointF(c.left() + c.width() * i / 3, c.top()), QPointF(c.left() + c.width() * i / 3, c.bottom()));
            p.drawLine(QPointF(c.left(), c.top() + c.height() * i / 3), QPointF(c.right(), c.top() + c.height() * i / 3));
        }
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        const QPointF pts[] = {c.topLeft(), QPointF(c.center().x(), c.top()), c.topRight(), QPointF(c.right(), c.center().y()),
                               c.bottomRight(), QPointF(c.center().x(), c.bottom()), c.bottomLeft(), QPointF(c.left(), c.center().y())};
        for (const QPointF &pt : pts)
            p.drawRect(QRectF(pt.x() - 4, pt.y() - 4, 8, 8));
        const QString sz = QStringLiteral("%1 × %2").arg(qRound(m_crop.width() * m_logical.width()))
                               .arg(qRound(m_crop.height() * m_logical.height()));
        QFont f = font();
        f.setPixelSize(12);
        p.setFont(f);
        const QRectF lr(c.left(), c.top() - 22, 110, 18);
        p.setBrush(QColor(0, 0, 0, 170));
        p.drawRoundedRect(lr, 4, 4);
        p.setPen(Qt::white);
        p.drawText(lr, Qt::AlignCenter, sz);
    }

    // масштаб у кутку
    if (!m_cropMode) {
        const QString z = QStringLiteral("%1%").arg(qRound(m_zoom * 100));
        QFont f = font();
        f.setPixelSize(12);
        p.setFont(f);
        const QRectF zr(width() - 70, height() - 30, 60, 20);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 120));
        p.drawRoundedRect(zr, 4, 4);
        p.setPen(QColor(230, 230, 235));
        p.drawText(zr, Qt::AlignCenter, z);
    }
}

void ImageCanvas::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (m_fit) {
        m_zoom = fitZoom();
        m_pan = QPointF();
        emit zoomChanged(m_zoom);
    }
    clampPan();
}

void ImageCanvas::wheelEvent(QWheelEvent *e)
{
    if (m_image.isNull() || m_cropMode)
        return;
    const int d = e->angleDelta().y();
    if (d == 0)
        return;
    const double factor = std::pow(1.0015, d);
    zoomTo(m_zoom * factor, e->position());
}

int ImageCanvas::cropHandleAt(const QPointF &pos) const
{
    const QRectF c = cropScreenRect();
    const QPointF pts[] = {c.topLeft(), QPointF(c.center().x(), c.top()), c.topRight(), QPointF(c.right(), c.center().y()),
                           c.bottomRight(), QPointF(c.center().x(), c.bottom()), c.bottomLeft(), QPointF(c.left(), c.center().y())};
    for (int i = 0; i < 8; ++i)
        if (QLineF(pts[i], pos).length() <= 10)
            return i;
    if (c.contains(pos))
        return 8;
    return -1;
}

void ImageCanvas::applyAspect(int handle)
{
    if (m_aspect <= 0 || m_logical.isEmpty())
        return;
    // аспект у нормалізованих координатах
    const double a = m_aspect * m_logical.height() / double(m_logical.width());
    QRectF r = m_crop;
    double w = r.width(), h = r.height();
    if (handle == 1 || handle == 5)
        w = h * a;
    else
        h = w / a;
    if (w > 1) { w = 1; h = w / a; }
    if (h > 1) { h = 1; w = h * a; }
    // якоримо протилежний кут/край
    QPointF anchor = r.center();
    switch (handle) {
    case 0: anchor = r.bottomRight(); r = QRectF(anchor.x() - w, anchor.y() - h, w, h); break;
    case 2: anchor = r.bottomLeft(); r = QRectF(anchor.x(), anchor.y() - h, w, h); break;
    case 4: anchor = r.topLeft(); r = QRectF(anchor.x(), anchor.y(), w, h); break;
    case 6: anchor = r.topRight(); r = QRectF(anchor.x() - w, anchor.y(), w, h); break;
    default: r = QRectF(anchor.x() - w / 2, anchor.y() - h / 2, w, h); break;
    }
    if (r.left() < 0) r.moveLeft(0);
    if (r.top() < 0) r.moveTop(0);
    if (r.right() > 1) r.moveRight(1);
    if (r.bottom() > 1) r.moveBottom(1);
    m_crop = r;
}

void ImageCanvas::mousePressEvent(QMouseEvent *e)
{
    setFocus();
    m_lastMouse = e->position();
    if (e->button() == Qt::BackButton) {
        emit prevRequested();
        return;
    }
    if (e->button() == Qt::ForwardButton) {
        emit nextRequested();
        return;
    }
    if (e->button() != Qt::LeftButton)
        return;
    if (m_cropMode) {
        m_cropDrag = cropHandleAt(e->position());
        m_cropStart = toNorm(e->position());
        m_cropAtPress = m_crop;
        if (m_cropDrag < 0) {
            m_cropDrag = 9;
            m_crop = QRectF(m_cropStart, QSizeF(0.0001, 0.0001));
        }
        return;
    }
    const QRectF r = imageRect();
    if (r.width() > width() || r.height() > height()) {
        m_panning = true;
        setCursor(Qt::ClosedHandCursor);
    }
}

void ImageCanvas::mouseMoveEvent(QMouseEvent *e)
{
    const QPointF pos = e->position();
    if (m_cropMode) {
        if (m_cropDrag < 0) {
            const int h = cropHandleAt(pos);
            static const Qt::CursorShape shapes[] = {Qt::SizeFDiagCursor, Qt::SizeVerCursor, Qt::SizeBDiagCursor,
                                                     Qt::SizeHorCursor,   Qt::SizeFDiagCursor, Qt::SizeVerCursor,
                                                     Qt::SizeBDiagCursor, Qt::SizeHorCursor,  Qt::SizeAllCursor};
            setCursor(h >= 0 ? shapes[h] : Qt::CrossCursor);
            return;
        }
        const QPointF n = toNorm(pos);
        const QPointF d = n - m_cropStart;
        QRectF r = m_cropAtPress;
        switch (m_cropDrag) {
        case 0: r.setTopLeft(r.topLeft() + d); break;
        case 1: r.setTop(r.top() + d.y()); break;
        case 2: r.setTopRight(r.topRight() + d); break;
        case 3: r.setRight(r.right() + d.x()); break;
        case 4: r.setBottomRight(r.bottomRight() + d); break;
        case 5: r.setBottom(r.bottom() + d.y()); break;
        case 6: r.setBottomLeft(r.bottomLeft() + d); break;
        case 7: r.setLeft(r.left() + d.x()); break;
        case 8: {
            r.translate(d);
            if (r.left() < 0) r.moveLeft(0);
            if (r.top() < 0) r.moveTop(0);
            if (r.right() > 1) r.moveRight(1);
            if (r.bottom() > 1) r.moveBottom(1);
            break;
        }
        case 9: r = QRectF(m_cropStart, n); break;
        }
        r = r.normalized().intersected(QRectF(0, 0, 1, 1));
        if (r.width() < 0.01) r.setWidth(0.01);
        if (r.height() < 0.01) r.setHeight(0.01);
        m_crop = r;
        if (m_aspect > 0 && m_cropDrag != 8)
            applyAspect(m_cropDrag == 9 ? 4 : m_cropDrag);
        update();
        emit cropChanged();
        return;
    }
    if (m_panning) {
        m_pan += pos - m_lastMouse;
        m_lastMouse = pos;
        clampPan();
        update();
    } else {
        const QRectF r = imageRect();
        setCursor((r.width() > width() || r.height() > height()) ? Qt::OpenHandCursor : Qt::ArrowCursor);
    }
}

void ImageCanvas::mouseReleaseEvent(QMouseEvent *)
{
    m_panning = false;
    m_cropDrag = -1;
    if (!m_cropMode)
        setCursor(Qt::ArrowCursor);
}

void ImageCanvas::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (m_cropMode || m_image.isNull())
        return;
    if (m_fit || m_zoom < 0.99)
        zoomTo(1.0, e->position());
    else
        fitToWindow();
}

void ImageCanvas::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
    case Qt::Key_Left:
    case Qt::Key_PageUp:
        if (!m_cropMode) emit prevRequested();
        return;
    case Qt::Key_Right:
    case Qt::Key_PageDown:
    case Qt::Key_Space:
        if (!m_cropMode) emit nextRequested();
        return;
    case Qt::Key_Escape:
        emit escapePressed();
        return;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        zoomTo(m_zoom * 1.25);
        return;
    case Qt::Key_Minus:
        zoomTo(m_zoom / 1.25);
        return;
    case Qt::Key_0:
        fitToWindow();
        return;
    case Qt::Key_1:
        zoomTo(1.0);
        return;
    default:
        QWidget::keyPressEvent(e);
    }
}
