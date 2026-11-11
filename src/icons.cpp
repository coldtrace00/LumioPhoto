#include "icons.h"

#include "theme.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

#include <cmath>
#include <initializer_list>

namespace Icons {

namespace {

QPolygonF poly(std::initializer_list<QPointF> pts)
{
    QPolygonF pg;
    for (const QPointF &p : pts)
        pg << p;
    return pg;
}

void starPath(QPainterPath &path, QPointF c, double R, double r)
{
    for (int i = 0; i < 10; ++i) {
        const double a = (-90 + i * 36) * 3.14159265358979 / 180.0;
        const double rad = (i % 2) ? r : R;
        const QPointF p(c.x() + std::cos(a) * rad, c.y() + std::sin(a) * rad);
        if (i == 0)
            path.moveTo(p);
        else
            path.lineTo(p);
    }
    path.closeSubpath();
}

void rotateArrow(QPainter &p, bool left)
{
    p.save();
    if (!left) {
        p.translate(24, 0);
        p.scale(-1, 1);
    }
    p.drawArc(QRectF(5, 6, 14, 14), 90 * 16, 270 * 16);
    p.drawPolyline(poly({{8.5, 2.5}, {12, 6}, {8.5, 9.5}}));
    p.restore();
}

// Усі гліфи — у системі координат 24x24
void drawGlyph(QPainter &p, Id id, const QColor &c)
{
    const QPen pen(c, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    auto line = [&p](double x1, double y1, double x2, double y2) { p.drawLine(QPointF(x1, y1), QPointF(x2, y2)); };

    switch (id) {
    case FolderAdd:
        p.drawPolygon(poly({{3, 6}, {9, 6}, {11, 8}, {21, 8}, {21, 19}, {3, 19}}));
        line(12, 11, 12, 16);
        line(9.5, 13.5, 14.5, 13.5);
        break;
    case Folder:
        p.drawPolygon(poly({{3, 6}, {9, 6}, {11, 8}, {21, 8}, {21, 19}, {3, 19}}));
        break;
    case Refresh:
        p.drawArc(QRectF(5, 5, 14, 14), 45 * 16, 300 * 16);
        p.drawPolyline(poly({{17.5, 2.5}, {17, 7}, {12.5, 6.5}}));
        break;
    case Star:
    case StarFilled: {
        QPainterPath s;
        starPath(s, QPointF(12, 12.6), 9, 4);
        if (id == StarFilled) {
            p.setBrush(c);
        }
        p.drawPath(s);
        break;
    }
    case RotateLeft:
        rotateArrow(p, true);
        break;
    case RotateRight:
        rotateArrow(p, false);
        break;
    case Export:
        p.drawPolyline(poly({{8, 9}, {4, 9}, {4, 20}, {20, 20}, {20, 9}, {16, 9}}));
        line(12, 3, 12, 14);
        p.drawPolyline(poly({{8, 7}, {12, 3}, {16, 7}}));
        break;
    case Album:
        p.drawRoundedRect(QRectF(6, 3, 14, 15), 2, 2);
        p.drawPolyline(poly({{4, 6}, {4, 21}, {17, 21}}));
        break;
    case Slideshow:
    case Play:
        if (id == Slideshow)
            p.drawRoundedRect(QRectF(3, 4, 18, 14), 2, 2);
        p.setBrush(c);
        if (id == Slideshow)
            p.drawPolygon(poly({{10, 8}, {15, 11}, {10, 14}}));
        else
            p.drawPolygon(poly({{8, 5}, {19, 12}, {8, 19}}));
        if (id == Slideshow) {
            p.setBrush(Qt::NoBrush);
            line(8, 21, 16, 21);
        }
        break;
    case Pause:
        p.setBrush(c);
        p.drawRoundedRect(QRectF(6.5, 5, 3.5, 14), 1, 1);
        p.drawRoundedRect(QRectF(14, 5, 3.5, 14), 1, 1);
        break;
    case Copy:
        p.drawRoundedRect(QRectF(8, 8, 12, 12), 2, 2);
        p.drawPolyline(poly({{8, 16}, {5.5, 16}, {4, 14.5}, {4, 5.5}, {5.5, 4}, {14.5, 4}, {16, 5.5}, {16, 8}}));
        break;
    case Trash:
        line(4, 6, 20, 6);
        p.drawPolyline(poly({{9, 6}, {9, 3.5}, {15, 3.5}, {15, 6}}));
        p.drawPolyline(poly({{6, 6}, {7, 20.5}, {17, 20.5}, {18, 6}}));
        line(10, 10, 10, 17);
        line(14, 10, 14, 17);
        break;
    case Info:
        p.drawEllipse(QRectF(3.5, 3.5, 17, 17));
        line(12, 11, 12, 16.5);
        p.drawPoint(QPointF(12, 7.8));
        break;
    case Crop:
        p.drawPolyline(poly({{6, 2.5}, {6, 18}, {21.5, 18}}));
        p.drawPolyline(poly({{2.5, 6}, {18, 6}, {18, 21.5}}));
        break;
    case Straighten:
        line(3, 16, 21, 8);
        p.setPen(QPen(c, 1.2, Qt::DashLine));
        line(3, 12, 21, 12);
        break;
    case Magic: {
        line(4, 20, 14, 10);
        QPainterPath s;
        starPath(s, QPointF(17, 7), 5, 1.8);
        p.setBrush(c);
        p.drawPath(s);
        break;
    }
    case Undo:
        p.drawPolyline(poly({{8, 5}, {4, 9}, {8, 13}}));
        {
            QPainterPath path(QPointF(4, 9));
            path.lineTo(14, 9);
            path.cubicTo(21, 9, 21, 19, 14, 19);
            path.lineTo(9, 19);
            p.drawPath(path);
        }
        break;
    case Reset:
        p.drawArc(QRectF(4.5, 4.5, 15, 15), 100 * 16, 300 * 16);
        p.drawPolyline(poly({{4, 4}, {5.5, 8.5}, {10, 7}}));
        break;
    case Back:
        line(20, 12, 4, 12);
        p.drawPolyline(poly({{10, 6}, {4, 12}, {10, 18}}));
        break;
    case Prev:
        p.drawPolyline(poly({{15, 4}, {7, 12}, {15, 20}}));
        break;
    case Next:
        p.drawPolyline(poly({{9, 4}, {17, 12}, {9, 20}}));
        break;
    case ZoomFit:
        p.drawPolyline(poly({{4, 9}, {4, 4}, {9, 4}}));
        p.drawPolyline(poly({{15, 4}, {20, 4}, {20, 9}}));
        p.drawPolyline(poly({{20, 15}, {20, 20}, {15, 20}}));
        p.drawPolyline(poly({{9, 20}, {4, 20}, {4, 15}}));
        break;
    case Zoom100:
        p.drawEllipse(QRectF(3.5, 3.5, 13, 13));
        line(14.5, 14.5, 20.5, 20.5);
        line(10, 7, 10, 13);
        line(7, 10, 13, 10);
        break;
    case Search:
        p.drawEllipse(QRectF(3.5, 3.5, 13, 13));
        line(14.5, 14.5, 20.5, 20.5);
        break;
    case Fullscreen:
        p.drawPolyline(poly({{3, 8}, {3, 3}, {8, 3}}));
        p.drawPolyline(poly({{16, 3}, {21, 3}, {21, 8}}));
        p.drawPolyline(poly({{21, 16}, {21, 21}, {16, 21}}));
        p.drawPolyline(poly({{8, 21}, {3, 21}, {3, 16}}));
        p.drawRect(QRectF(8, 8, 8, 8));
        break;
    case Edit:
        p.drawPolygon(poly({{4, 20}, {5, 15}, {16, 4}, {20, 8}, {9, 19}}));
        line(14, 6, 18, 10);
        break;
    case Save:
        p.drawRoundedRect(QRectF(4, 4, 16, 16), 2, 2);
        p.drawRect(QRectF(8, 4, 7, 5));
        p.drawRect(QRectF(7, 13, 10, 7));
        break;
    case Print:
        p.drawRect(QRectF(7, 3, 10, 5));
        p.drawRoundedRect(QRectF(3, 8, 18, 9), 2, 2);
        p.drawRect(QRectF(7, 14, 10, 7));
        break;
    case Wallpaper:
        p.drawRoundedRect(QRectF(3, 4, 18, 12), 1.5, 1.5);
        p.drawPolyline(poly({{5, 14}, {10, 9}, {13, 12}, {15, 10}, {19, 14}}));
        line(12, 16, 12, 20);
        line(8, 20, 16, 20);
        break;
    case Plus:
        line(12, 5, 12, 19);
        line(5, 12, 19, 12);
        break;
    case Gear: {
        p.save();
        p.translate(12, 12);
        p.setPen(QPen(c, 3, Qt::SolidLine, Qt::FlatCap));
        for (int i = 0; i < 8; ++i) {
            p.drawLine(QPointF(0, -6.5), QPointF(0, -9.5));
            p.rotate(45);
        }
        p.setPen(pen);
        p.drawEllipse(QPointF(0, 0), 6.5, 6.5);
        p.drawEllipse(QPointF(0, 0), 2.5, 2.5);
        p.restore();
        break;
    }
    case Photos:
        p.drawRoundedRect(QRectF(3, 4, 18, 16), 2.5, 2.5);
        p.drawPolyline(poly({{3.5, 17}, {9, 11}, {13, 15}, {16, 12}, {20.5, 16.5}}));
        p.drawEllipse(QPointF(15.5, 8.5), 1.6, 1.6);
        break;
    case Clock:
        p.drawEllipse(QRectF(3.5, 3.5, 17, 17));
        line(12, 12, 12, 7);
        line(12, 12, 15.5, 14);
        break;
    case Close:
        line(6, 6, 18, 18);
        line(18, 6, 6, 18);
        break;
    case Open:
        p.drawPolyline(poly({{11, 5}, {5, 5}, {5, 19}, {19, 19}, {19, 13}}));
        line(12, 12, 20, 4);
        p.drawPolyline(poly({{14, 4}, {20, 4}, {20, 10}}));
        break;
    }
}

} // namespace

QPixmap pixmap(Id id, int size, const QColor &color)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 24.0, size / 24.0);
    drawGlyph(p, id, color);
    return pm;
}

