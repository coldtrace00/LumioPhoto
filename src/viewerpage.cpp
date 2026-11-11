#include "viewerpage.h"
#include "i18n.h"

#include "icons.h"
#include "imagecanvas.h"
#include "infopanel.h"
#include "library.h"
#include "thumbcache.h"
#include "util.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImageWriter>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QRunnable>
#include <QScreen>
#include <QScrollArea>
#include <QSlider>
#include <QStackedWidget>
#include <QTabWidget>
#include <QThreadPool>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

int previewMaxSide()
{
    int m = 1920;
    if (QScreen *s = QGuiApplication::primaryScreen())
        m = int(qMax(s->size().width(), s->size().height()) * s->devicePixelRatio());
    return qBound(1280, m, 3200);
}

QToolButton *toolButton(QWidget *parent, Icons::Id icon, const QString &tip, bool checkable = false)
{
    auto *b = new QToolButton(parent);
    b->setIcon(Icons::icon(icon));
    b->setIconSize(QSize(20, 20));
    b->setToolTip(tip);
    b->setAutoRaise(true);
    b->setCheckable(checkable);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}

QToolButton *bigButton(QWidget *parent, Icons::Id icon, const QString &text, bool checkable = false)
{
    auto *b = new QToolButton(parent);
    b->setIcon(Icons::icon(icon));
    b->setIconSize(QSize(22, 22));
    b->setText(text);
    b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    b->setCheckable(checkable);
    b->setMinimumSize(76, 58);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}

} // namespace

ViewerPage::ViewerPage(QWidget *parent)
    : QWidget(parent)
{
    m_previewTimer.setSingleShot(true);
    m_previewTimer.setInterval(25);
    connect(&m_previewTimer, &QTimer::timeout, this, &ViewerPage::renderPreview);
    m_commitTimer.setSingleShot(true);
    m_commitTimer.setInterval(450);
    connect(&m_commitTimer, &QTimer::timeout, this, &ViewerPage::commitEdits);

    m_canvas = new ImageCanvas(this);
    m_info = new InfoPanel(this);
    m_info->setFixedWidth(270);
    m_info->setVisible(false);

    auto *center = new QHBoxLayout;
    center->setContentsMargins(0, 0, 0, 0);
    center->setSpacing(0);
    center->addWidget(buildEditPanel());
    center->addWidget(m_canvas, 1);
    center->addWidget(m_info);

    auto *main = new QVBoxLayout(this);
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);
    main->addWidget(buildTopBar());
    main->addLayout(center, 1);

    connect(m_canvas, &ImageCanvas::prevRequested, this, [this]() { go(-1); });
    connect(m_canvas, &ImageCanvas::nextRequested, this, [this]() { go(1); });
    connect(m_canvas, &ImageCanvas::escapePressed, this, [this]() {
        if (m_mode == Mode::Crop)
            finishCrop(false);
        else if (m_mode == Mode::Straighten)
            finishStraighten(false);
        else
            emit closed();
    });
    connect(m_canvas, &ImageCanvas::zoomChanged, this, [this]() { QTimer::singleShot(120, this, &ViewerPage::maybeLoadFullRes); });

    connect(m_info, &InfoPanel::tagsEdited, this, [this](qint64 id, const QString &t) {
        Library::setTags(id, t);
        for (Photo &p : m_list)
            if (p.id == id) {
                p.tags = t;
                emit photoChanged(p);
            }
    });
    connect(m_info, &InfoPanel::captionEdited, this, [this](qint64 id, const QString &c) {
        Library::setCaption(id, c);
        for (Photo &p : m_list)
            if (p.id == id) {
                p.caption = c;
                emit photoChanged(p);
            }
    });

    auto addShortcut = [this](const QKeySequence &ks, auto fn) {
        auto *a = new QAction(this);
        a->setShortcut(ks);
        a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        connect(a, &QAction::triggered, this, fn);
        addAction(a);
    };
    addShortcut(QKeySequence(Qt::Key_S), [this]() { toggleStar(); });
    addShortcut(QKeySequence(Qt::Key_Delete), [this]() { deleteCurrent(); });
    addShortcut(QKeySequence::Undo, [this]() { if (!m_undo.isEmpty()) setState(m_undo.takeLast()); });
    addShortcut(QKeySequence(QStringLiteral("Ctrl+L")), [this]() {
        beginChange();
        Edits::rotate(m_state, false);
        changed();
    });
    addShortcut(QKeySequence(QStringLiteral("Ctrl+R")), [this]() {
        beginChange();
        Edits::rotate(m_state, true);
        changed();
    });
    addShortcut(QKeySequence(Qt::Key_I), [this]() { m_infoBtn->toggle(); });
}

// ---------------------------------------------------------------------------
// Інтерфейс

