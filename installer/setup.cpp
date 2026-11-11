// Інсталятор в одному .exe (чистий Win32 + miniz, без Qt) — спільний для Lumio і Lumio Photo.
// Усе, що відрізняється між програмами, описано в appinfo.h.
//
// Той самий код збирається двічі:
//   * <Program>-Setup.exe — з ресурсами PAYLOAD (zip з програмою) та UNINSTALLER;
//   * Uninstall.exe       — з макросом LUMIO_UNINSTALLER (видаляє встановлену програму).
//
// Встановлення не потребує прав адміністратора: за замовчуванням
// %LOCALAPPDATA%\Programs\<Program>, реєстр — лише HKEY_CURRENT_USER.
// Мова інтерфейсу (українська / англійська) вибирається у вікні інсталятора
// і передається програмі.

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include <knownfolders.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <tlhelp32.h>

#include <string>
#include <vector>

#include "appinfo.h"
#include "resource.h"

#ifndef LUMIO_UNINSTALLER
#include "miniz.h"
#endif

namespace {

const wchar_t *kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t *kManifestName = L"uninstall.lst";

std::wstring uninstallKey()
{
    return std::wstring(L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\") + kApp.regKey;
}

// ---------------------------------------------------------------------------
// Мова

bool g_uk = true;

const wchar_t *T(const wchar_t *uk, const wchar_t *en)
{
    return g_uk ? uk : en;
}

bool windowsIsUkrainian()
{
    const LANGID ui = GetUserDefaultUILanguage();
    const LANGID loc = LANGIDFROMLCID(GetUserDefaultLCID());
    return PRIMARYLANGID(ui) == LANG_UKRAINIAN || PRIMARYLANGID(loc) == LANG_UKRAINIAN;
}

// ---------------------------------------------------------------------------
// Допоміжні функції

std::wstring knownFolder(REFKNOWNFOLDERID id)
{
    PWSTR p = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p)) && p)
        out = p;
    CoTaskMemFree(p);
    return out;
}

std::wstring joinPath(const std::wstring &a, const std::wstring &b)
{
    if (a.empty())
        return b;
    if (a.back() == L'\\' || a.back() == L'/')
        return a + b;
    return a + L"\\" + b;
}

std::wstring parentDir(const std::wstring &p)
{
    const size_t pos = p.find_last_of(L"\\/");
    return pos == std::wstring::npos ? std::wstring() : p.substr(0, pos);
}

[[maybe_unused]] std::wstring fileNameOf(const std::wstring &p)
{
    const size_t pos = p.find_last_of(L"\\/");
    return pos == std::wstring::npos ? p : p.substr(pos + 1);
}

[[maybe_unused]] std::wstring selfPath()
{
    std::wstring buf(32768, L'\0');
    const DWORD n = GetModuleFileNameW(nullptr, &buf[0], DWORD(buf.size()));
    buf.resize(n);
    return buf;
}

bool ensureDir(const std::wstring &dir)
{
    const int r = SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
    return r == ERROR_SUCCESS || r == ERROR_ALREADY_EXISTS || r == ERROR_FILE_EXISTS;
}

[[maybe_unused]] bool writeFile(const std::wstring &path, const void *data, size_t size)
{
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    const char *p = static_cast<const char *>(data);
    bool ok = true;
    while (size > 0) {
        DWORD chunk = DWORD(size > (1u << 30) ? (1u << 30) : size);
        DWORD written = 0;
        if (!WriteFile(h, p, chunk, &written, nullptr) || written == 0) {
            ok = false;
            break;
        }
        p += written;
        size -= written;
    }
    CloseHandle(h);
    return ok;
}

std::wstring fromUtf8(const std::string &s)
{
    if (s.empty())
        return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), int(s.size()), nullptr, 0);
    std::wstring w(size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), int(s.size()), &w[0], n);
    return w;
}

[[maybe_unused]] bool readFileUtf8(const std::wstring &path, std::string &out)
{
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    LARGE_INTEGER size;
    GetFileSizeEx(h, &size);
    out.resize(size_t(size.QuadPart));
    DWORD read = 0;
    const BOOL ok = out.empty() || ReadFile(h, &out[0], DWORD(out.size()), &read, nullptr);
    CloseHandle(h);
    return ok;
}

void killRunningApp()
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            for (const wchar_t *const *n = kApp.killNames; *n; ++n) {
                if (_wcsicmp(pe.szExeFile, *n) == 0 && pe.th32ProcessID != GetCurrentProcessId()) {
                    if (HANDLE p = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pe.th32ProcessID)) {
                        TerminateProcess(p, 0);
                        WaitForSingleObject(p, 3000);
                        CloseHandle(p);
                    }
                }
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
}

