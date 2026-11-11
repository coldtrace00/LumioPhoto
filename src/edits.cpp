#include "edits.h"

#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTransform>
#include <QtMath>

#include <algorithm>
#include <array>
#include <cmath>

bool EditState::isIdentity() const
{
    return rotate == 0 && !flip && straighten == 0 && crop.isEmpty() && !autoColor && !autoContrast
           && brightness == 0 && contrast == 0 && saturation == 0 && warmth == 0 && shadows == 0 && highlights == 0
           && !blackWhite && !sepia && sharpen == 0 && vignette == 0;
}

QString EditState::toJson() const
{
    if (isIdentity())
        return QString();
    QJsonObject o;
    auto putInt = [&o](const char *k, int v) { if (v) o.insert(QLatin1String(k), v); };
    auto putBool = [&o](const char *k, bool v) { if (v) o.insert(QLatin1String(k), true); };
    putInt("rotate", rotate);
    putBool("flip", flip);
    if (straighten != 0)
        o.insert(QStringLiteral("straighten"), straighten);
    if (!crop.isEmpty()) {
        o.insert(QStringLiteral("crop"), QStringLiteral("%1,%2,%3,%4")
                                             .arg(crop.x(), 0, 'f', 5).arg(crop.y(), 0, 'f', 5)
                                             .arg(crop.width(), 0, 'f', 5).arg(crop.height(), 0, 'f', 5));
    }
    putBool("autoColor", autoColor);
    putBool("autoContrast", autoContrast);
    putInt("brightness", brightness);
    putInt("contrast", contrast);
    putInt("saturation", saturation);
    putInt("warmth", warmth);
    putInt("shadows", shadows);
    putInt("highlights", highlights);
    putBool("bw", blackWhite);
    putBool("sepia", sepia);
    putInt("sharpen", sharpen);
    putInt("vignette", vignette);
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

EditState EditState::fromJson(const QString &json)
{
    EditState e;
    if (json.isEmpty())
        return e;
    const QJsonObject o = QJsonDocument::fromJson(json.toUtf8()).object();
    auto clampInt = [&o](const char *k, int lo, int hi) { return qBound(lo, o.value(QLatin1String(k)).toInt(), hi); };
    e.rotate = clampInt("rotate", 0, 3);
    e.flip = o.value(QStringLiteral("flip")).toBool();
    e.straighten = qBound(-45.0, o.value(QStringLiteral("straighten")).toDouble(), 45.0);
    const QStringList c = o.value(QStringLiteral("crop")).toString().split(QLatin1Char(','));
    if (c.size() == 4) {
        QRectF r(c[0].toDouble(), c[1].toDouble(), c[2].toDouble(), c[3].toDouble());
        r = r.intersected(QRectF(0, 0, 1, 1));
        if (r.width() > 0.001 && r.height() > 0.001)
            e.crop = r;
    }
    e.autoColor = o.value(QStringLiteral("autoColor")).toBool();
    e.autoContrast = o.value(QStringLiteral("autoContrast")).toBool();
    e.brightness = clampInt("brightness", -100, 100);
    e.contrast = clampInt("contrast", -100, 100);
    e.saturation = clampInt("saturation", -100, 100);
    e.warmth = clampInt("warmth", -100, 100);
    e.shadows = clampInt("shadows", 0, 100);
    e.highlights = clampInt("highlights", 0, 100);
    e.blackWhite = o.value(QStringLiteral("bw")).toBool();
    e.sepia = o.value(QStringLiteral("sepia")).toBool();
    e.sharpen = clampInt("sharpen", 0, 100);
    e.vignette = clampInt("vignette", 0, 100);
    return e;
}

namespace Edits {

namespace {

inline int clamp255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }
inline int clamp255(double v) { return v < 0 ? 0 : (v > 255 ? 255 : int(v + 0.5)); }

// Межі гістограми з відсіканням крайніх clip часток
void histogramBounds(const std::array<int, 256> &h, qint64 total, double clip, int &lo, int &hi)
{
    const qint64 cut = qint64(total * clip);
    qint64 acc = 0;
    lo = 0;
    for (int i = 0; i < 256; ++i) {
        acc += h[size_t(i)];
        if (acc > cut) { lo = i; break; }
    }
    acc = 0;
    hi = 255;
    for (int i = 255; i >= 0; --i) {
        acc += h[size_t(i)];
        if (acc > cut) { hi = i; break; }
    }
    if (hi - lo < 16) { // майже однотонне фото — не розтягуємо
        lo = 0;
        hi = 255;
    }
}

QImage sharpenImage(const QImage &src, int amount)
{
    // Нерізка маска з ядром 3x3
    const double k = amount / 100.0 * 1.5;
    QImage dst = src;
    const int w = src.width(), h = src.height();
    for (int y = 1; y < h - 1; ++y) {
        const QRgb *up = reinterpret_cast<const QRgb *>(src.constScanLine(y - 1));
        const QRgb *mid = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        const QRgb *dn = reinterpret_cast<const QRgb *>(src.constScanLine(y + 1));
        QRgb *out = reinterpret_cast<QRgb *>(dst.scanLine(y));
        for (int x = 1; x < w - 1; ++x) {
            auto blur = [&](int (*ch)(QRgb)) {
                return (ch(up[x - 1]) + 2 * ch(up[x]) + ch(up[x + 1]) + 2 * ch(mid[x - 1]) + 4 * ch(mid[x])
                        + 2 * ch(mid[x + 1]) + ch(dn[x - 1]) + 2 * ch(dn[x]) + ch(dn[x + 1]))
                       / 16.0;
            };
            const QRgb c = mid[x];
            const int r = clamp255(qRed(c) + k * (qRed(c) - blur(qRed)));
            const int g = clamp255(qGreen(c) + k * (qGreen(c) - blur(qGreen)));
            const int b = clamp255(qBlue(c) + k * (qBlue(c) - blur(qBlue)));
            out[x] = qRgba(r, g, b, qAlpha(c));
        }
    }
    return dst;
}

} // namespace

QImage applyGeometry(const QImage &src, const EditState &e, bool includeCrop)
{
    QImage img = src;
    if (e.rotate || e.flip) {
        QTransform t;
        if (e.flip)
            t.scale(-1, 1);
        if (e.rotate)
            t.rotate(90 * e.rotate);
        img = img.transformed(t);
    }
    if (e.straighten != 0) {
        const double a = qDegreesToRadians(std::abs(e.straighten));
        const double w = img.width(), h = img.height();
        // найбільший прямокутник тих самих пропорцій, що вміщується в повернуте фото
        const double s = std::min(w / (w * std::cos(a) + h * std::sin(a)), h / (w * std::sin(a) + h * std::cos(a)));
        const QImage rotated = img.convertToFormat(QImage::Format_ARGB32_Premultiplied)
                                   .transformed(QTransform().rotate(e.straighten), Qt::SmoothTransformation);
        const QSize inner(qMax(1, int(w * s)), qMax(1, int(h * s)));
        const QRect r(QPoint((rotated.width() - inner.width()) / 2, (rotated.height() - inner.height()) / 2), inner);
        img = rotated.copy(r);
    }
    if (includeCrop && !e.crop.isEmpty()) {
        const QRect r = QRectF(e.crop.x() * img.width(), e.crop.y() * img.height(), e.crop.width() * img.width(),
                               e.crop.height() * img.height())
                            .toAlignedRect()
                            .intersected(img.rect());
        if (!r.isEmpty())
            img = img.copy(r);
    }
    return img;
}

void rotate(EditState &e, bool clockwise)
{
    // у конвеєрі спершу поворот, потім віддзеркалення — тому з flip напрям обертання протилежний
    const bool cw = e.flip ? !clockwise : clockwise;
    e.rotate = (e.rotate + (cw ? 1 : 3)) % 4;
    if (!e.crop.isEmpty()) {
        const QRectF c = e.crop;
        e.crop = clockwise ? QRectF(1 - c.y() - c.height(), c.x(), c.height(), c.width())
                           : QRectF(c.y(), 1 - c.x() - c.width(), c.height(), c.width());
    }
}

void mirror(EditState &e)
{
    e.flip = !e.flip;
    e.straighten = -e.straighten;
    if (!e.crop.isEmpty())
        e.crop.moveLeft(1 - e.crop.x() - e.crop.width());
}

QSize editedSize(const QSize &src, const EditState &e)
{
    if (!src.isValid())
        return src;
    double w = src.width(), h = src.height();
    if (e.rotate % 2)
        std::swap(w, h);
    if (e.straighten != 0) {
        const double a = qDegreesToRadians(std::abs(e.straighten));
        const double s = std::min(w / (w * std::cos(a) + h * std::sin(a)), h / (w * std::sin(a) + h * std::cos(a)));
        w *= s;
        h *= s;
    }
    if (!e.crop.isEmpty()) {
        w *= e.crop.width();
        h *= e.crop.height();
    }
    return QSize(qMax(1, int(w)), qMax(1, int(h)));
}

QImage apply(const QImage &src, const EditState &e)
{
    if (src.isNull() || e.isIdentity())
        return src;

    QImage img = applyGeometry(src, e).convertToFormat(QImage::Format_ARGB32);
    const int w = img.width(), h = img.height();
    if (w == 0 || h == 0)
        return img;

    const bool tone = e.autoColor || e.autoContrast || e.brightness || e.contrast || e.saturation || e.warmth
                      || e.shadows || e.highlights || e.blackWhite || e.sepia || e.vignette;
    if (tone) {
        // --- 1. Таблиці (LUT) для кожного каналу
        std::array<std::array<int, 256>, 3> lut;
        for (int c = 0; c < 3; ++c)
            for (int i = 0; i < 256; ++i)
                lut[size_t(c)][size_t(i)] = i;

        if (e.autoColor || e.autoContrast) {
            std::array<std::array<int, 256>, 4> hist{}; // r, g, b, luma
            const int step = qMax(1, int(std::sqrt(double(w) * h / 250000.0))); // вибірка для швидкості
            qint64 total = 0;
            for (int y = 0; y < h; y += step) {
                const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
                for (int x = 0; x < w; x += step) {
                    const QRgb p = line[x];
                    ++hist[0][size_t(qRed(p))];
                    ++hist[1][size_t(qGreen(p))];
                    ++hist[2][size_t(qBlue(p))];
                    ++hist[3][size_t(qGray(p))];
                    ++total;
                }
            }
            for (int c = 0; c < 3; ++c) {
                int lo, hi;
                histogramBounds(e.autoColor ? hist[size_t(c)] : hist[3], total, 0.005, lo, hi);
                for (int i = 0; i < 256; ++i)
                    lut[size_t(c)][size_t(i)] = clamp255((i - lo) * 255.0 / (hi - lo));
            }
        }

        const double contrast = (100.0 + e.contrast) / 100.0;
        const double cf = contrast * contrast; // сильніша дія ближче до країв
        for (int c = 0; c < 3; ++c) {
            for (int i = 0; i < 256; ++i) {
                double v = lut[size_t(c)][size_t(i)];
                // тіні / світлі ділянки
                const double t = v / 255.0;
                if (e.shadows)
                    v += e.shadows / 100.0 * 90.0 * (1 - t) * (1 - t) * t * 4;
                if (e.highlights)
                    v -= e.highlights / 100.0 * 90.0 * t * t * (1 - t) * 4;
                v += e.brightness * 1.1;
                v = (v - 128) * cf + 128;
                if (c == 0)
                    v += e.warmth * 0.35;
                else if (c == 2)
                    v -= e.warmth * 0.35;
                lut[size_t(c)][size_t(i)] = clamp255(v);
            }
        }

        const double sat = (100.0 + e.saturation) / 100.0;
        const double cx = w / 2.0, cy = h / 2.0;
        const double maxR2 = cx * cx + cy * cy;
        const double vig = e.vignette / 100.0 * 0.85;

        for (int y = 0; y < h; ++y) {
            QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
            const double dy2 = (y - cy) * (y - cy);
            for (int x = 0; x < w; ++x) {
                const QRgb p = line[x];
                double r = lut[0][size_t(qRed(p))];
                double g = lut[1][size_t(qGreen(p))];
                double b = lut[2][size_t(qBlue(p))];
                const double luma = 0.299 * r + 0.587 * g + 0.114 * b;
                if (e.blackWhite || e.sepia) {
                    r = g = b = luma;
                    if (e.sepia) {
                        r = luma * 1.07 + 20;
                        g = luma * 0.95 + 8;
                        b = luma * 0.78;
                    }
                } else if (sat != 1.0) {
                    r = luma + (r - luma) * sat;
                    g = luma + (g - luma) * sat;
                    b = luma + (b - luma) * sat;
                }
                if (vig > 0) {
                    const double d = ((x - cx) * (x - cx) + dy2) / maxR2; // 0 у центрі, 1 у кутах
                    const double f = 1.0 - vig * d * d;
                    r *= f;
                    g *= f;
                    b *= f;
                }
                line[x] = qRgba(clamp255(r), clamp255(g), clamp255(b), qAlpha(p));
            }
        }
    }

    if (e.sharpen > 0 && w > 2 && h > 2)
        img = sharpenImage(img, e.sharpen);
    return img;
}

QImage load(const QString &path, int maxSide, QSize *originalSize)
{
    QImageReader reader(path);
    reader.setAutoTransform(true); // лише щоб правильно ПОКАЗАТИ (файл не змінюється)
    QSize s = reader.size();
    if (reader.transformation() & QImageIOHandler::TransformationRotate90)
        s.transpose();
    if (originalSize)
        *originalSize = s;
    if (maxSide > 0 && s.isValid() && (s.width() > maxSide || s.height() > maxSide)) {
        QSize scaled = s;
        scaled.scale(maxSide, maxSide, Qt::KeepAspectRatio);
        if (reader.transformation() & QImageIOHandler::TransformationRotate90)
            scaled.transpose(); // scaledSize задається ДО повороту
        reader.setScaledSize(scaled);
    }
    QImage img = reader.read();
    if (!img.isNull() && maxSide > 0 && (img.width() > maxSide || img.height() > maxSide))
        img = img.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (originalSize && !originalSize->isValid())
        *originalSize = img.size();
    return img;
}

} // namespace Edits