QWidget *ViewerPage::buildTopBar()
{
    auto *bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("topBar"));
    auto *l = new QHBoxLayout(bar);
    l->setContentsMargins(8, 4, 8, 4);

    auto *back = new QToolButton(bar);
    back->setIcon(Icons::icon(Icons::Back));
    back->setText(tr2("Бібліотека", "Library"));
    back->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    back->setAutoRaise(true);
    back->setToolTip(tr2("Назад до бібліотеки (Esc)", "Back to library (Esc)"));
    connect(back, &QToolButton::clicked, this, &ViewerPage::closed);
    l->addWidget(back);

    auto *prev = toolButton(bar, Icons::Prev, tr2("Попереднє (←)", "Previous (←)"));
    auto *next = toolButton(bar, Icons::Next, tr2("Наступне (→, пробіл)", "Next (→, Space)"));
    connect(prev, &QToolButton::clicked, this, [this]() { go(-1); });
    connect(next, &QToolButton::clicked, this, [this]() { go(1); });
    l->addWidget(prev);
    l->addWidget(next);

    m_title = new QLabel(bar);
    m_title->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l->addWidget(m_title, 1);

    m_starBtn = toolButton(bar, Icons::Star, tr2("Улюблене (S)", "Favorite (S)"), true);
    connect(m_starBtn, &QToolButton::clicked, this, &ViewerPage::toggleStar);
    l->addWidget(m_starBtn);

    auto *fit = toolButton(bar, Icons::ZoomFit, tr2("Вмістити у вікно (0)", "Fit to window (0)"));
    connect(fit, &QToolButton::clicked, m_canvas, &ImageCanvas::fitToWindow);
    auto *z100 = toolButton(bar, Icons::Zoom100, tr2("Реальний розмір 100% (1)", "Actual size 100% (1)"));
    connect(z100, &QToolButton::clicked, this, [this]() { m_canvas->zoomTo(1.0); });
    l->addWidget(fit);
    l->addWidget(z100);

    auto *show = toolButton(bar, Icons::Slideshow, tr2("Слайдшоу", "Slideshow"));
    connect(show, &QToolButton::clicked, this, [this]() { emit slideshowRequested(m_list, m_index); });
    auto *exp = toolButton(bar, Icons::Export, tr2("Експорт…", "Exporting…"));
    connect(exp, &QToolButton::clicked, this, [this]() {
        if (hasPhoto())
            emit exportRequested({current()});
    });
    auto *wall = toolButton(bar, Icons::Wallpaper, tr2("Встановити як шпалери", "Set as wallpaper"));
    connect(wall, &QToolButton::clicked, this, &ViewerPage::setAsWallpaper);
    auto *copy = toolButton(bar, Icons::Copy, tr2("Копіювати зображення", "Copy image"));
    connect(copy, &QToolButton::clicked, this, [this]() {
        if (!m_canvas->image().isNull())
            Util::copyImage(m_canvas->image());
    });
    auto *folder = toolButton(bar, Icons::Folder, tr2("Показати в папці", "Show in folder"));
    connect(folder, &QToolButton::clicked, this, [this]() {
        if (hasPhoto())
            Util::showInFolder(current().path);
    });
    m_infoBtn = toolButton(bar, Icons::Info, tr2("Інформація (I)", "Information (I)"), true);
    connect(m_infoBtn, &QToolButton::toggled, this, [this](bool on) {
        m_info->setVisible(on);
        if (on && hasPhoto())
            m_info->setPhoto(current());
    });
    auto *del = toolButton(bar, Icons::Trash, tr2("Видалити в кошик (Delete)", "Move to Recycle Bin (Delete)"));
    connect(del, &QToolButton::clicked, this, &ViewerPage::deleteCurrent);
    for (QWidget *w : {static_cast<QWidget *>(show), static_cast<QWidget *>(exp), static_cast<QWidget *>(wall),
                       static_cast<QWidget *>(copy), static_cast<QWidget *>(folder), static_cast<QWidget *>(m_infoBtn),
                       static_cast<QWidget *>(del)})
        l->addWidget(w);
    return bar;
}

QSlider *ViewerPage::addSlider(QWidget *parent, QVBoxLayout *l, const QString &title, int min, int max,
                               int EditState::*field)
{
    auto *row = new QHBoxLayout;
    auto *lbl = new QLabel(title, parent);
    auto *val = new QLabel(QStringLiteral("0"), parent);
    val->setMinimumWidth(32);
    val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    val->setStyleSheet(QStringLiteral("color: #9aa0aa;"));
    row->addWidget(lbl, 1);
    row->addWidget(val);
    l->addLayout(row);

    auto *s = new QSlider(Qt::Horizontal, parent);
    s->setRange(min, max);
    l->addWidget(s);
    l->addSpacing(4);

    connect(s, &QSlider::sliderPressed, this, [this]() { beginChange(); });
    connect(s, &QSlider::sliderReleased, this, [this]() { commitEdits(); });
    connect(s, &QSlider::valueChanged, this, [this, s, val, field](int v) {
        val->setText(QString::number(v));
        if (m_syncing)
            return;
        if (!s->isSliderDown())
            beginChange();
        m_state.*field = v;
        changed(false);
    });
    m_sliders.append({s, field});
    return s;
}