[[maybe_unused]] void setRegString(HKEY root, const std::wstring &key, const wchar_t *name, const std::wstring &value)
{
    HKEY h;
    if (RegCreateKeyExW(root, key.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &h, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(h, name, 0, REG_SZ, reinterpret_cast<const BYTE *>(value.c_str()),
                       DWORD((value.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(h);
    }
}

[[maybe_unused]] void setRegDword(HKEY root, const std::wstring &key, const wchar_t *name, DWORD value)
{
    HKEY h;
    if (RegCreateKeyExW(root, key.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &h, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(h, name, 0, REG_DWORD, reinterpret_cast<const BYTE *>(&value), sizeof(value));
        RegCloseKey(h);
    }
}

void deleteRegValue(HKEY root, const wchar_t *key, const wchar_t *name)
{
    HKEY h;
    if (RegOpenKeyExW(root, key, 0, KEY_SET_VALUE, &h) == ERROR_SUCCESS) {
        RegDeleteValueW(h, name);
        RegCloseKey(h);
    }
}

[[maybe_unused]] bool createShortcut(const std::wstring &lnkPath, const std::wstring &target, const std::wstring &args,
                                     const std::wstring &workDir, const std::wstring &description)
{
    IShellLinkW *link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                                reinterpret_cast<void **>(&link))))
        return false;
    link->SetPath(target.c_str());
    link->SetArguments(args.c_str());
    link->SetWorkingDirectory(workDir.c_str());
    link->SetDescription(description.c_str());
    link->SetIconLocation(target.c_str(), 0);
    IPersistFile *pf = nullptr;
    bool ok = false;
    if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&pf)))) {
        ok = SUCCEEDED(pf->Save(lnkPath.c_str(), TRUE));
        pf->Release();
    }
    link->Release();
    return ok;
}

std::wstring shortcutName() { return std::wstring(kApp.name) + L".lnk"; }
std::wstring startMenuShortcut() { return joinPath(knownFolder(FOLDERID_Programs), shortcutName()); }
std::wstring desktopShortcut() { return joinPath(knownFolder(FOLDERID_Desktop), shortcutName()); }

// Файл налаштувань програми (той самий, що читає QSettings)
std::wstring settingsIni(const std::wstring &installDir, bool portable)
{
    if (portable)
        return joinPath(installDir, kApp.portableSettingsFile);
    const std::wstring base = knownFolder(kApp.settingsInLocalAppData ? FOLDERID_LocalAppData : FOLDERID_RoamingAppData);
    return joinPath(base, kApp.settingsFile);
}

// Записати налаштування інтерфейсу в ini програми (секція [ui]). Робимо це вручну (UTF-8), а не через
// WritePrivateProfileString: та працює з ANSI і могла б зіпсувати кирилицю в інших налаштуваннях.
[[maybe_unused]] void writeUiSettings(const std::wstring &installDir, bool portable,
                                      const std::vector<std::pair<std::string, std::string>> &values)
{
    const std::wstring ini = settingsIni(installDir, portable);
    ensureDir(parentDir(ini));

    std::string text;
    readFileUtf8(ini, text);
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        lines.push_back(line);
        start = end + 1;
    }

    for (const auto &kv : values) {
        const std::string entry = kv.first + "=" + kv.second;
        bool inUi = false, done = false;
        size_t uiHeader = std::string::npos;
        for (size_t i = 0; i < lines.size(); ++i) {
            const std::string &l = lines[i];
            if (!l.empty() && l[0] == '[') {
                if (inUi) { // секція [ui] закінчилась, а ключа в ній не було
                    size_t pos = i;
                    while (pos > 0 && lines[pos - 1].empty()) // перед порожнім рядком між секціями
                        --pos;
                    lines.insert(lines.begin() + long(pos), entry);
                    done = true;
                    break;
                }
                inUi = (l == "[ui]");
                if (inUi)
                    uiHeader = i;
                continue;
            }
            if (inUi && l.rfind(kv.first + "=", 0) == 0) {
                lines[i] = entry;
                done = true;
                break;
            }
        }
        if (!done) {
            if (uiHeader == std::string::npos) {
                if (!lines.empty() && !lines.back().empty())
                    lines.push_back(std::string());
                lines.push_back("[ui]");
            }
            lines.push_back(entry); // [ui] — остання секція
        }
    }

    std::string out;
    for (const std::string &l : lines)
        out += l + "\r\n";
    writeFile(ini, out.data(), out.size());
}

[[maybe_unused]] int dpiScale(int v)
{
    static int dpi = 0;
    if (!dpi) {
        HDC dc = GetDC(nullptr);
        dpi = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(nullptr, dc);
        if (dpi <= 0)
            dpi = 96;
    }
    return MulDiv(v, dpi, 96);
}

[[maybe_unused]] HFONT makeFont(int pt, int weight)
{
    HDC dc = GetDC(nullptr);
    const int h = -MulDiv(pt, GetDeviceCaps(dc, LOGPIXELSY), 72);
    ReleaseDC(nullptr, dc);
    return CreateFontW(h, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
}

} // namespace

