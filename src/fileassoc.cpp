#include "fileassoc.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QUrl>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#endif

namespace FileAssoc {

namespace {
const wchar_t kProgId[] = L"LumioPhoto.Image";
const wchar_t kAppName[] = L"Lumio Photo";
const wchar_t kCapabilities[] = L"Software\\LumioPhoto\\Capabilities";

#ifdef Q_OS_WIN
std::wstring w(const QString &s)
{
    return s.toStdWString();
}

void setString(const std::wstring &key, const wchar_t *name, const std::wstring &value)
{
    RegSetKeyValueW(HKEY_CURRENT_USER, key.c_str(), name, REG_SZ, value.c_str(),
                    DWORD((value.size() + 1) * sizeof(wchar_t)));
}

std::wstring getString(const std::wstring &key, const wchar_t *name)
{
    wchar_t buf[2048];
    DWORD size = sizeof(buf);
    if (RegGetValueW(HKEY_CURRENT_USER, key.c_str(), name, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS)
        return {};
    return buf;
}

std::wstring exePath()
{
    return w(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
}

std::wstring openCommand()
{
    return L"\"" + exePath() + L"\" \"%1\"";
}
#endif
} // namespace

QStringList extensions()
{
    return {QStringLiteral(".jpg"), QStringLiteral(".jpeg"), QStringLiteral(".jfif"), QStringLiteral(".png"),
            QStringLiteral(".bmp"), QStringLiteral(".gif"), QStringLiteral(".webp"), QStringLiteral(".tif"),
            QStringLiteral(".tiff"), QStringLiteral(".heic"), QStringLiteral(".heif"), QStringLiteral(".avif")};
}

bool isSupported()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

void registerApp()
{
#ifdef Q_OS_WIN
    const std::wstring classes = L"Software\\Classes\\";
    const std::wstring progId = classes + kProgId;
    const std::wstring icon = L"\"" + exePath() + L"\",0";

    setString(progId, nullptr, L"Lumio Photo");
    setString(progId + L"\\DefaultIcon", nullptr, icon);
    setString(progId + L"\\shell\\open", L"FriendlyAppName", kAppName);
    setString(progId + L"\\shell\\open\\command", nullptr, openCommand());

    const std::wstring app = classes + L"Applications\\LumioPhoto.exe";
    setString(app, L"FriendlyAppName", kAppName);
    setString(app + L"\\DefaultIcon", nullptr, icon);
    setString(app + L"\\shell\\open\\command", nullptr, openCommand());

    setString(kCapabilities, L"ApplicationName", kAppName);
    setString(kCapabilities, L"ApplicationIcon", icon);
    setString(kCapabilities, L"ApplicationDescription",
              L"Fast local photo viewer and organizer / Швидкий переглядач і організатор фото");

    for (const QString &ext : extensions()) {
        const std::wstring e = w(ext);
        setString(classes + e + L"\\OpenWithProgids", kProgId, L"");
        setString(app + L"\\SupportedTypes", e.c_str(), L"");
        setString(std::wstring(kCapabilities) + L"\\FileAssociations", e.c_str(), kProgId);
    }
    setString(L"Software\\RegisteredApplications", kAppName, kCapabilities);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
#endif
}

void ensureRegistered()
{
#ifdef Q_OS_WIN
    const std::wstring current = getString(std::wstring(L"Software\\Classes\\") + kProgId + L"\\shell\\open\\command", nullptr);
    if (current != openCommand())
        registerApp();
#endif
}

void openDefaultAppsSettings()
{
#ifdef Q_OS_WIN
    registerApp();
    // Windows 10/11 не дозволяє програмам самим ставити себе за замовчуванням —
    // відкриваємо сторінку налаштувань саме для Lumio Photo.
    if (!QDesktopServices::openUrl(QUrl(QStringLiteral("ms-settings:defaultapps?registeredAppUser=Lumio%20Photo"))))
        QDesktopServices::openUrl(QUrl(QStringLiteral("ms-settings:defaultapps")));
#endif
}

} // namespace FileAssoc