QWidget *ViewerPage::buildEditPanel()
{
    auto *panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("editPanel"));
    panel->setFixedWidth(290);
    auto *pl = new QVBoxLayout(panel);
    pl->setContentsMargins(8, 8, 8, 8);

    m_panelStack = new QStackedWidget(panel);
    pl->addWidget(m_panelStack, 1);

    // ===== 0: звичайні правки
    auto *tabs = new QTabWidget(m_panelStack);
    tabs->setDocumentMode(true);

    // --- Основне
    auto *basic = new QWidget(tabs);
    auto *bl = new QVBoxLayout(basic);
    auto *grid1 = new QHBoxLayout;
    auto *crop = bigButton(basic, Icons::Crop, tr2("Обрізати", "Crop"));
    auto *straight = bigButton(basic, Icons::Straighten, tr2("Вирівняти", "Straighten"));
    auto *lucky = bigButton(basic, Icons::Magic, tr2("Магія", "Magic"));
    lucky->setToolTip(tr2("«Мені пощастить»: одним кліком покращити колір і контраст", "“I\'m feeling lucky”: improve color and contrast in one click"));
    connect(crop, &QToolButton::clicked, this, &ViewerPage::startCrop);
    connect(straight, &QToolButton::clicked, this, &ViewerPage::startStraighten);
    connect(lucky, &QToolButton::clicked, this, [this]() {
        beginChange();
        const bool on = !(m_state.autoColor && m_state.autoContrast);
        m_state.autoColor = on;
        m_state.autoContrast = on;
        changed();
    });
    grid1->addWidget(crop);
    grid1->addWidget(straight);
    grid1->addWidget(lucky);
    bl->addLayout(grid1);

    auto *grid2 = new QHBoxLayout;
    m_autoContrastBtn = bigButton(basic, Icons::Magic, tr2("Контраст", "Contrast"), true);
    m_autoContrastBtn->setToolTip(tr2("Автоматичний контраст", "Automatic contrast"));
    m_autoColorBtn = bigButton(basic, Icons::Magic, tr2("Колір", "Color"), true);
    m_autoColorBtn->setToolTip(tr2("Автоматичний баланс кольорів", "Automatic color balance"));
    connect(m_autoContrastBtn, &QToolButton::clicked, this, [this](bool on) { beginChange(); m_state.autoContrast = on; changed(); });
    connect(m_autoColorBtn, &QToolButton::clicked, this, [this](bool on) { beginChange(); m_state.autoColor = on; changed(); });
    auto *flip = bigButton(basic, Icons::Prev, tr2("Дзеркало", "Mirror"));
    connect(flip, &QToolButton::clicked, this, [this]() {
        beginChange();
        Edits::mirror(m_state);
        changed();
    });
    grid2->addWidget(m_autoContrastBtn);
    grid2->addWidget(m_autoColorBtn);
    grid2->addWidget(flip);
    bl->addLayout(grid2);

    auto *grid3 = new QHBoxLayout;
    auto *rl = bigButton(basic, Icons::RotateLeft, tr2("Ліворуч", "Left"));
    auto *rr = bigButton(basic, Icons::RotateRight, tr2("Праворуч", "Right"));
    rl->setToolTip(tr2("Повернути ліворуч (Ctrl+L)", "Rotate left (Ctrl+L)"));
    rr->setToolTip(tr2("Повернути праворуч (Ctrl+R)", "Rotate right (Ctrl+R)"));
    connect(rl, &QToolButton::clicked, this, [this]() { beginChange(); Edits::rotate(m_state, false); changed(); });
    connect(rr, &QToolButton::clicked, this, [this]() { beginChange(); Edits::rotate(m_state, true); changed(); });
    grid3->addWidget(rl);
    grid3->addWidget(rr);
    grid3->addStretch();
    bl->addLayout(grid3);
    bl->addStretch();
    tabs->addTab(basic, tr2("Основне", "Basic"));

    // --- Тон
    auto *tone = new QWidget(tabs);
    auto *tl = new QVBoxLayout(tone);
    addSlider(tone, tl, tr2("Яскравість", "Brightness"), -100, 100, &EditState::brightness);
    addSlider(tone, tl, tr2("Контраст", "Contrast"), -100, 100, &EditState::contrast);
    addSlider(tone, tl, tr2("Тіні (висвітлити)", "Shadows (lighten)"), 0, 100, &EditState::shadows);
    addSlider(tone, tl, tr2("Світлі ділянки (приглушити)", "Highlights (tone down)"), 0, 100, &EditState::highlights);
    addSlider(tone, tl, tr2("Насиченість", "Saturation"), -100, 100, &EditState::saturation);
    addSlider(tone, tl, tr2("Теплота", "Warmth"), -100, 100, &EditState::warmth);
    tl->addStretch();
    tabs->addTab(tone, tr2("Тон", "Tone"));

    // --- Ефекти
    auto *fx = new QWidget(tabs);
    auto *fl = new QVBoxLayout(fx);
    auto *fxRow = new QHBoxLayout;
    m_bwBtn = bigButton(fx, Icons::Photos, tr2("Ч/Б", "B&W"), true);
    m_sepiaBtn = bigButton(fx, Icons::Photos, tr2("Сепія", "Sepia"), true);
    connect(m_bwBtn, &QToolButton::clicked, this, [this](bool on) {
        beginChange();
        m_state.blackWhite = on;
        if (on)
            m_state.sepia = false;
        changed();
    });
    connect(m_sepiaBtn, &QToolButton::clicked, this, [this](bool on) {
        beginChange();
        m_state.sepia = on;
        if (on)
            m_state.blackWhite = false;
        changed();
    });
    fxRow->addWidget(m_bwBtn);
    fxRow->addWidget(m_sepiaBtn);
    fxRow->addStretch();
    fl->addLayout(fxRow);
    addSlider(fx, fl, tr2("Різкість", "Sharpness"), 0, 100, &EditState::sharpen);
    addSlider(fx, fl, tr2("Віньєтка", "Vignette"), 0, 100, &EditState::vignette);
    fl->addStretch();
    tabs->addTab(fx, tr2("Ефекти", "Effects"));
    m_panelStack->addWidget(tabs);

    // ===== 1: обрізання
    auto *cropPanel = new QWidget(m_panelStack);
    auto *cl = new QVBoxLayout(cropPanel);
    auto *ct = new QLabel(tr2("<b>Обрізання</b><br>Потягніть рамку або її кути.", "<b>Crop</b><br>Drag the frame or its corners."), cropPanel);
    ct->setWordWrap(true);
    cl->addWidget(ct);
    cl->addWidget(new QLabel(tr2("Пропорції:", "Aspect ratio:"), cropPanel));
    m_aspect = new QComboBox(cropPanel);
    m_aspect->addItem(tr2("Вільно", "Free"), 0.0);
    m_aspect->addItem(tr2("Як в оригіналі", "Same as original"), -1.0);
    m_aspect->addItem(tr2("1:1 (квадрат)", "1:1 (square)"), 1.0);
    m_aspect->addItem(QStringLiteral("4:3"), 4.0 / 3);
    m_aspect->addItem(QStringLiteral("3:4"), 3.0 / 4);
    m_aspect->addItem(QStringLiteral("3:2"), 3.0 / 2);
    m_aspect->addItem(QStringLiteral("2:3"), 2.0 / 3);
    m_aspect->addItem(QStringLiteral("16:9"), 16.0 / 9);
    m_aspect->addItem(QStringLiteral("9:16"), 9.0 / 16);
    m_aspect->addItem(QStringLiteral("4:5 (Instagram)"), 4.0 / 5);
    connect(m_aspect, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        double a = m_aspect->currentData().toDouble();
        if (a < 0) {
            const QSize s = m_canvas->logicalSize();
            a = s.height() > 0 ? double(s.width()) / s.height() : 0;
        }
        m_canvas->setCropAspect(a);
    });
    cl->addWidget(m_aspect);
    auto *cropApply = new QPushButton(tr2("Застосувати", "Apply"), cropPanel);
    cropApply->setDefault(true);
    auto *cropReset = new QPushButton(tr2("Без обрізання", "No crop"), cropPanel);
    auto *cropCancel = new QPushButton(tr2("Скасувати", "Cancel"), cropPanel);
    connect(cropApply, &QPushButton::clicked, this, [this]() { finishCrop(true); });
    connect(cropCancel, &QPushButton::clicked, this, [this]() { finishCrop(false); });
    connect(cropReset, &QPushButton::clicked, this, [this]() {
        m_canvas->setCropMode(false);
        m_mode = Mode::View;
        m_panelStack->setCurrentIndex(0);
        beginChange();
        m_state.crop = QRectF();
        changed();
    });
    cl->addSpacing(8);
    cl->addWidget(cropApply);
    cl->addWidget(cropReset);
    cl->addWidget(cropCancel);
    cl->addStretch();
    m_panelStack->addWidget(cropPanel);

    // ===== 2: вирівнювання
    auto *stPanel = new QWidget(m_panelStack);
    auto *sl = new QVBoxLayout(stPanel);
    auto *stt = new QLabel(tr2("<b>Вирівнювання</b><br>Поверніть так, щоб горизонт збігся з сіткою.", "<b>Straighten</b><br>Rotate until the horizon lines up with the grid."), stPanel);
    stt->setWordWrap(true);
    sl->addWidget(stt);
    m_straightenLabel = new QLabel(QStringLiteral("0.0°"), stPanel);
    m_straightenLabel->setAlignment(Qt::AlignCenter);
    sl->addWidget(m_straightenLabel);
    m_straightenSlider = new QSlider(Qt::Horizontal, stPanel);
    m_straightenSlider->setRange(-200, 200); // десяті частки градуса
    connect(m_straightenSlider, &QSlider::valueChanged, this, [this](int v) {
        m_straightenLabel->setText(QStringLiteral("%1°").arg(v / 10.0, 0, 'f', 1));
        if (m_mode == Mode::Straighten && !m_syncing) {
            m_state.straighten = v / 10.0;
            schedulePreview();
        }
    });
    sl->addWidget(m_straightenSlider);
    auto *stApply = new QPushButton(tr2("Застосувати", "Apply"), stPanel);
    auto *stCancel = new QPushButton(tr2("Скасувати", "Cancel"), stPanel);
    connect(stApply, &QPushButton::clicked, this, [this]() { finishStraighten(true); });
    connect(stCancel, &QPushButton::clicked, this, [this]() { finishStraighten(false); });
    sl->addSpacing(8);
    sl->addWidget(stApply);
    sl->addWidget(stCancel);
    sl->addStretch();
    m_panelStack->addWidget(stPanel);

    // ===== нижні кнопки
    auto *row = new QHBoxLayout;
    m_undoBtn = new QPushButton(Icons::icon(Icons::Undo), tr2("Скасувати", "Undo"), panel);
    m_undoBtn->setToolTip(tr2("Скасувати останню правку (Ctrl+Z)", "Undo last edit (Ctrl+Z)"));
    connect(m_undoBtn, &QPushButton::clicked, this, [this]() {
        if (!m_undo.isEmpty())
            setState(m_undo.takeLast());
    });
    auto *reset = new QPushButton(Icons::icon(Icons::Reset), tr2("Скинути все", "Reset all"), panel);
    reset->setToolTip(tr2("Прибрати всі правки (оригінал і так не змінено)", "Remove all edits (the original is untouched anyway)"));
    connect(reset, &QPushButton::clicked, this, [this]() {
        if (m_state.isIdentity())
            return;
        beginChange();
        setState(EditState());
    });
    row->addWidget(m_undoBtn);
    row->addWidget(reset);
    pl->addLayout(row);

    auto *save = new QPushButton(Icons::icon(Icons::Save), tr2("Зберегти у файл…", "Save to file…"), panel);
    save->setToolTip(tr2("Застосувати правки до самого файлу (оригінал збережеться в резервній копії)", "Apply the edits to the file itself (the original is kept as a backup)"));
    connect(save, &QPushButton::clicked, this, &ViewerPage::saveToFile);
    pl->addWidget(save);
    m_restoreBtn = new QPushButton(Icons::icon(Icons::Reset), tr2("Відновити оригінал", "Restore original"), panel);
    connect(m_restoreBtn, &QPushButton::clicked, this, &ViewerPage::restoreOriginal);
    pl->addWidget(m_restoreBtn);

    auto *note = new QLabel(tr2("Правки неруйнівні: файл не змінюється, доки ви не натиснете «Зберегти у файл».", "Edits are non-destructive: the file is not changed until you click “Save to file”."), panel);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: #7c828c; font-size: 11px;"));
    pl->addWidget(note);
    return panel;
}