#ifdef LUMIO_UNINSTALLER
// ===========================================================================
// Деінсталятор

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const std::wstring self = selfPath();
    const std::wstring dir = parentDir(self);

    // мова — та, яку вибрали при встановленні (або мова Windows)
    std::string iniText;
    g_uk = windowsIsUkrainian();
    if (readFileUtf8(settingsIni(dir, false), iniText)) {
        if (iniText.find("language=uk") != std::string::npos)
            g_uk = true;
        else if (iniText.find("language=en") != std::string::npos)
            g_uk = false;
    }

    const std::wstring title = std::wstring(T(L"Видалення ", L"Uninstall ")) + kApp.name;
    const std::wstring ask = std::wstring(T(L"Видалити ", L"Remove ")) + kApp.name
                             + T(L" з цього комп'ютера?\n\n", L" from this computer?\n\n") + T(kApp.keepUk, kApp.keepEn);
    if (MessageBoxW(nullptr, ask.c_str(), title.c_str(), MB_YESNO | MB_ICONQUESTION) != IDYES)
        return 0;

    killRunningApp();

    // Видаляємо лише ті файли, які поставив інсталятор
    std::string manifest;
    if (readFileUtf8(joinPath(dir, kManifestName), manifest)) {
        std::vector<std::wstring> dirs;
        size_t start = 0;
        while (start < manifest.size()) {
            size_t end = manifest.find('\n', start);
            if (end == std::string::npos)
                end = manifest.size();
            std::string line = manifest.substr(start, end - start);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            start = end + 1;
            if (line.empty() || line.find("..") != std::string::npos)
                continue;
            const std::wstring full = joinPath(dir, fromUtf8(line));
            DeleteFileW(full.c_str());
            for (std::wstring d = parentDir(full); d.size() > dir.size(); d = parentDir(d))
                dirs.push_back(d);
        }
        for (int pass = 0; pass < 6; ++pass)
            for (const std::wstring &d : dirs)
                RemoveDirectoryW(d.c_str());
    }
    DeleteFileW(joinPath(dir, kManifestName).c_str());
    DeleteFileW(joinPath(dir, L"portable.txt").c_str());

    DeleteFileW(startMenuShortcut().c_str());
    DeleteFileW(desktopShortcut().c_str());
    RegDeleteKeyW(HKEY_CURRENT_USER, uninstallKey().c_str());
    deleteRegValue(HKEY_CURRENT_USER, kRunKey, kApp.regKey);

    // реєстрація для «Відкрити за допомогою» / «Програми за замовчуванням»
    if (kApp.assocProgId) {
        for (const wchar_t *const *e = kApp.assocExts; e && *e; ++e)
            deleteRegValue(HKEY_CURRENT_USER, (std::wstring(L"Software\\Classes\\") + *e + L"\\OpenWithProgids").c_str(),
                           kApp.assocProgId);
        deleteRegValue(HKEY_CURRENT_USER, L"Software\\RegisteredApplications", kApp.registeredAppName);
        for (const wchar_t *const *k = kApp.cleanupKeys; k && *k; ++k)
            RegDeleteTreeW(HKEY_CURRENT_USER, *k);
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }

    // Дані програми (бібліотека, налаштування) — лише за згодою і в кошик
    if (kApp.dataDirName) {
        const std::wstring dataDir = joinPath(knownFolder(kApp.settingsInLocalAppData ? FOLDERID_LocalAppData
                                                                                       : FOLDERID_RoamingAppData),
                                              kApp.dataDirName);
        if (GetFileAttributesW(dataDir.c_str()) != INVALID_FILE_ATTRIBUTES
            && MessageBoxW(nullptr, T(kApp.deleteDataUk, kApp.deleteDataEn), title.c_str(),
                           MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES) {
            std::wstring from = dataDir;
            from.push_back(L'\0'); // SHFileOperation потребує подвійного нуля
            SHFILEOPSTRUCTW op = {};
            op.wFunc = FO_DELETE;
            op.pFrom = from.c_str();
            op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
            SHFileOperationW(&op);
        }
    }

    // Сам себе файл видалити не може — просимо cmd зробити це після виходу
    std::wstring cmd = L"cmd.exe /c ping 127.0.0.1 -n 3 > nul & del /f /q \"" + self + L"\" & rmdir \"" + dir + L"\"";
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    wchar_t sysDir[MAX_PATH];
    GetSystemDirectoryW(sysDir, MAX_PATH);
    if (CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, sysDir, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }

    const std::wstring done = std::wstring(kApp.name) + T(L" видалено.", L" has been removed.");
    MessageBoxW(nullptr, done.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
    CoUninitialize();
    return 0;
}

#else
// ===========================================================================
// Інсталятор

namespace {

enum {
    IDC_LANG = 100,
    IDC_THEME,
    IDC_MODE_INSTALL,
    IDC_MODE_PORTABLE,
    IDC_PATH_LABEL,
    IDC_PATH,
    IDC_BROWSE,
    IDC_DESKTOP,
    IDC_AUTOSTART,
    IDC_LAUNCH,
    IDC_PROGRESS,
    IDC_STATUS,
    IDC_INSTALL,
    IDC_CANCEL,
};

const UINT WM_APP_PROGRESS = WM_APP + 1;
const UINT WM_APP_DONE = WM_APP + 2;

struct InstallJob {
    HWND hwnd;
    std::wstring dir;
    bool portable;
    bool desktop;
    bool autostart;
    bool dark;
    std::wstring error;
};

HWND g_hwnd = nullptr;
HFONT g_font = nullptr, g_titleFont = nullptr, g_subFont = nullptr;
HICON g_logo = nullptr;
HBRUSH g_white = nullptr;
bool g_pathEdited = false;
bool g_busy = false;
bool g_done = false;
std::wstring g_doneDir;

// Режим оновлення: <Setup>.exe --update --dir "<папка програми>" [--portable]
// Запускається з самої програми (кнопка «Оновити»). Ставить нову версію в ту саму
// папку, не питаючи нічого, зберігає налаштування, ярлики й автозапуск, потім запускає програму.
bool g_update = false;
bool g_updatePortable = false;
std::wstring g_updateDir;

void parseCommandLine()
{
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
        return;
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        if (a == L"--update")
            g_update = true;
        else if (a == L"--portable")
            g_updatePortable = true;
        else if (a == L"--dir" && i + 1 < argc)
            g_updateDir = argv[++i];
    }
    LocalFree(argv);
    while (!g_updateDir.empty() && (g_updateDir.back() == L'\\' || g_updateDir.back() == L'/'))
        g_updateDir.pop_back();
    if (g_updateDir.size() < 3 || GetFileAttributesW(joinPath(g_updateDir, kApp.exe).c_str()) == INVALID_FILE_ATTRIBUTES)
        g_update = false; // немає що оновлювати — звичайне встановлення
}

bool regValueExists(HKEY root, const wchar_t *key, const wchar_t *name)
{
    return RegGetValueW(root, key, name, RRF_RT_ANY, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
}

std::wstring defaultDir(bool portable)
{
    if (portable)
        return joinPath(knownFolder(FOLDERID_Desktop), kApp.portableDirName);
    return joinPath(joinPath(knownFolder(FOLDERID_LocalAppData), L"Programs"), kApp.dirName);
}

HWND item(int id) { return GetDlgItem(g_hwnd, id); }

bool isChecked(int id) { return SendMessageW(item(id), BM_GETCHECK, 0, 0) == BST_CHECKED; }

std::wstring getText(int id)
{
    const int n = GetWindowTextLengthW(item(id));
    std::wstring s(size_t(n) + 1, L'\0');
    GetWindowTextW(item(id), &s[0], n + 1);
    s.resize(size_t(n));
    while (!s.empty() && (s.back() == L' ' || s.back() == L'"' || s.back() == L'\\'))
        s.pop_back();
    while (!s.empty() && (s.front() == L' ' || s.front() == L'"'))
        s.erase(s.begin());
    return s;
}

void setPathText(const std::wstring &p)
{
    SetWindowTextW(item(IDC_PATH), p.c_str());
    g_pathEdited = false;
}

const void *resourceData(int id, DWORD &size)
{
    HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10)); // RT_RCDATA
    if (!r)
        return nullptr;
    HGLOBAL g = LoadResource(nullptr, r);
    size = SizeofResource(nullptr, r);
    return g ? LockResource(g) : nullptr;
}

