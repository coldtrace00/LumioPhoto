#include "exif.h"
#include "i18n.h"

#include <QFile>

#include <cmath>

namespace Exif {

namespace {

// Безпечний читач: кожне звернення перевіряє межі буфера
class Reader
{
public:
    Reader(const QByteArray &d, bool le) : m_d(d), m_le(le) {}

    bool has(qint64 off, qint64 len) const { return off >= 0 && len >= 0 && off + len <= m_d.size(); }

    quint16 u16(qint64 off) const
    {
        if (!has(off, 2))
            return 0;
        const auto *p = reinterpret_cast<const uchar *>(m_d.constData() + off);
        return m_le ? quint16(p[0] | (p[1] << 8)) : quint16((p[0] << 8) | p[1]);
    }

    quint32 u32(qint64 off) const
    {
        if (!has(off, 4))
            return 0;
        const auto *p = reinterpret_cast<const uchar *>(m_d.constData() + off);
        return m_le ? (quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16) | (quint32(p[3]) << 24))
                    : ((quint32(p[0]) << 24) | (quint32(p[1]) << 16) | (quint32(p[2]) << 8) | quint32(p[3]));
    }

    const QByteArray &data() const { return m_d; }

private:
    const QByteArray &m_d;
    bool m_le;
};

struct Entry {
    quint16 tag = 0;
    quint16 type = 0;
    quint32 count = 0;
    qint64 valueOffset = 0; // де лежать дані (всередині запису або за зміщенням)
};

int typeSize(quint16 type)
{
    switch (type) {
    case 1: case 2: case 6: case 7: return 1;
    case 3: case 8: return 2;
    case 4: case 9: case 11: return 4;
    case 5: case 10: case 12: return 8;
    default: return 0;
    }
}

QVector<Entry> readIfd(const Reader &r, qint64 off)
{
    QVector<Entry> out;
    if (!r.has(off, 2))
        return out;
    const int n = r.u16(off);
    if (n > 1000)
        return out; // явно пошкоджено
    for (int i = 0; i < n; ++i) {
        const qint64 e = off + 2 + qint64(i) * 12;
        if (!r.has(e, 12))
            break;
        Entry en;
        en.tag = r.u16(e);
        en.type = r.u16(e + 2);
        en.count = r.u32(e + 4);
        const int ts = typeSize(en.type);
        if (ts == 0 || en.count > 100000)
            continue;
        const qint64 bytes = qint64(ts) * en.count;
        en.valueOffset = bytes <= 4 ? e + 8 : qint64(r.u32(e + 8));
        if (!r.has(en.valueOffset, bytes))
            continue;
        out.append(en);
    }
    return out;
}

QString asciiValue(const Reader &r, const Entry &e)
{
    if (e.type != 2)
        return QString();
    QByteArray b = r.data().mid(int(e.valueOffset), int(e.count));
    const int z = b.indexOf('\0');
    if (z >= 0)
        b.truncate(z);
    return QString::fromUtf8(b).trimmed();
}

double rationalValue(const Reader &r, const Entry &e, int index = 0)
{
    if ((e.type != 5 && e.type != 10) || quint32(index) >= e.count)
        return 0;
    const qint64 o = e.valueOffset + qint64(index) * 8;
    if (e.type == 5) {
        const quint32 num = r.u32(o), den = r.u32(o + 4);
        return den ? double(num) / den : 0;
    }
    const qint32 num = qint32(r.u32(o)), den = qint32(r.u32(o + 4));
    return den ? double(num) / den : 0;
}

quint32 intValue(const Reader &r, const Entry &e)
{
    if (e.type == 3)
        return r.u16(e.valueOffset);
    if (e.type == 4)
        return r.u32(e.valueOffset);
    if (e.type == 1)
        return quint8(r.data().at(int(e.valueOffset)));
    return 0;
}

QDateTime parseDate(const QString &s)
{
    // "2024:05:17 14:03:22"
    QDateTime dt = QDateTime::fromString(s.left(19), QStringLiteral("yyyy:MM:dd HH:mm:ss"));
    return dt.isValid() && dt.date().year() > 1900 ? dt : QDateTime();
}

} // namespace