// ---------------------------------------------------------------------------
// Завантаження й показ

void ViewerPage::open(const PhotoList &list, int index)
{
    m_list = list;
    m_index = qBound(0, index, qMax(0, list.size() - 1));
    loadCurrent();
    m_canvas->setFocus();
}

void ViewerPage::updatePhoto(const Photo &p)
{
    for (Photo &x : m_list)
        if (x.id == p.id)
            x = p;
    if (hasPhoto() && current().id == p.id) {
        updateTitle();
        if (m_info->isVisible())
            m_info->setPhoto(current());
    }
}

void ViewerPage::go(int delta)
{
    if (m_list.isEmpty() || m_mode != Mode::View)
        return;
    commitEdits();
    const int n = m_list.size();
    m_index = ((m_index + delta) % n + n) % n;
    loadCurrent();
}

void ViewerPage::updateTitle()
{
    if (!hasPhoto())
        return;
    const Photo &p = current();
    m_title->setText(tr2("<b>%1</b>  <span style='color:#9aa0aa'>%2 з %3  •  %4</span>", "<b>%1</b>  <span style='color:#9aa0aa'>%2 of %3  •  %4</span>")
                         .arg(p.name.toHtmlEscaped())
                         .arg(m_index + 1)
                         .arg(m_list.size())
                         .arg(QDateTime::fromSecsSinceEpoch(p.taken).toString(QStringLiteral("dd.MM.yyyy HH:mm"))));
    m_starBtn->setChecked(p.starred);
    m_starBtn->setIcon(Icons::icon(p.starred ? Icons::StarFilled : Icons::Star,
                                   p.starred ? QColor(255, 200, 60) : QColor()));
    m_restoreBtn->setVisible(Util::hasBackup(p.path));
}