DWORD WINAPI installThread(LPVOID param)
{
    auto *job = static_cast<InstallJob *>(param);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    killRunningApp();

    DWORD payloadSize = 0;
    const void *payload = resourceData(IDR_PAYLOAD, payloadSize);
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    std::string manifest;
    unsigned long long totalBytes = 0;

    if (!payload || !mz_zip_reader_init_mem(&zip, payload, payloadSize, 0)) {
        job->error = T(L"Інсталятор пошкоджено: не знайдено архів програми.",
                       L"The installer is damaged: the program archive was not found.");
    } else if (!ensureDir(job->dir)) {
        job->error = std::wstring(T(L"Не вдалося створити папку:\n", L"Could not create the folder:\n")) + job->dir
                     + T(L"\n\nВиберіть іншу папку (наприклад, у своїй папці користувача).",
                         L"\n\nChoose another folder (for example, inside your user folder).");
    } else {
        const mz_uint count = mz_zip_reader_get_num_files(&zip);
        for (mz_uint i = 0; i < count && job->error.empty(); ++i) {
            mz_zip_archive_file_stat st;
            if (!mz_zip_reader_file_stat(&zip, i, &st))
                continue;
            std::string name = st.m_filename;
            while (name.rfind("./", 0) == 0)
                name.erase(0, 2);
            if (name.empty() || name.find("..") != std::string::npos)
                continue;
            std::wstring rel = fromUtf8(name);
            for (wchar_t &c : rel)
                if (c == L'/')
                    c = L'\\';
            const std::wstring full = joinPath(job->dir, rel);

            if (mz_zip_reader_is_file_a_directory(&zip, i)) {
                ensureDir(full);
            } else {
                ensureDir(parentDir(full));
                size_t size = 0;
                void *data = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
                if (!data) {
                    job->error = std::wstring(T(L"Не вдалося розпакувати файл:\n", L"Could not extract the file:\n")) + rel;
                    break;
                }
                bool ok = writeFile(full, data, size);
                if (!ok) { // файл може бути ще зайнятий щойно закритою програмою
                    Sleep(700);
                    ok = writeFile(full, data, size);
                }
                mz_free(data);
                if (!ok) {
                    job->error = std::wstring(T(L"Не вдалося записати файл:\n", L"Could not write the file:\n")) + full
                                 + T(L"\n\nМожливо, бракує прав на цю папку.",
                                     L"\n\nYou may not have permission to write to this folder.");
                    break;
                }
                totalBytes += size;
                manifest += name + "\n";
            }
            PostMessageW(job->hwnd, WM_APP_PROGRESS, WPARAM((i + 1) * 100 / (count ? count : 1)), 0);
        }
        mz_zip_reader_end(&zip);
    }

    if (job->error.empty()) {
        const std::wstring exe = joinPath(job->dir, kApp.exe);
        const std::wstring desc = T(kApp.descUk, kApp.descEn);

        DWORD unSize = 0;
        const void *un = resourceData(IDR_UNINSTALLER, unSize);
        if (!job->portable && un && writeFile(joinPath(job->dir, L"Uninstall.exe"), un, unSize))
            manifest += "Uninstall.exe\n";

        const std::wstring portableFlag = joinPath(job->dir, L"portable.txt");
        if (job->portable) {
            writeFile(portableFlag, kApp.portableFileText, strlen(kApp.portableFileText));
        } else {
            DeleteFileW(portableFlag.c_str());
            writeFile(joinPath(job->dir, kManifestName), manifest.data(), manifest.size());
            createShortcut(startMenuShortcut(), exe, L"", job->dir, desc);

            const std::wstring key = uninstallKey();
            setRegString(HKEY_CURRENT_USER, key, L"DisplayName", kApp.name);
            setRegString(HKEY_CURRENT_USER, key, L"DisplayVersion", kApp.version);
            setRegString(HKEY_CURRENT_USER, key, L"Publisher", L"Lumio");
            setRegString(HKEY_CURRENT_USER, key, L"DisplayIcon", exe);
            setRegString(HKEY_CURRENT_USER, key, L"InstallLocation", job->dir);
            setRegString(HKEY_CURRENT_USER, key, L"UninstallString", L"\"" + joinPath(job->dir, L"Uninstall.exe") + L"\"");
            setRegDword(HKEY_CURRENT_USER, key, L"NoModify", 1);
            setRegDword(HKEY_CURRENT_USER, key, L"NoRepair", 1);
            setRegDword(HKEY_CURRENT_USER, key, L"EstimatedSize", DWORD(totalBytes / 1024));
        }

        // вибрані в інсталяторі мова й тема — програмі (при оновленні налаштування не чіпаємо)
        if (!g_update) {
            std::vector<std::pair<std::string, std::string>> ui = {{"language", g_uk ? "uk" : "en"}};
            if (kApp.hasThemeChoice)
                ui.push_back({"theme", job->dark ? "dark" : "light"});
            writeUiSettings(job->dir, job->portable, ui);
        }

        if (job->desktop)
            createShortcut(desktopShortcut(), exe, L"", job->dir, desc);

        if (kApp.hasAutostart) {
            if (job->autostart)
                setRegString(HKEY_CURRENT_USER, kRunKey, kApp.regKey, L"\"" + exe + L"\" " + kApp.autostartArgs);
            else
                deleteRegValue(HKEY_CURRENT_USER, kRunKey, kApp.regKey);
        }
        if (kApp.legacyRunName)
            deleteRegValue(HKEY_CURRENT_USER, kRunKey, kApp.legacyRunName);
    }

    CoUninitialize();
    PostMessageW(job->hwnd, WM_APP_DONE, 0, reinterpret_cast<LPARAM>(job));
    return 0;
}

