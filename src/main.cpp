// Lumio Photo — швидкий локальний організатор і редактор фото (відкрита заміна Picasa).
// Без телеметрії й аналітики. Мережа — лише перевірка оновлень за кнопкою. Ліцензія MIT.
//
//   LumioPhoto.exe              — бібліотека (головне вікно)
//   LumioPhoto.exe <фото>       — швидкий переглядач, як Picasa Photo Viewer (так відкриває Провідник)
//   LumioPhoto.exe --edit <фото> — одразу редактор у головному вікні

#include "icons.h"
#include "library.h"
#include "mainwindow.h"
#include "quickviewer.h"
#include "theme.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>
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

// Повідомлення між копіями: "view <шлях>", "edit <шлях>" або "library"
bool sendToRunningInstance(const QString &message)
{
    QLocalSocket socket;
    socket.connectToServer(instanceName());
    if (!socket.waitForConnected(400))
        return false;
#ifdef Q_OS_WIN
    AllowSetForegroundWindow(ASFW_ANY); // щоб уже відкрите вікно могло вийти на передній план
#endif
    socket.write(message.toUtf8() + '\n');
    socket.flush();
    socket.waitForBytesWritten(1000);
    socket.disconnectFromServer();
    return true;
}

// Хто що показує: бібліотека створюється лише тоді, коли вона справді потрібна
class Controller : public QObject
{
public:
    void showLibrary(const QString &editPath = QString())
    {
        if (!m_main) {
            m_main = new MainWindow;
            m_main->setAttribute(Qt::WA_DeleteOnClose);
            m_main->show();
            QPointer<MainWindow> w = m_main;
            QTimer::singleShot(0, m_main, [w, editPath]() {
                if (w)
                    w->startup(editPath);
            });
        } else {
            m_main->bringToFront();
            if (!editPath.isEmpty())
                m_main->openFile(editPath);
        }
    }

    void view(const QString &path)
    {
        if (!m_viewer) {
            m_viewer = new QuickViewer;
            m_viewer->setAttribute(Qt::WA_DeleteOnClose);
            connect(m_viewer, &QuickViewer::editRequested, this, [this](const QString &p) { showLibrary(p); });
        }
        m_viewer->openPath(path);
        m_viewer->showOnScreen();
    }

    void handle(const QString &message)
    {
        if (message.startsWith(QLatin1String("view ")))
            view(message.mid(5));
        else if (message.startsWith(QLatin1String("edit ")))
            showLibrary(message.mid(5));
        else
            showLibrary();
    }

private:
    QPointer<MainWindow> m_main;
    QPointer<QuickViewer> m_viewer;
};

QString buildMessage()
{
    const QStringList args = QCoreApplication::arguments();
    bool edit = false;
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == QLatin1String("--edit")) {
            edit = true;
            continue;
        }
        if (args[i].startsWith(QLatin1String("--")))
            continue;
        return (edit ? QStringLiteral("edit ") : QStringLiteral("view ")) + QFileInfo(args[i]).absoluteFilePath();
    }
    return QStringLiteral("library");
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

    const QString message = buildMessage();
    if (!qEnvironmentVariableIsSet("LP_MULTI_INSTANCE") && sendToRunningInstance(message))
        return 0;

    QApplication::setWindowIcon(Icons::appIcon());
    Theme::apply(app);

    Controller controller;

    QLocalServer server;
    QLocalServer::removeServer(instanceName()); // прибрати «завислий» канал після аварійного виходу
    server.setSocketOptions(QLocalServer::UserAccessOption);
    if (server.listen(instanceName())) {
        QObject::connect(&server, &QLocalServer::newConnection, &controller, [&server, &controller]() {
            while (QLocalSocket *s = server.nextPendingConnection()) {
                QObject::connect(s, &QLocalSocket::disconnected, s, &QObject::deleteLater);
                QObject::connect(s, &QLocalSocket::readyRead, &controller, [s, &controller]() {
                    while (s->canReadLine()) {
                        const QString msg = QString::fromUtf8(s->readLine()).trimmed();
                        if (!msg.isEmpty())
                            controller.handle(msg);
                    }
                });
            }
        });
    }

    controller.handle(message);
    return QApplication::exec();
}