void ViewerPage::loadCurrent()
{
    if (!hasPhoto())
        return;
    ++m_gen;
    m_mode = Mode::View;
    m_canvas->setCropMode(false);
    m_canvas->setGrid(false);
    m_panelStack->setCurrentIndex(0);
    m_previewTimer.stop();

    const Photo &p = current();
    m_state = EditState::fromJson(p.edits);
    m_undo.clear();
    m_base = QImage();
    m_fullEdited = QImage();
    m_fullKey.clear();
    m_fullLoading = false;
    syncControls();
    updateTitle();
    if (m_info->isVisible() || m_infoBtn->isChecked())
        m_info->setPhoto(p);

    // миттєво показуємо мініатюру з кешу, поки вантажиться саме фото
    const QPixmap thumb = ThumbCache::instance()->get(p);
    const QSize logical = Edits::editedSize(QSize(p.width, p.height), m_state);
    if (!thumb.isNull() && thumb.width() > 1)
        m_canvas->setImage(thumb.toImage(), logical.isValid() ? logical : thumb.size());
    else
        m_canvas->setImage(QImage(), QSize());

    const int gen = m_gen;
    const QString path = p.path;
    const int maxSide = previewMaxSide();
    QPointer<ViewerPage> self(this);
    QThreadPool::globalInstance()->start(QRunnable::create([self, gen, path, maxSide]() {
        QSize orig;
        const QImage img = Edits::load(path, maxSide, &orig);
        QMetaObject::invokeMethod(qApp, [self, gen, img, orig]() {
            if (self)
                self->onBaseLoaded(gen, img, orig);
        }, Qt::QueuedConnection);
    }));
}