void browseFolder()
{
    IFileOpenDialog *dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_IFileOpenDialog,
                                reinterpret_cast<void **>(&dlg))))
        return;
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    const std::wstring title = std::wstring(T(L"Куди встановити ", L"Where to install ")) + kApp.name + L"?";
    dlg->SetTitle(title.c_str());
    if (SUCCEEDED(dlg->Show(g_hwnd))) {
        IShellItem *it = nullptr;
        if (SUCCEEDED(dlg->GetResult(&it))) {
            PWSTR p = nullptr;
            if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p) {
                std::wstring dir = p;
                const std::wstring last = fileNameOf(dir);
                if (_wcsicmp(last.c_str(), kApp.dirName) != 0 && _wcsicmp(last.c_str(), kApp.portableDirName) != 0)
                    dir = joinPath(dir, isChecked(IDC_MODE_PORTABLE) ? kApp.portableDirName : kApp.dirName);
                SetWindowTextW(item(IDC_PATH), dir.c_str());
                g_pathEdited = true;
                CoTaskMemFree(p);
            }
            it->Release();
        }
    }
    dlg->Release();
}

void updateStatusText()
{
    if (g_done) {
        const std::wstring s = std::wstring(T(L"Готово! ", L"Done! ")) + kApp.name
                               + T(L" встановлено в:\n", L" is installed in:\n") + g_doneDir;
        SetWindowTextW(item(IDC_STATUS), s.c_str());
        return;
    }
    if (g_busy) {
        SetWindowTextW(item(IDC_STATUS), g_update ? T(L"Оновлення… Програма запуститься автоматично.",
                                                     L"Updating… The program will start automatically.")
                                                  : T(L"Встановлення…", L"Installing…"));
        return;
    }
    const bool portable = isChecked(IDC_MODE_PORTABLE);
    SetWindowTextW(item(IDC_STATUS),
                   portable ? T(kApp.portableHintUk, kApp.portableHintEn)
                            : T(L"Звичайне встановлення: ярлик у меню «Пуск», видалення через «Параметри → Програми». "
                                L"Права адміністратора не потрібні.",
                                L"Regular install: a Start menu shortcut, uninstall via “Settings → Apps”. "
                                L"No administrator rights needed."));
}

