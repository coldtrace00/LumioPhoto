#include "updater.h"

#include "i18n.h"
#include "icons.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

// Адреса GitHub API (перевизначається лише в тестовій збірці)
#ifndef UPDATER_API_BASE
#define UPDATER_API_BASE "https://api.github.com"
#endif

namespace {

QNetworkRequest githubRequest(const QUrl &url)
{
    QNetworkRequest req(url);
    // GitHub вимагає User-Agent; більше нічого про користувача не передаємо
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Lumio-Updater"));
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(30000);
    return req;
}

bool isPortable()
{
    return QFile::exists(QCoreApplication::applicationDirPath() + QStringLiteral("/portable.txt"));
}

} // namespace

int UpdateDialog::compareVersions(const QString &a, const QString &b)
{
    auto parts = [](QString v) {
        v = v.trimmed();
        if (v.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
            v.remove(0, 1);
        const int dash = v.indexOf(QLatin1Char('-'));
        if (dash >= 0)
            v.truncate(dash);
        QList<int> out;
        for (const QString &p : v.split(QLatin1Char('.')))
            out << p.toInt();
        while (out.size() < 3)
            out << 0;
        return out;
    };
    const QList<int> pa = parts(a), pb = parts(b);
    for (int i = 0; i < qMax(pa.size(), pb.size()); ++i) {
        const int x = i < pa.size() ? pa[i] : 0, y = i < pb.size() ? pb[i] : 0;
        if (x != y)
            return x < y ? -1 : 1;
    }
    return 0;
}

UpdateDialog::UpdateDialog(const Config &config, QWidget *parent)
    : QDialog(parent)
    , m_config(config)
    , m_net(new QNetworkAccessManager(this))
{
    setWindowTitle(tr2("Оновлення — %1", "Updates — %1").arg(config.appName));
    setMinimumWidth(520);

    m_icon = new QLabel(this);
    m_icon->setPixmap(Icons::appPixmap(56));
    m_title = new QLabel(this);
    m_title->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: 600;"));
    m_text = new QLabel(this);
    m_text->setWordWrap(true);
    m_text->setTextFormat(Qt::RichText);
    m_text->setOpenExternalLinks(true);

    auto *head = new QHBoxLayout;
    head->addWidget(m_icon, 0, Qt::AlignTop);
    auto *ht = new QVBoxLayout;
    ht->addWidget(m_title);
    ht->addWidget(m_text);
    head->addLayout(ht, 1);

    m_notes = new QTextBrowser(this);
    m_notes->setOpenExternalLinks(true);
    m_notes->setMinimumHeight(180);
    m_notes->hide();

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0);

    m_actionButton = new QPushButton(this);
    m_actionButton->hide();
    m_closeButton = new QPushButton(tr2("Закрити", "Close"), this);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_actionButton, &QPushButton::clicked, this, &UpdateDialog::download);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(m_actionButton);
    buttons->addWidget(m_closeButton);

    auto *l = new QVBoxLayout(this);
    l->addLayout(head);
    l->addWidget(m_notes, 1);
    l->addWidget(m_progress);
    l->addLayout(buttons);

    check();
}

void UpdateDialog::showError(const QString &text)
{
    m_progress->hide();
    m_title->setText(m_downloading ? tr2("Не вдалося оновити", "Update failed")
                                   : tr2("Не вдалося перевірити оновлення", "Could not check for updates"));
    m_text->setText(text.toHtmlEscaped());
    m_actionButton->hide();
}

void UpdateDialog::check()
{
    m_title->setText(tr2("Перевіряю оновлення…", "Checking for updates…"));
    m_text->setText(tr2("Поточна версія: %1", "Current version: %1").arg(m_config.version));
    m_progress->setRange(0, 0);
    m_progress->show();
    QNetworkReply *reply = m_net->get(
        githubRequest(QUrl(QStringLiteral(UPDATER_API_BASE "/repos/%1/releases/latest").arg(m_config.repo))));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onReleaseReply(reply); });
}