void ViewerPage::onBaseLoaded(int gen, const QImage &img, const QSize &orig)
{
    if (gen != m_gen)
        return;
    if (img.isNull()) {
        m_canvas->setImage(QImage(), QSize());
        m_canvas->setMessage(tr2("Не вдалося відкрити файл.\nМожливо, формат не підтримується або файл пошкоджено.", "Could not open the file.\nThe format may be unsupported or the file is damaged."));
        return;
    }
    m_base = img;
    m_origSize = orig.isValid() ? orig : img.size();
    renderPreview();
}

void ViewerPage::schedulePreview()
{
    m_previewTimer.start();
}

void ViewerPage::renderPreview()
{
    if (m_base.isNull())
        return;
    EditState s = m_state;
    if (m_mode == Mode::Crop)
        s.crop = QRectF();
    const QImage disp = Edits::apply(m_base, s);
    m_canvas->setImage(disp, Edits::editedSize(m_origSize, s), true);
    maybeLoadFullRes();
}

void ViewerPage::maybeLoadFullRes()
{
    if (m_mode != Mode::View || m_base.isNull() || !hasPhoto())
        return;
    const QImage &shown = m_canvas->image();
    const QSize logical = m_canvas->logicalSize();
    if (shown.isNull() || logical.isEmpty())
        return;
    const double have = double(shown.width()) / logical.width(); // скільки пікселів зображення на логічний піксель
    const double need = m_canvas->zoom() * m_canvas->devicePixelRatioF();
    if (need <= have * 1.05)
        return;
    const QString key = m_state.toJson();
    if (!m_fullEdited.isNull() && m_fullKey == key) {
        if (m_canvas->image().cacheKey() != m_fullEdited.cacheKey())
            m_canvas->setImage(m_fullEdited, logical, true);
        return;
    }
    if (m_fullLoading)
        return;
    m_fullLoading = true;
    const int gen = m_gen;
    const QString path = current().path;
    const EditState st = m_state;
    QPointer<ViewerPage> self(this);
    QThreadPool::globalInstance()->start(QRunnable::create([self, gen, path, st, key]() {
        const QImage img = Edits::apply(Edits::load(path, 0), st);
        QMetaObject::invokeMethod(qApp, [self, gen, key, img]() {
            if (self)
                self->onFullLoaded(gen, key, img);
        }, Qt::QueuedConnection);
    }));
}

