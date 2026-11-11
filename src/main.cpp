// Lumio Photo — швидкий локальний організатор і редактор фото (відкрита заміна Picasa).
// Без телеметрії й аналітики. Мережа — лише перевірка оновлень за кнопкою. Ліцензія MIT.

#include "icons.h"
#include "library.h"
#include "mainwindow.h"
#include "theme.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

// Окремий «канал» на кожну копію програми (встановлена / портативна)
QString instanceName()
{
    const QByteArray key = QDir::cleanPath(AppPaths::dataDir()).toLower().toUtf8();
    return QStringLiteral("LumioPhoto-") + QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha1).toHex().left(16));
}

// Якщо програма вже відкрита — передаємо їй файл і виходимо
bool sendToRunningInstance(const QString &path)
{
    QLocalSocket socket;
    socket.connectToServer(instanceName());
    if (!socket.waitForConnected(500))
        return false;
#ifdef Q_OS_WIN
    AllowSetForegroundWindow(ASFW_ANY); // щоб уже відкрите вікно могло вийти на передній план
#endif
    socket.write(path.toUtf8() + '\n');
    socket.flush();
    socket.waitForBytesWritten(1000);
    socket.disconnectFromServer();
    return true;
}

QString fileArgument()
{
    const QStringList args = QCoreApplication::arguments();
    for (int i = 1; i < args.size(); ++i)
        if (!args[i].startsWith(QLatin1String("--")))
            return QFileInfo(args[i]).absoluteFilePath();
    return QString();
}

} // namespace

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Lumio Photo"));
    QApplication::setOrganizationName(QStringLiteral("LumioPhoto"));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));

    const QString openPath = fileArgument();
    if (!qEnvironmentVariableIsSet("LP_MULTI_INSTANCE") && sendToRunningInstance(openPath))
        return 0;

    QApplication::setWindowIcon(Icons::appIcon());
    Theme::apply(app);

    MainWindow w;

    QLocalServer server;
    QLocalServer::removeServer(instanceName()); // прибрати «завислий» канал після аварійного виходу
    server.setSocketOptions(QLocalServer::UserAccessOption);
    if (server.listen(instanceName())) {
        QObject::connect(&server, &QLocalServer::newConnection, &w, [&server, &w]() {
            while (QLocalSocket *s = server.nextPendingConnection()) {
                QObject::connect(s, &QLocalSocket::disconnected, s, &QObject::deleteLater);
                QObject::connect(s, &QLocalSocket::readyRead, &w, [s, &w]() {
                    while (s->canReadLine()) {
                        const QString path = QString::fromUtf8(s->readLine()).trimmed();
                        if (path.isEmpty())
                            w.bringToFront();
                        else
                            w.openFile(path);
                    }
                });
            }
        });
    }

    w.show();
    QTimer::singleShot(0, &w, [&w, openPath]() { w.startup(openPath); });
    return QApplication::exec();
}