ExifData parseTiff(const QByteArray &tiff)
{
    ExifData out;
    if (tiff.size() < 8)
        return out;
    bool le;
    if (tiff.startsWith("II"))
        le = true;
    else if (tiff.startsWith("MM"))
        le = false;
    else
        return out;
    Reader r(tiff, le);
    if (r.u16(2) != 42)
        return out;

    qint64 exifIfd = 0, gpsIfd = 0;
    QString dateTime;
    const QVector<Entry> ifd0 = readIfd(r, r.u32(4));
    for (const Entry &e : ifd0) {
        switch (e.tag) {
        case 0x010F: out.make = asciiValue(r, e); break;
        case 0x0110: out.model = asciiValue(r, e); break;
        case 0x0112: out.orientation = int(intValue(r, e)); break;
        case 0x0132: dateTime = asciiValue(r, e); break;
        case 0x8769: exifIfd = intValue(r, e); break;
        case 0x8825: gpsIfd = intValue(r, e); break;
        default: break;
        }
    }
    if (exifIfd > 0) {
        for (const Entry &e : readIfd(r, exifIfd)) {
            switch (e.tag) {
            case 0x9003: out.taken = parseDate(asciiValue(r, e)); break;
            case 0x829A: out.exposure = rationalValue(r, e); break;
            case 0x829D: out.fnumber = rationalValue(r, e); break;
            case 0x8827: out.iso = int(intValue(r, e)); break;
            case 0x920A: out.focal = rationalValue(r, e); break;
            case 0xA434: out.lens = asciiValue(r, e); break;
            default: break;
            }
        }
    }
    if (!out.taken.isValid() && !dateTime.isEmpty())
        out.taken = parseDate(dateTime);

    if (gpsIfd > 0) {
        QString latRef, lonRef;
        double lat = -1, lon = -1;
        for (const Entry &e : readIfd(r, gpsIfd)) {
            auto dms = [&](const Entry &en) {
                return rationalValue(r, en, 0) + rationalValue(r, en, 1) / 60.0 + rationalValue(r, en, 2) / 3600.0;
            };
            switch (e.tag) {
            case 1: latRef = asciiValue(r, e); break;
            case 2: if (e.count >= 3) lat = dms(e); break;
            case 3: lonRef = asciiValue(r, e); break;
            case 4: if (e.count >= 3) lon = dms(e); break;
            default: break;
            }
        }
        if (lat >= 0 && lon >= 0 && lat <= 90 && lon <= 180) {
            out.hasGps = true;
            out.latitude = latRef == QLatin1String("S") ? -lat : lat;
            out.longitude = lonRef == QLatin1String("W") ? -lon : lon;
        }
    }
    if (out.orientation < 1 || out.orientation > 8)
        out.orientation = 1;
    out.valid = true;
    return out;
}

ExifData read(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return ExifData();
    const QByteArray head = f.read(256 * 1024); // EXIF завжди на початку JPEG
    if (head.size() < 4 || uchar(head[0]) != 0xFF || uchar(head[1]) != 0xD8)
        return ExifData();

    int pos = 2;
    while (pos + 4 <= head.size()) {
        if (uchar(head[pos]) != 0xFF)
            break;
        const uchar marker = uchar(head[pos + 1]);
        if (marker == 0xD8 || (marker >= 0xD0 && marker <= 0xD7)) {
            pos += 2;
            continue;
        }
        if (marker == 0xDA || marker == 0xD9) // початок даних зображення — далі EXIF не буде
            break;
        const int len = (uchar(head[pos + 2]) << 8) | uchar(head[pos + 3]);
        if (len < 2)
            break;
        if (marker == 0xE1 && pos + 4 + 6 <= head.size() && head.mid(pos + 4, 6) == QByteArray("Exif\0\0", 6)) {
            const int start = pos + 10;
            const int size = qMin(len - 8, int(head.size()) - start);
            if (size > 0)
                return parseTiff(head.mid(start, size));
            break;
        }
        pos += 2 + len;
    }
    return ExifData();
}

QString describeExposure(double s)
{
    if (s <= 0)
        return QString();
    if (s >= 1)
        return tr2("%1 с", "%1 s").arg(s, 0, 'g', 3);
    return tr2("1/%1 с", "1/%1 s").arg(qRound(1.0 / s));
}

} // namespace Exif