void ViewerPage::onFullLoaded(int gen, const QString &stateJson, const QImage &img)
{
    m_fullLoading = false;
    if (gen != m_gen || img.isNull())
        return;
    m_fullEdited = img;
    m_fullKey = stateJson;
    if (stateJson == m_state.toJson() && m_mode == Mode::View)
        m_canvas->setImage(img, m_canvas->logicalSize(), true);
    else
        maybeLoadFullRes();
}

// ---------------------------------------------------------------------------
// Правки

void ViewerPage::beginChange()
{
    m_undo.append(m_state);
    if (m_undo.size() > 100)
        m_undo.removeFirst();
}

void ViewerPage::changed(bool commitNow)
{
    syncControls();
    schedulePreview();
    if (commitNow)
        commitEdits();
    else
        m_commitTimer.start();
}

void ViewerPage::setState(const EditState &s)
{
    m_state = s;
    changed(true);
}

void ViewerPage::commitEdits()
{
    m_commitTimer.stop();
    if (!hasPhoto())
        return;
    Photo &p = m_list[m_index];
    const QString json = m_state.toJson();
    if (p.edits == json)
        return;
    p.edits = json;
    Library::setEdits(p.id, json);
    ThumbCache::instance()->invalidate(p.path);
    emit photoChanged(p);
}

void ViewerPage::syncControls()
{
    m_syncing = true;
    for (const auto &pr : std::as_const(m_sliders))
        if (!pr.first->isSliderDown())
            pr.first->setValue(m_state.*(pr.second));
    m_autoContrastBtn->setChecked(m_state.autoContrast);
    m_autoColorBtn->setChecked(m_state.autoColor);
    m_bwBtn->setChecked(m_state.blackWhite);
    m_sepiaBtn->setChecked(m_state.sepia);
    m_undoBtn->setEnabled(!m_undo.isEmpty());
    m_syncing = false;
}

void ViewerPage::startCrop()
{
    if (m_base.isNull())
        return;
    commitEdits();
    m_beforeMode = m_state;
    m_mode = Mode::Crop;
    m_panelStack->setCurrentIndex(1);
    renderPreview(); // показати без обрізання
    m_canvas->setCropMode(true, m_state.crop.isEmpty() ? QRectF(0.05, 0.05, 0.9, 0.9) : m_state.crop);
    m_aspect->setCurrentIndex(0);
    m_canvas->setCropAspect(0);
    m_canvas->setFocus();
}

void ViewerPage::finishCrop(bool apply)
{
    const QRectF r = m_canvas->cropRect();
    m_canvas->setCropMode(false);
    m_mode = Mode::View;
    m_panelStack->setCurrentIndex(0);
    if (apply) {
        beginChange();
        const bool full = r.left() < 0.002 && r.top() < 0.002 && r.right() > 0.998 && r.bottom() > 0.998;
        m_state.crop = full ? QRectF() : r;
        changed(true);
    } else {
        renderPreview();
    }
    m_canvas->fitToWindow();
}

void ViewerPage::startStraighten()
{
    if (m_base.isNull())
        return;
    commitEdits();
    m_beforeMode = m_state;
    m_mode = Mode::Straighten;
    m_panelStack->setCurrentIndex(2);
    m_syncing = true;
    m_straightenSlider->setValue(qRound(m_state.straighten * 10));
    m_syncing = false;
    m_straightenLabel->setText(QStringLiteral("%1°").arg(m_state.straighten, 0, 'f', 1));
    m_canvas->setGrid(true);
    m_canvas->fitToWindow();
}

void ViewerPage::finishStraighten(bool apply)
{
    m_canvas->setGrid(false);
    m_mode = Mode::View;
    m_panelStack->setCurrentIndex(0);
    if (apply) {
        const double angle = m_state.straighten;
        m_state = m_beforeMode;
        if (angle != m_beforeMode.straighten) {
            beginChange();
            m_state.straighten = angle;
        }
        changed(true);
    } else {
        m_state = m_beforeMode;
        renderPreview();
    }
}

// ---------------------------------------------------------------------------
// Дії

void ViewerPage::toggleStar()
{
    if (!hasPhoto())
        return;
    Photo &p = m_list[m_index];
    p.starred = !p.starred;
    Library::setStarred({p.id}, p.starred);
    updateTitle();
    emit photoChanged(p);
}

void ViewerPage::deleteCurrent()
{
    if (!hasPhoto())
        return;
    const Photo p = current();
    if (QMessageBox::question(this, tr2("Видалення", "Delete"),
                              tr2("Перемістити «%1» у кошик?", "Move “%1” to the Recycle Bin?").arg(p.name), QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No)
        != QMessageBox::Yes)
        return;
    QString err;
    if (!Util::moveToTrash(p.path, &err)) {
        QMessageBox::warning(this, tr2("Видалення", "Delete"), err);
        return;
    }
    Library::remove(p.id);
    m_list.removeAt(m_index);
    emit photoRemoved(p.id);
    if (m_list.isEmpty()) {
        emit closed();
        return;
    }
    m_index = qMin(m_index, m_list.size() - 1);
    loadCurrent();
}

