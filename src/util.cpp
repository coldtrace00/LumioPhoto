#include "util.h"
#include "i18n.h"

#include "library.h"

#include <QClipboard>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageWriter>
#include <QProcess>
#include <QUrl>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace Util {

bool naturalLess(const QString &a, const QString &b)
{
    int i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i].isDigit() && b[j].isDigit()) {
            int si = i, sj = j;
            while (si < a.size() && a[si] == QLatin1Char('0')) ++si;
            while (sj < b.size() && b[sj] == QLatin1Char('0')) ++sj;
            int ei = si, ej = sj;
            while (ei < a.size() && a[ei].isDigit()) ++ei;
            while (ej < b.size() && b[ej].isDigit()) ++ej;
            if (ei - si != ej - sj)
                return ei - si < ej - sj;
            const int c = QStringView(a).mid(si, ei - si).compare(QStringView(b).mid(sj, ej - sj));
            if (c != 0)
                return c < 0;
            i = ei;
            j = ej;
            continue;
        }
        const QChar ca = a[i].toCaseFolded(), cb = b[j].toCaseFolded();
        if (ca != cb)
            return QString::localeAwareCompare(QString(ca), QString(cb)) < 0;
        ++i;
        ++j;
    }
    if ((a.size() - i) != (b.size() - j))
        return (a.size() - i) < (b.size() - j);
    return a < b;
}

QString humanSize(qint64 bytes)
{
    if (bytes < 1024)
        return tr2("%1 Б", "%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return tr2("%1 КБ", "%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024)
        return tr2("%1 МБ", "%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return tr2("%1 ГБ", "%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

bool moveToTrash(const QString &path, QString *error)
{
    if (QFile::moveToTrash(path))
        return true;
    if (error)
        *error = tr2("Не вдалося перемістити в кошик: %1", "Could not move to the Recycle Bin: %1").arg(QDir::toNativeSeparators(path));
    return false;
}

void showInFolder(const QString &path)
{
#ifdef Q_OS_WIN
    QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,"), QDir::toNativeSeparators(path)});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

void copyImage(const QImage &img)
{
    QGuiApplication::clipboard()->setImage(img);
}

bool saveImage(const QImage &img, const QString &path, int jpegQuality, QString *error)
{
    if (img.isNull()) {
        if (error)
            *error = tr2("Порожнє зображення", "Empty image");
        return false;
    }
    const QString suffix = QFileInfo(path).suffix().toLower();
    QByteArray fmt = "PNG";
    QImage out = img;
    if (suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg") || suffix == QLatin1String("jfif")) {
        fmt = "JPEG";
        out = out.convertToFormat(QImage::Format_RGB888);
    } else if (suffix == QLatin1String("bmp")) {
        fmt = "BMP";
        out = out.convertToFormat(QImage::Format_RGB32);
    } else if (suffix == QLatin1String("webp")) {
        fmt = "WEBP";
    } else if (suffix == QLatin1String("tif") || suffix == QLatin1String("tiff")) {
        fmt = "TIFF";
    }
    // Прибрати будь-який текст/метадані, які могли «прилипнути» до QImage
    for (const QString &k : out.textKeys())
        out.setText(k, QString());
    out.setDotsPerMeterX(0);
    out.setDotsPerMeterY(0);

    // Пишемо у тимчасовий файл і лише потім замінюємо — щоб збій не зіпсував оригінал
    const QString tmp = path + QStringLiteral(".lumio-tmp");
    QImageWriter w(tmp, fmt);
    if (fmt == "JPEG" || fmt == "WEBP")
        w.setQuality(jpegQuality);
    if (!w.write(out)) {
        if (error)
            *error = w.errorString();
        QFile::remove(tmp);
        return false;
    }
    if (QFile::exists(path) && !QFile::remove(path)) {
        QFile::remove(tmp);
        if (error)
            *error = tr2("Файл зайнятий іншою програмою", "The file is in use by another program");
        return false;
    }
    if (!QFile::rename(tmp, path)) {
        if (error)
            *error = tr2("Не вдалося записати файл", "Could not write the file");
        return false;
    }
    return true;
}

bool setWallpaper(const QImage &img, QString *error)
{
#ifdef Q_OS_WIN
    const QString path = AppPaths::dataDir() + QStringLiteral("/wallpaper.jpg");
    if (!saveImage(img, path, 95, error))
        return false;
    std::wstring w = QDir::toNativeSeparators(path).toStdWString();
    if (!SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, w.data(), SPIF_UPDATEINIFILE | SPIF_SENDCHANGE)) {
        if (error)
            *error = tr2("Windows не дозволила змінити шпалери", "Windows did not allow changing the wallpaper");
        return false;
    }
    return true;
#else
    Q_UNUSED(img)
    if (error)
        *error = tr2("Доступно лише у Windows", "Available only on Windows");
    return false;
#endif
}

QString backupPathFor(const QString &photoPath)
{
    const QByteArray h = QCryptographicHash::hash(photoPath.toUtf8(), QCryptographicHash::Sha1).toHex().left(16);
    return AppPaths::originalsDir() + QLatin1Char('/') + QString::fromLatin1(h) + QLatin1Char('_')
           + QFileInfo(photoPath).fileName();
}

bool hasBackup(const QString &photoPath)
{
    return QFile::exists(backupPathFor(photoPath));
}

} // namespace Util
