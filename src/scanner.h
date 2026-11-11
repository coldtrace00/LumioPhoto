#pragma once

#include <QStringList>
#include <QThread>

#include <atomic>

// Фоновий пошук фото в папках бібліотеки. Працює у власному потоці
// з окремим підключенням до бази, тож інтерфейс не «замерзає».
class Scanner : public QThread
{
    Q_OBJECT
public:
    explicit Scanner(QObject *parent = nullptr);
    ~Scanner() override;

    void start(const QStringList &roots);
    void cancel() { m_cancel = true; }

    static QStringList supportedSuffixes();

signals:
    void progress(int found, const QString &currentFolder);
    void finishedScan(int added, int updated, int removed);

protected:
    void run() override;

private:
    QStringList m_roots;
    std::atomic<bool> m_cancel{false};
};