void ViewerPage::saveToFile()
{
    if (!hasPhoto())
        return;
    commitEdits();
    Photo &p = m_list[m_index];
    if (m_state.isIdentity()) {
        QMessageBox::information(this, tr2("Зберегти у файл", "Save to file"), tr2("Правок немає — зберігати нічого.", "There are no edits — nothing to save."));
        return;
    }
    const QString suffix = QFileInfo(p.path).suffix().toLower();
    const QByteArray fmt = (suffix == QLatin1String("jpg") || suffix == QLatin1String("jfif")) ? QByteArray("jpeg")
                                                                                                 : suffix.toLatin1();
    if (!QImageWriter::supportedImageFormats().contains(fmt)) {
        QMessageBox::warning(this, tr2("Зберегти у файл", "Save to file"),
                             tr2("Формат .%1 не можна перезаписати. Скористайтеся «Експорт…».", "The .%1 format cannot be overwritten. Use “Export…”.").arg(suffix));
        return;
    }
    if (QMessageBox::question(this, tr2("Зберегти у файл", "Save to file"),
                              tr2("Застосувати правки до файлу «%1»?\n\n"
                                             "Оригінал буде збережено в резервній копії — його можна відновити.\n"
                                             "Метадані (EXIF, GPS тощо) у новий файл не записуються.", "Apply the edits to “%1”?\n\nThe original will be kept as a backup and can be restored.\nMetadata (EXIF, GPS, etc.) is not written to the new file.")
                                  .arg(p.name),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
        != QMessageBox::Yes)
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QImage full = Edits::apply(Edits::load(p.path, 0), m_state);
    const QString backup = Util::backupPathFor(p.path);
    bool ok = !full.isNull();
    if (ok && !QFile::exists(backup))
        ok = QFile::copy(p.path, backup);
    QString err;
    if (ok)
        ok = Util::saveImage(full, p.path, 95, &err);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, tr2("Зберегти у файл", "Save to file"),
                             tr2("Не вдалося зберегти файл.\n%1", "Could not save the file.\n%1").arg(err));
        return;
    }

    const QFileInfo fi(p.path);
    p.edits.clear();
    p.size = fi.size();
    p.mtime = fi.lastModified().toSecsSinceEpoch();
    p.width = full.width();
    p.height = full.height();
    Library::setEdits(p.id, QString());
    Library::updateFileInfo(p.id, p.size, p.mtime, p.width, p.height);
    ThumbCache::instance()->invalidate(p.path);
    emit photoChanged(p);
    loadCurrent();
}

void ViewerPage::restoreOriginal()
{
    if (!hasPhoto())
        return;
    Photo &p = m_list[m_index];
    const QString backup = Util::backupPathFor(p.path);
    if (!QFile::exists(backup))
        return;
    if (QMessageBox::question(this, tr2("Відновити оригінал", "Restore original"),
                              tr2("Повернути файл «%1» до оригіналу? Збережені у файл правки буде втрачено.", "Restore “%1” to the original? Edits saved to the file will be lost.")
                                  .arg(p.name),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes)
        return;
    const QString tmp = p.path + QStringLiteral(".lumio-restore");
    QFile::remove(tmp);
    if (!QFile::copy(backup, tmp) || (QFile::exists(p.path) && !QFile::remove(p.path)) || !QFile::rename(tmp, p.path)) {
        QFile::remove(tmp);
        QMessageBox::warning(this, tr2("Відновити оригінал", "Restore original"), tr2("Не вдалося відновити файл.", "Could not restore the file."));
        return;
    }
    QFile::remove(backup);
    QSize orig;
    Edits::load(p.path, 64, &orig);
    const QFileInfo fi(p.path);
    p.size = fi.size();
    p.mtime = fi.lastModified().toSecsSinceEpoch();
    p.width = orig.width();
    p.height = orig.height();
    Library::updateFileInfo(p.id, p.size, p.mtime, p.width, p.height);
    ThumbCache::instance()->invalidate(p.path);
    emit photoChanged(p);
    loadCurrent();
}

void ViewerPage::setAsWallpaper()
{
    if (!hasPhoto())
        return;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QImage img = Edits::apply(Edits::load(current().path, 3840), m_state);
    QString err;
    const bool ok = Util::setWallpaper(img, &err);
    QApplication::restoreOverrideCursor();
    if (!ok)
        QMessageBox::warning(this, tr2("Шпалери", "Wallpaper"), err);
}

void ViewerPage::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape) {
        emit m_canvas->escapePressed();
        return;
    }
    QWidget::keyPressEvent(e);
}