QIcon icon(Id id, const QColor &requested)
{
    const QColor color = requested.isValid() ? requested : Theme::iconColor();
    QIcon ic;
    for (int s : {16, 20, 24, 32, 40, 48})
        ic.addPixmap(pixmap(id, s, color));
    QColor dis = color;
    dis.setAlpha(90);
    for (int s : {16, 20, 24, 32, 40, 48})
        ic.addPixmap(pixmap(id, s, dis), QIcon::Disabled);
    return ic;
}

// Лого Lumio Photo: «фото» (гори й сонце) у рамці на градієнті бірюза → синій → фіолетовий
// та спалах — у тому ж стилі, що й лого Lumio.
QPixmap appPixmap(int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 24.0, size / 24.0);

    QLinearGradient g(2, 1, 22, 23);
    g.setColorAt(0.0, QColor(0x2d, 0xe0, 0xc0));
    g.setColorAt(0.5, QColor(0x3b, 0x82, 0xf6));
    g.setColorAt(1.0, QColor(0x8b, 0x4b, 0xff));
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawRoundedRect(QRectF(1, 1, 22, 22), 6, 6);

    QLinearGradient gl(0, 1, 0, 12);
    gl.setColorAt(0, QColor(255, 255, 255, 70));
    gl.setColorAt(1, QColor(255, 255, 255, 0));
    p.setBrush(gl);
    p.drawRoundedRect(QRectF(1, 1, 22, 11), 6, 6);

    const double pw = size <= 20 ? 1.9 : 1.45;
    p.setPen(QPen(Qt::white, pw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(QColor(255, 255, 255, 38));
    const QRectF frame(4.6, 6.2, 14.8, 12.2);
    p.drawRoundedRect(frame, 2.4, 2.4);

    // гори
    QPainterPath m;
    m.moveTo(5.4, 17.4);
    m.lineTo(9.6, 12.2);
    m.lineTo(12.4, 15.2);
    m.lineTo(14.6, 13.0);
    m.lineTo(18.6, 17.4);
    m.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawPath(m);
    // сонце
    p.drawEllipse(QPointF(15.2, 9.6), 1.35, 1.35);

    if (size >= 20) {
        const QPointF s(19.0, 4.6);
        const double L = 2.6, w = 0.75;
        QPainterPath star;
        star.moveTo(s + QPointF(0, -L));
        star.quadTo(s + QPointF(w * 0.3, -w * 0.3), s + QPointF(L, 0));
        star.quadTo(s + QPointF(w * 0.3, w * 0.3), s + QPointF(0, L));
        star.quadTo(s + QPointF(-w * 0.3, w * 0.3), s + QPointF(-L, 0));
        star.quadTo(s + QPointF(-w * 0.3, -w * 0.3), s + QPointF(0, -L));
        p.drawPath(star);
    }
    p.end();
    return pm;
}

QIcon appIcon()
{
    QIcon ic;
    for (int s : {16, 20, 24, 32, 40, 48, 64, 128, 256})
        ic.addPixmap(appPixmap(s));
    return ic;
}

} // namespace Icons
