#include "infopanel.h"
#include "i18n.h"

#include "exif.h"
#include "library.h"
#include "util.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

InfoPanel::InfoPanel(QWidget *parent)
    : QWidget(parent)
{
    setMinimumWidth(230);
    auto *l = new QVBoxLayout(this);
    l->setContentsMargins(12, 12, 12, 12);
    l->setSpacing(8);

    m_title = new QLabel(this);
    m_title->setWordWrap(true);
    m_title->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 600;"));
    m_title->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l->addWidget(m_title);

    m_details = new QLabel(this);
    m_details->setWordWrap(true);
    m_details->setTextFormat(Qt::RichText);
    m_details->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_details->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    l->addWidget(m_details);

    m_editors = new QWidget(this);
    auto *el = new QVBoxLayout(m_editors);
    el->setContentsMargins(0, 8, 0, 0);
    el->setSpacing(4);
    auto *tl = new QLabel(tr2("Теги (через кому):", "Tags (comma separated):"), m_editors);
    m_tags = new QLineEdit(m_editors);
    m_tags->setPlaceholderText(tr2("відпустка, море, сім'я", "vacation, sea, family"));
    auto *cl = new QLabel(tr2("Підпис:", "Caption:"), m_editors);
    m_caption = new QLineEdit(m_editors);
    m_caption->setPlaceholderText(tr2("Додати підпис…", "Add a caption…"));
    m_albums = new QLabel(m_editors);
    m_albums->setWordWrap(true);
    m_albums->setStyleSheet(QStringLiteral("color: #9aa0aa;"));
    auto *note = new QLabel(tr2("Теги й підписи зберігаються лише в Lumio Photo — файли не змінюються.", "Tags and captions are stored only in Lumio Photo — your files are not modified."), m_editors);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: #7c828c; font-size: 11px;"));
    el->addWidget(tl);
    el->addWidget(m_tags);
    el->addWidget(cl);
    el->addWidget(m_caption);
    el->addWidget(m_albums);
    el->addWidget(note);
    l->addWidget(m_editors);
    l->addStretch();

    connect(m_tags, &QLineEdit::editingFinished, this, [this]() {
        if (m_id && m_tags->isModified()) {
            QStringList parts;
            for (const QString &t : m_tags->text().split(QLatin1Char(','), Qt::SkipEmptyParts))
                if (!t.trimmed().isEmpty())
                    parts << t.trimmed();
            parts.removeDuplicates();
            m_tags->setText(parts.join(QStringLiteral(", ")));
            m_tags->setModified(false);
            emit tagsEdited(m_id, m_tags->text());
        }
    });
    connect(m_caption, &QLineEdit::editingFinished, this, [this]() {
        if (m_id && m_caption->isModified()) {
            m_caption->setModified(false);
            emit captionEdited(m_id, m_caption->text().trimmed());
        }
    });
    clear();
}

void InfoPanel::clear()
{
    m_id = 0;
    m_title->setText(tr2("Нічого не вибрано", "Nothing selected"));
    m_details->clear();
    m_editors->hide();
}

void InfoPanel::setMultiple(int count, qint64 totalBytes)
{
    m_id = 0;
    m_title->setText(tr2("Вибрано фото: %1", "Selected photos: %1").arg(count));
    m_details->setText(tr2("Загальний розмір: %1", "Total size: %1").arg(Util::humanSize(totalBytes)));
    m_editors->hide();
}

void InfoPanel::setPhoto(const Photo &p)
{
    m_id = p.id;
    m_title->setText(p.name);

    auto row = [](const QString &k, const QString &v) {
        return v.isEmpty() ? QString()
                           : QStringLiteral("<tr><td style='color:#9aa0aa;padding-right:8px'>%1</td><td>%2</td></tr>")
                                 .arg(k, v.toHtmlEscaped());
    };
    QString html = QStringLiteral("<table cellspacing='0' cellpadding='2'>");
    html += row(tr2("Дата", "Date"), QDateTime::fromSecsSinceEpoch(p.taken).toString(QStringLiteral("dd.MM.yyyy HH:mm")));
    if (p.width > 0)
        html += row(tr2("Розмір", "Size"), tr2("%1 × %2 (%3 Мп)", "%1 × %2 (%3 MP)").arg(p.width).arg(p.height)
                                                  .arg(p.width * double(p.height) / 1e6, 0, 'f', 1));
    html += row(tr2("Файл", "File"), Util::humanSize(p.size));
    html += row(tr2("Папка", "Folder"), QDir::toNativeSeparators(p.folder));

    const QString suffix = QFileInfo(p.path).suffix().toLower();
    if (suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg") || suffix == QLatin1String("jfif")) {
        const ExifData ex = Exif::read(p.path);
        if (ex.valid) {
            QString camera = ex.model;
            if (!ex.make.isEmpty() && !camera.startsWith(ex.make, Qt::CaseInsensitive))
                camera = ex.make + QLatin1Char(' ') + camera;
            html += row(tr2("Камера", "Camera"), camera.trimmed());
            html += row(tr2("Об'єктив", "Lens"), ex.lens);
            QStringList shot;
            if (ex.exposure > 0) shot << Exif::describeExposure(ex.exposure);
            if (ex.fnumber > 0) shot << QStringLiteral("f/%1").arg(ex.fnumber, 0, 'g', 3);
            if (ex.iso > 0) shot << QStringLiteral("ISO %1").arg(ex.iso);
            if (ex.focal > 0) shot << tr2("%1 мм", "%1 mm").arg(ex.focal, 0, 'g', 4);
            html += row(tr2("Зйомка", "Shot"), shot.join(QStringLiteral("  •  ")));
            if (ex.hasGps)
                html += row(QStringLiteral("GPS"), QStringLiteral("%1, %2").arg(ex.latitude, 0, 'f', 5).arg(ex.longitude, 0, 'f', 5));
        }
    }
    if (!p.edits.isEmpty())
        html += row(tr2("Правки", "Edits"), tr2("так (оригінал не змінено)", "yes (original untouched)"));
    html += QStringLiteral("</table>");
    m_details->setText(html);

    m_tags->setText(p.tags);
    m_tags->setModified(false);
    m_caption->setText(p.caption);
    m_caption->setModified(false);
    const QStringList albums = Library::albumsOf(p.id);
    m_albums->setText(albums.isEmpty() ? QString() : tr2("Альбоми: %1", "Albums: %1").arg(albums.join(QStringLiteral(", "))));
    m_editors->show();
}
