#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class QProgressBar;
class QPushButton;
class QTextBrowser;

// Оновлення одним натисканням: перевіряє останній реліз на GitHub,
// показує, що нового, завантажує інсталятор і запускає його в режимі оновлення.
//
// Мережа використовується ЛИШЕ тут і лише тоді, коли користувач сам відкрив це вікно.
// Жодних даних про користувача не надсилається — тільки звичайний запит до GitHub.
class UpdateDialog : public QDialog
{
    Q_OBJECT
public:
    struct Config {
        QString appName;      // "Lumio Photo"
        QString repo;         // "coldtrace00/LumioPhoto"
        QString assetName;    // "LumioPhoto-Setup.exe"
        QString version;      // поточна версія, "1.0.0"
    };

    UpdateDialog(const Config &config, QWidget *parent = nullptr);

    // "1.2.10" > "1.2.9"; префікс "v" ігнорується
    static int compareVersions(const QString &a, const QString &b);

private:
    void check();
    void onReleaseReply(QNetworkReply *reply);
    void download();
    void onDownloadFinished(QNetworkReply *reply);
    void showError(const QString &text);

    Config m_config;
    QNetworkAccessManager *m_net;
    QLabel *m_icon;
    QLabel *m_title;
    QLabel *m_text;
    QTextBrowser *m_notes;
    QProgressBar *m_progress;
    QPushButton *m_actionButton;
    QPushButton *m_closeButton;

    QString m_latestVersion;
    QString m_assetUrl;
    QString m_assetSha256; // з GitHub (поле digest), якщо є
    QString m_releaseUrl;
    qint64 m_assetSize = 0;
    bool m_downloading = false;
};