// Усі написи — у вибраній мові (викликається і при зміні мови)
void applyTexts()
{
    const std::wstring title = std::wstring(g_update ? T(L"Оновлення ", L"Updating ") : T(L"Встановлення ", L"Setup — "))
                               + kApp.name;
    SetWindowTextW(g_hwnd, title.c_str());
    SetWindowTextW(item(IDC_MODE_INSTALL), T(L"Встановити на цей комп'ютер", L"Install on this computer"));
    SetWindowTextW(item(IDC_MODE_PORTABLE), T(L"Портативна версія (без встановлення)", L"Portable version (no installation)"));
    SetWindowTextW(item(IDC_PATH_LABEL), T(L"Папка:", L"Folder:"));
    SetWindowTextW(item(IDC_BROWSE), T(L"Огляд…", L"Browse…"));
    SetWindowTextW(item(IDC_DESKTOP), T(L"Ярлик на робочому столі", L"Desktop shortcut"));
    SetWindowTextW(item(IDC_AUTOSTART), T(L"Запускати разом з Windows", L"Start with Windows"));
    const std::wstring launch = std::wstring(T(L"Запустити ", L"Launch ")) + kApp.name
                                + T(L" після встановлення", L" after installation");
    SetWindowTextW(item(IDC_LAUNCH), launch.c_str());
    SetWindowTextW(item(IDC_INSTALL), g_done ? T(L"Готово", L"Done") : T(L"Встановити", L"Install"));
    SetWindowTextW(item(IDC_CANCEL), T(L"Скасувати", L"Cancel"));
    // пункти теми — у вибраній мові, зберігаючи вибір
    LRESULT themeSel = SendMessageW(item(IDC_THEME), CB_GETCURSEL, 0, 0);
    if (themeSel < 0)
        themeSel = 0;
    SendMessageW(item(IDC_THEME), CB_RESETCONTENT, 0, 0);
    SendMessageW(item(IDC_THEME), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(T(L"Темна тема", L"Dark theme")));
    SendMessageW(item(IDC_THEME), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(T(L"Світла тема", L"Light theme")));
    SendMessageW(item(IDC_THEME), CB_SETCURSEL, WPARAM(themeSel), 0);
    updateStatusText();
    InvalidateRect(g_hwnd, nullptr, TRUE);
}

void updateModeUi()
{
    if (!g_pathEdited)
        setPathText(defaultDir(isChecked(IDC_MODE_PORTABLE)));
    updateStatusText();
}

void startInstall()
{
    std::wstring dir = getText(IDC_PATH);
    if (dir.size() < 3 || dir[1] != L':') {
        const std::wstring msg = std::wstring(T(L"Вкажіть повний шлях до папки, наприклад C:\\",
                                                L"Enter the full folder path, for example C:\\"))
                                 + kApp.dirName;
        MessageBoxW(g_hwnd, msg.c_str(), kApp.name, MB_ICONWARNING);
        return;
    }
    g_busy = true;
    for (int id : {IDC_LANG, IDC_THEME, IDC_MODE_INSTALL, IDC_MODE_PORTABLE, IDC_PATH, IDC_BROWSE, IDC_DESKTOP, IDC_AUTOSTART, IDC_INSTALL})
        EnableWindow(item(id), FALSE);
    ShowWindow(item(IDC_PROGRESS), SW_SHOW);
    updateStatusText();

    auto *job = new InstallJob{g_hwnd, dir, isChecked(IDC_MODE_PORTABLE), isChecked(IDC_DESKTOP),
                               isChecked(IDC_AUTOSTART),
                               SendMessageW(item(IDC_THEME), CB_GETCURSEL, 0, 0) != 1, std::wstring()};
    HANDLE t = CreateThread(nullptr, 0, installThread, job, 0, nullptr);
    if (t)
        CloseHandle(t);
}