void UpdateDialog::onReleaseReply(QNetworkReply *reply)
{
    reply->deleteLater();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 404) {
        m_progress->hide();
        m_title->setText(tr2("У вас остання версія", "You have the latest version"));
        m_text->setText(tr2("Версія %1. Опублікованих оновлень поки немає.", "Version %1. No updates have been published yet.")
                            .arg(m_config.version));
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        showError(tr2("Перевірте підключення до інтернету.\n\n%1", "Check your internet connection.\n\n%1")
                      .arg(reply->errorString()));
        return;
    }
    const QJsonObject rel = QJsonDocument::fromJson(reply->readAll()).object();
    m_latestVersion = rel.value(QStringLiteral("tag_name")).toString();
    m_releaseUrl = rel.value(QStringLiteral("html_url")).toString();
    if (m_latestVersion.isEmpty()) {
        showError(tr2("GitHub повернув незрозумілу відповідь.", "GitHub returned an unexpected response."));
        return;
    }

    m_progress->hide();
    if (compareVersions(m_latestVersion, m_config.version) <= 0) {
        m_title->setText(tr2("У вас остання версія", "You have the latest version"));
        m_text->setText(tr2("Встановлено версію %1 — оновлювати нічого.", "Version %1 is installed — nothing to update.")
                            .arg(m_config.version));
        return;
    }

    // шукаємо інсталятор серед файлів релізу
    for (const QJsonValue &v : rel.value(QStringLiteral("assets")).toArray()) {
        const QJsonObject a = v.toObject();
        if (a.value(QStringLiteral("name")).toString().compare(m_config.assetName, Qt::CaseInsensitive) == 0) {
            m_assetUrl = a.value(QStringLiteral("browser_download_url")).toString();
            m_assetSize = a.value(QStringLiteral("size")).toVariant().toLongLong();
            const QString digest = a.value(QStringLiteral("digest")).toString();
            if (digest.startsWith(QLatin1String("sha256:")))
                m_assetSha256 = digest.mid(7).toLower();
        }
    }

    const QString ver = m_latestVersion.startsWith(QLatin1Char('v')) ? m_latestVersion.mid(1) : m_latestVersion;
    m_title->setText(tr2("Доступна нова версія %1", "New version %1 is available").arg(ver));
    QString text = tr2("У вас версія %1.", "You have version %1.").arg(m_config.version);
    if (!m_releaseUrl.isEmpty())
        text += QStringLiteral(" <a href=\"%1\">%2</a>").arg(m_releaseUrl.toHtmlEscaped(),
                                                             tr2("Сторінка релізу", "Release page"));
    m_text->setText(text);

    const QString notes = rel.value(QStringLiteral("body")).toString().trimmed();
    if (!notes.isEmpty()) {
        m_notes->setMarkdown(notes);
        m_notes->show();
    }

#if defined(Q_OS_WIN) || defined(UPDATER_TEST)
    if (m_assetUrl.isEmpty()) {
        m_text->setText(text + QStringLiteral("<br>")
                        + tr2("У релізі немає файлу %1 — завантажте оновлення зі сторінки релізу.",
                              "The release has no %1 file — download the update from the release page.")
                              .arg(m_config.assetName.toHtmlEscaped()));
        return;
    }
    m_actionButton->setText(tr2("Оновити зараз", "Update now"));
    m_actionButton->setIcon(Icons::icon(Icons::Refresh));
    m_actionButton->setDefault(true);
    m_actionButton->show();
#else
    m_text->setText(text + QStringLiteral("<br>")
                    + tr2("Автоматичне оновлення доступне лише у Windows.", "Automatic updates are available only on Windows."));
#endif
}

void UpdateDialog::download()
{
    m_downloading = true;
    m_actionButton->setEnabled(false);
    m_title->setText(tr2("Завантажую оновлення…", "Downloading the update…"));
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->show();
    QNetworkReply *reply = m_net->get(githubRequest(QUrl(m_assetUrl)));
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64 total) {
        if (total <= 0)
            total = m_assetSize;
        if (total > 0)
            m_progress->setValue(int(got * 100 / total));
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onDownloadFinished(reply); });
}

void UpdateDialog::onDownloadFinished(QNetworkReply *reply)
{
    reply->deleteLater();
    m_actionButton->setEnabled(true);
    if (reply->error() != QNetworkReply::NoError) {
        showError(tr2("Не вдалося завантажити оновлення.\n\n%1", "Could not download the update.\n\n%1")
                      .arg(reply->errorString()));
        return;
    }
    const QByteArray data = reply->readAll();
    if (data.size() < 64 * 1024 || !data.startsWith("MZ")) {
        showError(tr2("Завантажений файл не схожий на інсталятор.", "The downloaded file does not look like an installer."));
        return;
    }
    if (!m_assetSha256.isEmpty()) {
        const QString got = QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
        if (got != m_assetSha256) {
            showError(tr2("Контрольна сума не збігається — файл пошкоджено або підмінено. Оновлення скасовано.",
                          "Checksum mismatch — the file is damaged or was tampered with. Update cancelled."));
            return;
        }
    }

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/LumioUpdate");
    QDir().mkpath(dir);
    const QString path = dir + QLatin1Char('/') + m_config.assetName;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size()) {
        showError(tr2("Не вдалося зберегти інсталятор.", "Could not save the installer."));
        return;
    }
    f.close();

    // інсталятор у режимі оновлення: та сама папка, ті самі налаштування, потім запуск програми
    QStringList args = {QStringLiteral("--update"), QStringLiteral("--dir"),
                        QDir::toNativeSeparators(QCoreApplication::applicationDirPath())};
    if (isPortable())
        args << QStringLiteral("--portable");
    if (!QProcess::startDetached(path, args)) {
        showError(tr2("Не вдалося запустити інсталятор.", "Could not start the installer."));
        return;
    }
    QCoreApplication::quit();
}
