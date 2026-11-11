#include "exportdialog.h"
#include "i18n.h"

#include "edits.h"
#include "library.h"
#include "util.h"

#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImageWriter>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

ExportDialog::ExportDialog(const PhotoList &photos, QWidget *parent)
    : QDialog(parent)
    , m_photos(photos)
{
    setWindowTitle(tr2("Експорт фото (%1)", "Export photos (%1)").arg(photos.size()));
    setMinimumWidth(520);
    QSettings &s = AppPaths::settings();

    auto *form = new QFormLayout;
    m_dir = new QLineEdit(this);
    m_dir->setText(s.value(QStringLiteral("export/dir"),
                           QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + QStringLiteral("/Lumio Photo Export"))
                       .toString());
    auto *browse = new QPushButton(tr2("Огляд…", "Browse…"), this);
    connect(browse, &QPushButton::clicked, this, [this]() {
        const QString d = QFileDialog::getExistingDirectory(this, tr2("Куди експортувати", "Export to"), m_dir->text());
        if (!d.isEmpty())
            m_dir->setText(QDir::toNativeSeparators(d));
    });
    auto *dirRow = new QHBoxLayout;
    dirRow->addWidget(m_dir, 1);
    dirRow->addWidget(browse);
    form->addRow(tr2("Папка:", "Folder:"), dirRow);

    m_size = new QComboBox(this);
    m_size->addItem(tr2("Оригінальний розмір", "Original size"), 0);
    for (int px : {3840, 2560, 1920, 1600, 1280, 1024, 800, 640})
        m_size->addItem(tr2("Не більше %1 пікс.", "At most %1 px").arg(px), px);
    m_size->setCurrentIndex(qMax(0, m_size->findData(s.value(QStringLiteral("export/size"), 0).toInt())));
    form->addRow(tr2("Розмір:", "Size:"), m_size);

    m_format = new QComboBox(this);
    m_format->addItem(QStringLiteral("JPEG"), QStringLiteral("jpg"));
    m_format->addItem(tr2("PNG (без втрат)", "PNG (lossless)"), QStringLiteral("png"));
    m_format->addItem(tr2("Як в оригіналі", "Same as original"), QString());
    m_format->setCurrentIndex(qMax(0, m_format->findData(s.value(QStringLiteral("export/format"), QStringLiteral("jpg")).toString())));
    form->addRow(tr2("Формат:", "Format:"), m_format);

    m_quality = new QSlider(Qt::Horizontal, this);
    m_quality->setRange(50, 100);
    m_quality->setValue(s.value(QStringLiteral("export/quality"), 90).toInt());
    m_qualityLabel = new QLabel(this);
    m_qualityLabel->setMinimumWidth(40);
    auto updateQ = [this]() { m_qualityLabel->setText(QStringLiteral("%1%").arg(m_quality->value())); };
    connect(m_quality, &QSlider::valueChanged, this, updateQ);
    updateQ();
    auto *qRow = new QHBoxLayout;
    qRow->addWidget(m_quality, 1);
    qRow->addWidget(m_qualityLabel);
    form->addRow(tr2("Якість JPEG:", "JPEG quality:"), qRow);

    auto *note = new QLabel(tr2("Правки буде застосовано. Метадані (EXIF, GPS, дата, камера) "
                                           "в експортовані файли не записуються.", "Edits will be applied. Metadata (EXIF, GPS, date, camera) is not written to exported files."),
                            this);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: #8a909a;"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr2("Експортувати", "Export"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr2("Скасувати", "Cancel"));
    connect(buttons, &QDialogButtonBox::accepted, this, &ExportDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &ExportDialog::reject);

    auto *l = new QVBoxLayout(this);
    l->addLayout(form);
    l->addWidget(note);
    l->addWidget(buttons);
}

void ExportDialog::accept()
{
    const QString dir = QDir::fromNativeSeparators(m_dir->text().trimmed());
    if (dir.isEmpty() || !QDir().mkpath(dir)) {
        QMessageBox::warning(this, tr2("Експорт", "Export"), tr2("Не вдалося створити папку.", "Could not create the folder."));
        return;
    }
    QSettings &s = AppPaths::settings();
    s.setValue(QStringLiteral("export/dir"), m_dir->text());
    s.setValue(QStringLiteral("export/size"), m_size->currentData());
    s.setValue(QStringLiteral("export/format"), m_format->currentData());
    s.setValue(QStringLiteral("export/quality"), m_quality->value());

    const int maxSide = m_size->currentData().toInt();
    const QString fmt = m_format->currentData().toString();

    QProgressDialog progress(tr2("Експорт…", "Exporting…"), tr2("Зупинити", "Stop"), 0, m_photos.size(), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(300);
    int done = 0;
    QStringList failed;
    for (const Photo &p : std::as_const(m_photos)) {
        if (progress.wasCanceled())
            break;
        progress.setValue(done);
        progress.setLabelText(p.name);
        QApplication::processEvents();

        QImage img = Edits::load(p.path, 0);
        if (!p.edits.isEmpty())
            img = Edits::apply(img, EditState::fromJson(p.edits));
        if (maxSide > 0 && (img.width() > maxSide || img.height() > maxSide))
            img = img.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        const QFileInfo fi(p.path);
        QString suffix = fmt.isEmpty() ? fi.suffix().toLower() : fmt;
        if (suffix == QLatin1String("jpeg") || suffix == QLatin1String("jfif"))
            suffix = QStringLiteral("jpg");
        const QByteArray wfmt = suffix == QLatin1String("jpg") ? QByteArray("jpeg")
                                : suffix == QLatin1String("tif") ? QByteArray("tiff") : suffix.toLatin1();
        static const QStringList writable = {QStringLiteral("jpg"), QStringLiteral("png"), QStringLiteral("bmp"),
                                             QStringLiteral("webp"), QStringLiteral("tif"), QStringLiteral("tiff")};
        if (!writable.contains(suffix) || !QImageWriter::supportedImageFormats().contains(wfmt))
            suffix = QStringLiteral("png"); // gif, heic тощо — зберігаємо як PNG
        QString target = dir + QLatin1Char('/') + fi.completeBaseName() + QLatin1Char('.') + suffix;
        for (int i = 1; QFile::exists(target); ++i)
            target = dir + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral("_%1.").arg(i) + suffix;
        if (img.isNull() || !Util::saveImage(img, target, m_quality->value()))
            failed << p.name;
        ++done;
    }
    progress.setValue(m_photos.size());

    QString msg = tr2("Експортовано: %1", "Exported: %1").arg(done - failed.size());
    if (!failed.isEmpty())
        msg += tr2("\nНе вдалося: %1", "\nFailed: %1").arg(failed.join(QStringLiteral(", ")));
    QMessageBox box(QMessageBox::Information, tr2("Експорт", "Export"), msg, QMessageBox::Ok, this);
    QPushButton *open = box.addButton(tr2("Відкрити папку", "Open folder"), QMessageBox::ActionRole);
    box.exec();
    if (box.clickedButton() == open)
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    QDialog::accept();
}