void createControls(HWND hwnd, HINSTANCE inst)
{
    auto make = [&](const wchar_t *cls, DWORD style, int x, int y, int w, int h, int id, DWORD exStyle = 0) {
        HWND c = CreateWindowExW(exStyle, cls, L"", WS_CHILD | WS_VISIBLE | style, dpiScale(x), dpiScale(y), dpiScale(w),
                                 dpiScale(h), hwnd, reinterpret_cast<HMENU>(INT_PTR(id)), inst, nullptr);
        SendMessageW(c, WM_SETFONT, WPARAM(g_font), TRUE);
        return c;
    };

    // перемикачі мови й теми (у шапці праворуч)
    HWND lang = make(WC_COMBOBOXW, CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, kApp.hasThemeChoice ? 266 : 366, 30,
                     kApp.hasThemeChoice ? 112 : 130, 200, IDC_LANG);
    HWND theme = make(WC_COMBOBOXW, CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 384, 30, 112, 200, IDC_THEME);
    if (!kApp.hasThemeChoice)
        ShowWindow(theme, SW_HIDE);
    SendMessageW(lang, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Українська"));
    SendMessageW(lang, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"English"));
    SendMessageW(lang, CB_SETCURSEL, g_uk ? 0 : 1, 0);

    make(L"BUTTON", BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP, 24, 100, 472, 24, IDC_MODE_INSTALL);
    make(L"BUTTON", BS_AUTORADIOBUTTON, 24, 126, 472, 24, IDC_MODE_PORTABLE);

    make(L"STATIC", SS_LEFT, 24, 164, 300, 20, IDC_PATH_LABEL);
    make(L"EDIT", WS_TABSTOP | ES_AUTOHSCROLL, 24, 186, 372, 26, IDC_PATH, WS_EX_CLIENTEDGE);
    make(L"BUTTON", BS_PUSHBUTTON | WS_TABSTOP, 404, 185, 92, 28, IDC_BROWSE);

    make(L"BUTTON", BS_AUTOCHECKBOX | WS_TABSTOP, 24, 226, 472, 22, IDC_DESKTOP);
    HWND autostart = make(L"BUTTON", BS_AUTOCHECKBOX | WS_TABSTOP, 24, 250, 472, 22, IDC_AUTOSTART);
    make(L"BUTTON", BS_AUTOCHECKBOX | WS_TABSTOP, 24, kApp.hasAutostart ? 274 : 250, 472, 22, IDC_LAUNCH);
    if (!kApp.hasAutostart)
        ShowWindow(autostart, SW_HIDE);

    make(L"STATIC", SS_LEFT, 24, 306, 472, 40, IDC_STATUS);
    HWND prog = make(PROGRESS_CLASSW, 0, 24, 350, 472, 14, IDC_PROGRESS);
    SendMessageW(prog, PBM_SETRANGE32, 0, 100);
    ShowWindow(prog, SW_HIDE);

    make(L"BUTTON", BS_DEFPUSHBUTTON | WS_TABSTOP, 296, 378, 110, 32, IDC_INSTALL);
    make(L"BUTTON", BS_PUSHBUTTON | WS_TABSTOP, 412, 378, 84, 32, IDC_CANCEL);

    SendMessageW(item(IDC_MODE_INSTALL), BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(item(IDC_DESKTOP), BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(item(IDC_AUTOSTART), BM_SETCHECK, kApp.hasAutostart ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(item(IDC_LAUNCH), BM_SETCHECK, BST_CHECKED, 0);
    applyTexts();
    updateModeUi();

    if (g_update) {
        // та сама папка й режим; ярлик на робочому столі та автозапуск — як були
        SendMessageW(item(IDC_MODE_INSTALL), BM_SETCHECK, g_updatePortable ? BST_UNCHECKED : BST_CHECKED, 0);
        SendMessageW(item(IDC_MODE_PORTABLE), BM_SETCHECK, g_updatePortable ? BST_CHECKED : BST_UNCHECKED, 0);
        SetWindowTextW(item(IDC_PATH), g_updateDir.c_str());
        g_pathEdited = true;
        const bool hadDesktop = GetFileAttributesW(desktopShortcut().c_str()) != INVALID_FILE_ATTRIBUTES;
        SendMessageW(item(IDC_DESKTOP), BM_SETCHECK, hadDesktop && !g_updatePortable ? BST_CHECKED : BST_UNCHECKED, 0);
        if (kApp.hasAutostart)
            SendMessageW(item(IDC_AUTOSTART), BM_SETCHECK,
                         regValueExists(HKEY_CURRENT_USER, kRunKey, kApp.regKey) ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(item(IDC_LAUNCH), BM_SETCHECK, BST_CHECKED, 0);
        PostMessageW(hwnd, WM_COMMAND, MAKEWPARAM(IDC_INSTALL, BN_CLICKED), 0); // одразу починаємо
    }
}

void paintHeader(HWND hwnd, HDC dc)
{
    RECT rc;
    GetClientRect(hwnd, &rc);

    // кольорова смужка-градієнт угорі (кольори лого)
    const LONG mid = rc.right / 2, h = dpiScale(6);
    TRIVERTEX vx[4] = {
        {0, 0, kApp.grad[0][0], kApp.grad[0][1], kApp.grad[0][2], 0},
        {mid, h, kApp.grad[1][0], kApp.grad[1][1], kApp.grad[1][2], 0},
        {mid, 0, kApp.grad[1][0], kApp.grad[1][1], kApp.grad[1][2], 0},
        {rc.right, h, kApp.grad[2][0], kApp.grad[2][1], kApp.grad[2][2], 0},
    };
    GRADIENT_RECT gr[2] = {{0, 1}, {2, 3}};
    GradientFill(dc, vx, 4, gr, 2, GRADIENT_FILL_RECT_H);

    if (g_logo)
        DrawIconEx(dc, dpiScale(24), dpiScale(26), g_logo, dpiScale(56), dpiScale(56), 0, nullptr, DI_NORMAL);

    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ old = SelectObject(dc, g_titleFont);
    SetTextColor(dc, RGB(0x20, 0x20, 0x28));
    RECT tr = {dpiScale(94), dpiScale(24), dpiScale(kApp.hasThemeChoice ? 260 : 360), dpiScale(60)};
    DrawTextW(dc, kApp.name, -1, &tr, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(dc, g_subFont);
    SetTextColor(dc, RGB(0x6b, 0x6b, 0x78));
    RECT sr = {dpiScale(96), dpiScale(60), rc.right - dpiScale(20), dpiScale(84)};
    const std::wstring sub = std::wstring(T(kApp.taglineUk, kApp.taglineEn)) + T(L" Версія ", L" Version ") + kApp.version;
    DrawTextW(dc, sub.c_str(), -1, &sr, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(dc, old);
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        g_hwnd = hwnd;
        createControls(hwnd, reinterpret_cast<CREATESTRUCTW *>(lParam)->hInstance);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        paintHeader(hwnd, dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, TRANSPARENT);
        if (GetDlgCtrlID(reinterpret_cast<HWND>(lParam)) == IDC_STATUS)
            SetTextColor(dc, RGB(0x6b, 0x6b, 0x78));
        return reinterpret_cast<LRESULT>(g_white);
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_LANG:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                g_uk = SendMessageW(item(IDC_LANG), CB_GETCURSEL, 0, 0) == 0;
                applyTexts();
            }
            break;
        case IDC_MODE_INSTALL:
        case IDC_MODE_PORTABLE:
            if (HIWORD(wParam) == BN_CLICKED)
                updateModeUi();
            break;
        case IDC_PATH:
            if (HIWORD(wParam) == EN_CHANGE && GetFocus() == item(IDC_PATH))
                g_pathEdited = true;
            break;
        case IDC_BROWSE:
            browseFolder();
            break;
        case IDC_INSTALL:
            if (g_done)
                DestroyWindow(hwnd);
            else if (!g_busy)
                startInstall();
            break;
        case IDC_CANCEL:
        case IDCANCEL:
            if (!g_busy)
                DestroyWindow(hwnd);
            break;
        }
        return 0;
    case WM_APP_PROGRESS:
        SendMessageW(item(IDC_PROGRESS), PBM_SETPOS, wParam, 0);
        return 0;
    case WM_APP_DONE: {
        auto *job = reinterpret_cast<InstallJob *>(lParam);
        g_busy = false;
        if (!job->error.empty()) {
            const std::wstring title = std::wstring(kApp.name) + T(L" — помилка", L" — error");
            MessageBoxW(hwnd, job->error.c_str(), title.c_str(), MB_ICONERROR);
            for (int id : {IDC_LANG, IDC_THEME, IDC_MODE_INSTALL, IDC_MODE_PORTABLE, IDC_PATH, IDC_BROWSE, IDC_DESKTOP,
                           IDC_AUTOSTART, IDC_INSTALL})
                EnableWindow(item(id), TRUE);
            ShowWindow(item(IDC_PROGRESS), SW_HIDE);
            updateStatusText();
        } else {
            g_done = true;
            g_doneDir = job->dir;
            SendMessageW(item(IDC_PROGRESS), PBM_SETPOS, 100, 0);
            applyTexts();
            EnableWindow(item(IDC_INSTALL), TRUE);
            ShowWindow(item(IDC_CANCEL), SW_HIDE);
            if (isChecked(IDC_LAUNCH)) {
                const std::wstring exe = joinPath(job->dir, kApp.exe);
                ShellExecuteW(nullptr, L"open", exe.c_str(), nullptr, job->dir.c_str(), SW_SHOWNORMAL);
            }
            if (g_update)
                SetTimer(hwnd, 1, 1500, nullptr); // оновлено — вікно закриється саме
        }
        delete job;
        return 0;
    }
    case WM_TIMER:
        if (wParam == 1) {
            KillTimer(hwnd, 1);
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_CLOSE:
        if (!g_busy)
            DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    g_uk = windowsIsUkrainian(); // початковий вибір; змінюється у вікні
    parseCommandLine();
    if (g_update) {
        std::string iniText;
        if (readFileUtf8(settingsIni(g_updateDir, g_updatePortable), iniText)) {
            if (iniText.find("language=uk") != std::string::npos)
                g_uk = true;
            else if (iniText.find("language=en") != std::string::npos)
                g_uk = false;
        }
    }
    g_font = makeFont(10, FW_NORMAL);
    g_titleFont = makeFont(22, FW_SEMIBOLD);
    g_subFont = makeFont(10, FW_NORMAL);
    g_white = CreateSolidBrush(RGB(255, 255, 255));
    g_logo = static_cast<HICON>(LoadImageW(inst, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, dpiScale(56),
                                           dpiScale(56), LR_DEFAULTCOLOR));

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(IDI_APPICON));
    wc.hIconSm = static_cast<HICON>(LoadImageW(inst, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, 16, 16, 0));
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW
    wc.hbrBackground = g_white;
    wc.lpszClassName = L"LumioSetupWindow";
    RegisterClassExW(&wc);

    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT r = {0, 0, dpiScale(520), dpiScale(430)};
    AdjustWindowRect(&r, style, FALSE);
    const int w = r.right - r.left, h = r.bottom - r.top;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2, y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, kApp.name, style, x, y, w, h, nullptr, nullptr, inst, nullptr);
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    CoUninitialize();
    return 0;
}

#endif
