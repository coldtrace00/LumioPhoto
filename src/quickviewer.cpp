#include "quickviewer.h"

#include "i18n.h"
#include "icons.h"
#include "library.h"
#include "scanner.h"
#include "util.h"

#include <QApplication>
#include <QCursor>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QScreen>
#include <QThreadPool>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>

namespace {

const QColor kBackdrop(10, 10, 12, 228);

} // namespace

QuickViewer::QuickViewer(QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint)
    , m_pool(new QThreadPool(this))
{
    m_pool->setMaxThreadCount(2);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setWindowIcon(Icons::appIcon());

    const QColor white(240, 240, 244);
    auto mk = [this, white](QWidget *parent, Icons::Id icon, const QString &tip, auto slot) {
        auto *b = new QToolButton(parent);
        b->setIcon(Icons::icon(icon, white));
        b->setIconSize(QSize(22, 22));
        b->setToolTip(tip);
        b->setFocusPolicy(Qt::NoFocus);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QToolButton::clicked, this, slot);
        return b;
    };
    auto mkText = [this](QWidget *parent, const QString &text, const QString &tip, auto slot) {
        auto *b = new QToolButton(parent);
        b->setText(text);
        b->setToolTip(tip);
        b->setFocusPolicy(Qt::NoFocus);
        b->setCursor(Qt::PointingHandCursor);
        b->setMinimumSize(34, 34);
        connect(b, &QToolButton::clicked, this, slot);
        return b;
    };
    auto sep = [](QWidget *parent) {
        auto *f = new QWidget(parent);
        f->setFixedSize(1, 24);
        f->setStyleSheet(QStringLiteral("background: rgba(255,255,255,40);"));
        return f;
    };

    // нижня панель
    m_bar = new QWidget(this);
    m_bar->setAttribute(Qt::WA_StyledBackground);
    m_bar->setObjectName(QStringLiteral("viewerBar"));
    const QString css = QStringLiteral(
        "#viewerBar, #viewerTitle { background: rgba(28,28,32,235); border: 1px solid rgba(255,255,255,28); border-radius: 14px; color: #e8e8ee; }"
        "#viewerBar QToolButton { color: #f0f0f4; background: transparent; border: none; border-radius: 9px; padding: 6px; font-size: 18px; }"
        "#viewerBar QToolButton:hover { background: rgba(255,255,255,34); }"
        "#viewerBar QToolButton:pressed { background: rgba(255,255,255,60); }"
        "#viewerBar QToolButton:disabled { color: rgba(255,255,255,70); }"
        "#viewerBar QLabel { color: #e8e8ee; background: transparent; }"
        "#viewerBar #editButton { font-size: 13px; padding: 6px 12px; border: 1px solid rgba(255,255,255,50); }");
    setStyleSheet(css);

    auto *bl = new QHBoxLayout(m_bar);
    bl->setContentsMargins(10, 6, 10, 6);
    bl->setSpacing(4);
    m_prev = mk(m_bar, Icons::Prev, tr2("Попереднє (←)", "Previous (←)"), [this]() { go(-1); });
    m_next = mk(m_bar, Icons::Next, tr2("Наступне (→)", "Next (→)"), [this]() { go(1); });
    bl->addWidget(m_prev);
    bl->addWidget(m_next);
    bl->addSpacing(4);
    bl->addWidget(sep(m_bar));
    bl->addSpacing(4);
    bl->addWidget(mkText(m_bar, QStringLiteral("−"), tr2("Зменшити (−)", "Zoom out (−)"), [this]() { zoomTo(m_zoom / 1.25); }));
    m_zoomLabel = new QLabel(m_bar);
    m_zoomLabel->setMinimumWidth(48);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    bl->addWidget(m_zoomLabel);
    bl->addWidget(mkText(m_bar, QStringLiteral("+"), tr2("Збільшити (+)", "Zoom in (+)"), [this]() { zoomTo(m_zoom * 1.25); }));
    bl->addWidget(mk(m_bar, Icons::ZoomFit, tr2("Вмістити на екрані (0)", "Fit to screen (0)"), [this]() { fit(); update(); updateUi(); }));
    bl->addWidget(mk(m_bar, Icons::Zoom100, tr2("Реальний розмір (1)", "Actual size (1)"), [this]() { zoomTo(1.0); }));
    bl->addSpacing(4);
    bl->addWidget(sep(m_bar));
    bl->addSpacing(4);
    bl->addWidget(mk(m_bar, Icons::RotateLeft, tr2("Повернути ліворуч (Ctrl+L)", "Rotate left (Ctrl+L)"), [this]() { rotate(false); }));
    bl->addWidget(mk(m_bar, Icons::RotateRight, tr2("Повернути праворуч (Ctrl+R)", "Rotate right (Ctrl+R)"), [this]() { rotate(true); }));
    bl->addSpacing(4);
    bl->addWidget(sep(m_bar));
    bl->addSpacing(4);
    bl->addWidget(mk(m_bar, Icons::Trash, tr2("Видалити в кошик (Delete)", "Move to Recycle Bin (Delete)"), [this]() { deleteCurrent(); }));
    auto *editBtn = new QToolButton(m_bar);
    editBtn->setObjectName(QStringLiteral("editButton"));
    editBtn->setIcon(Icons::icon(Icons::Edit, white));
    editBtn->setText(tr2("Редагувати", "Edit"));
    editBtn->setToolTip(tr2("Відкрити в Lumio Photo для редагування (Enter)", "Open in Lumio Photo for editing (Enter)"));
    editBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    editBtn->setFocusPolicy(Qt::NoFocus);
    editBtn->setCursor(Qt::PointingHandCursor);
    connect(editBtn, &QToolButton::clicked, this, &QuickViewer::edit);
    bl->addSpacing(6);
    bl->addWidget(editBtn);

    // верхній рядок: назва, номер, розмір
    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("viewerTitle"));
    m_title->setAttribute(Qt::WA_StyledBackground);
    m_title->setContentsMargins(14, 6, 14, 6);
    m_title->setTextFormat(Qt::PlainText);

    auto *close = mk(this, Icons::Close, tr2("Закрити (Esc)", "Close (Esc)"), [this]() { this->close(); });
    close->setObjectName(QStringLiteral("closeButton"));
    close->setIconSize(QSize(24, 24));
    close->setStyleSheet(QStringLiteral("QToolButton { background: rgba(28,28,32,235); border-radius: 18px; padding: 6px; }"
                                        "QToolButton:hover { background: rgba(200,40,40,220); }"));
}

QuickViewer::~QuickViewer()
{
    ++m_gen;
    m_pool->clear();
    m_pool->waitForDone(2000);
}

int QuickViewer::maxSide() const
{
    QScreen *s = screen() ? screen() : QGuiApplication::primaryScreen();
    const QSize px = s->geometry().size() * s->devicePixelRatio();
    return qMax(px.width(), px.height());
}

QString QuickViewer::currentPath() const
{
    return m_index >= 0 && m_index < m_files.size() ? m_files.at(m_index) : QString();
}

EditState QuickViewer::editsFor(const QString &path) const
{
    return EditState::fromJson(m_edits.value(Library::pathKey(path)));
}

namespace {

struct LoadResult {
    QImage image;
    QSize logical;
};

// Завантаження (у фоновому потоці): зменшена до розміру екрана або повна копія з урахуванням правок
LoadResult loadImage(const QString &path, const QString &editsJson, int side)
{
    LoadResult r;
    QSize orig;
    QImage img = Edits::load(path, side, &orig);
    if (img.isNull())
        return r;
    const EditState e = EditState::fromJson(editsJson);
    if (!e.isIdentity()) {
        img = Edits::apply(img, e);
        r.logical = Edits::editedSize(orig, e);
    } else {
        r.logical = orig;
    }
    img.convertTo(img.hasAlphaChannel() ? QImage::Format_ARGB32_Premultiplied : QImage::Format_RGB32);
    r.image = img;
    return r;
}

} // namespace

// ---------------------------------------------------------------------------

void QuickViewer::openPath(const QString &rawPath)
{
    const QString path = QDir::cleanPath(QFileInfo(rawPath).absoluteFilePath());
    ++m_gen;
    m_cache.clear();
    m_pending.clear();
    m_startPath = path;
    m_files = {path};
    m_index = 0;
    m_scanned = false;
    m_editsLoaded = false;
    m_edits.clear();

    // перше фото — одразу, без черги: вікно з'являється вже з картинкою
    if (!screen()) {
        if (QScreen *s = QGuiApplication::screenAt(QCursor::pos()))
            setScreen(s);
    }
    const LoadResult r = loadImage(path, QString(), maxSide());
    if (r.image.isNull()) {
        m_pixmap = QPixmap();
        m_failed = true;
        m_logical = QSize();
    } else {
        Loaded l{r.image, r.logical, false};
        m_cache.insert(path, l);
        setLoaded(l, false);
    }
    updateUi();

    // решту — після того, як вікно намалювалося
    QTimer::singleShot(40, this, [this, gen = m_gen]() {
        if (gen != m_gen)
            return;
        scanFolder();
        loadEdits();
        prefetch();
        maybeLoadFull();
    });
}

void QuickViewer::showOnScreen()
{
    QScreen *s = QGuiApplication::screenAt(QCursor::pos());
    if (!s)
        s = QGuiApplication::primaryScreen();
    setGeometry(s->availableGeometry());
    if (isMinimized())
        showNormal();
    show();
    raise();
    activateWindow();
    setFocus();
}

void QuickViewer::scanFolder()
{
    const QFileInfo fi(m_startPath);
    QStringList filters;
    for (const QString &s : Scanner::supportedSuffixes())
        filters << QStringLiteral("*.") + s;
    QFileInfoList entries = QDir(fi.absolutePath()).entryInfoList(filters, QDir::Files | QDir::Readable);
    std::sort(entries.begin(), entries.end(),
              [](const QFileInfo &a, const QFileInfo &b) { return Util::naturalLess(a.fileName(), b.fileName()); });
    const QString cur = Library::pathKey(currentPath());
    QStringList files;
    files.reserve(entries.size());
    int index = -1;
    for (const QFileInfo &e : std::as_const(entries)) {
        const QString p = QDir::cleanPath(e.absoluteFilePath());
        if (index < 0 && Library::pathKey(p) == cur)
            index = files.size();
        files << p;
    }
    if (index < 0) { // наприклад, формат, якого немає у фільтрі
        files.prepend(currentPath());
        index = 0;
    }
    m_files = files;
    m_index = index;
    m_scanned = true;
    updateUi();
}

void QuickViewer::loadEdits()
{
    // Правки (поворот тощо), зроблені раніше в Lumio Photo, — з бази програми
    if (!Library::open())
        return;
    m_edits = Library::editsInFolder(QFileInfo(m_startPath).absolutePath());
    m_editsLoaded = true;
    if (m_edits.isEmpty())
        return;
    // фото з правками перевантажуємо
    ++m_gen;
    for (auto it = m_cache.begin(); it != m_cache.end();) {
        if (m_edits.contains(Library::pathKey(it.key())))
            it = m_cache.erase(it);
        else
            ++it;
    }
    m_pending.clear();
    if (!m_cache.contains(currentPath())) {
        m_pixmap = QPixmap();
        requestLoad(m_index, false);
        update();
    }
}

void QuickViewer::requestLoad(int index, bool full)
{
    if (index < 0 || index >= m_files.size())
        return;
    const QString path = m_files.at(index);
    const QString key = full ? path + QStringLiteral("#full") : path;
    if (m_pending.contains(key) || (!full && m_cache.contains(path)))
        return;
    m_pending.insert(key);
    const QString json = m_edits.value(Library::pathKey(path));
    const int side = full ? 0 : maxSide();
    QPointer<QuickViewer> self(this);
    const int gen = m_gen;
    m_pool->start([self, path, json, side, full, gen]() {
        const LoadResult r = loadImage(path, json, side);
        QMetaObject::invokeMethod(
            qApp,
            [self, gen, path, r, full]() {
                if (self)
                    self->onLoaded(gen, path, Loaded{r.image, r.logical, full}, full);
            },
            Qt::QueuedConnection);
    });
}

void QuickViewer::onLoaded(int gen, const QString &path, const Loaded &l, bool full)
{
    m_pending.remove(full ? path + QStringLiteral("#full") : path);
    if (gen != m_gen)
        return;
    const bool current = path == currentPath();
    if (l.image.isNull()) {
        if (current && !full) {
            m_failed = true;
            updateUi();
            update();
        }
        return;
    }
    if (!full)
        m_cache.insert(path, l);
    if (!current)
        return;
    if (full) {
        if (!m_full && l.image.width() > m_pixmap.width())
            setLoaded(l, true);
    } else if (m_pixmap.isNull()) {
        setLoaded(l, false);
        maybeLoadFull();
    }
}

void QuickViewer::setLoaded(const Loaded &l, bool keepView)
{
    m_pixmap = QPixmap::fromImage(l.image);
    m_logical = l.logical.isValid() ? l.logical : l.image.size();
    m_full = l.full || l.image.size() == m_logical;
    m_failed = false;
    if (!keepView)
        fit();
    else
        clampPan();
    updateUi();
    update();
}

void QuickViewer::prefetch()
{
    if (m_files.size() <= 1)
        return;
    // залишаємо в пам'яті лише поточне й сусідні фото
    QSet<QString> keep;
    for (int d = -2; d <= 2; ++d)
        keep.insert(m_files.at(((m_index + d) % m_files.size() + m_files.size()) % m_files.size()));
    for (auto it = m_cache.begin(); it != m_cache.end();) {
        if (!keep.contains(it.key()))
            it = m_cache.erase(it);
        else
            ++it;
    }
    for (int d : {1, -1, 2}) {
        const int i = ((m_index + d) % m_files.size() + m_files.size()) % m_files.size();
        requestLoad(i, false);
    }
}

void QuickViewer::maybeLoadFull()
{
    if (m_full || m_pixmap.isNull() || m_logical.isEmpty())
        return;
    // потрібна повна роздільність, лише коли збільшують більше, ніж дає зменшена копія
    if (m_zoom * m_logical.width() > m_pixmap.width() * 1.05)
        requestLoad(m_index, true);
}

void QuickViewer::go(int delta)
{
    if (m_files.size() <= 1)
        return;
    goTo(((m_index + delta) % m_files.size() + m_files.size()) % m_files.size());
}

void QuickViewer::goTo(int index)
{
    if (index < 0 || index >= m_files.size())
        return;
    m_index = index;
    showCurrent();
}

void QuickViewer::showCurrent()
{
    const QString path = currentPath();
    const auto it = m_cache.constFind(path);
    if (it != m_cache.constEnd()) {
        setLoaded(*it, false);
    } else {
        m_pixmap = QPixmap();
        m_failed = false;
        m_logical = QSize();
        requestLoad(m_index, false);
        updateUi();
        update();
    }
    prefetch();
    maybeLoadFull();
}

// ---------------------------------------------------------------------------
// Дії

void QuickViewer::rotate(bool clockwise)
{
    const QString path = currentPath();
    if (path.isEmpty() || m_pixmap.isNull())
        return;
    EditState e = editsFor(path);
    Edits::rotate(e, clockwise);
    const QString json = e.toJson();

    // Неруйнівно: поворот запам'ятовується в базі Lumio Photo, файл не змінюється
    bool saved = false;
    if (Library::open()) {
        const PhotoList pl = Library::ensurePhotos({path});
        if (!pl.isEmpty()) {
            Library::setEdits(pl.first().id, json);
            saved = true;
        }
    }
    if (!saved)
        qWarning("Lumio Photo: rotation was not saved (library unavailable)");
    if (json.isEmpty())
        m_edits.remove(Library::pathKey(path));
    else
        m_edits.insert(Library::pathKey(path), json);

    // миттєво: повертаємо те, що вже на екрані
    QTransform t;
    t.rotate(clockwise ? 90 : -90);
    QImage img = m_pixmap.toImage().transformed(t);
    QSize logical = m_logical;
    logical.transpose();
    ++m_gen; // старі завантаження цього фото вже неактуальні
    m_pending.clear();
    const Loaded l{img, logical, m_full};
    m_cache.insert(path, l);
    setLoaded(l, false);
    prefetch();
}

void QuickViewer::deleteCurrent()
{
    const QString path = currentPath();
    if (path.isEmpty())
        return;
    QMessageBox box(QMessageBox::Question, QStringLiteral("Lumio Photo"),
                    tr2("Перемістити «%1» у кошик?", "Move “%1” to the Recycle Bin?").arg(QFileInfo(path).fileName()),
                    QMessageBox::Yes | QMessageBox::No, this);
    box.setDefaultButton(QMessageBox::Yes);
    if (box.exec() != QMessageBox::Yes)
        return;
    QString err;
    if (!Util::moveToTrash(path, &err)) {
        QMessageBox::warning(this, QStringLiteral("Lumio Photo"), tr2("Не вдалося видалити файл:\n%1", "Could not delete the file:\n%1").arg(err));
        return;
    }
    m_cache.remove(path);
    m_files.removeAt(m_index);
    if (m_files.isEmpty()) {
        close();
        return;
    }
    if (m_index >= m_files.size())
        m_index = 0;
    showCurrent();
}

void QuickViewer::edit()
{
    const QString path = currentPath();
    if (path.isEmpty())
        return;
    emit editRequested(path);
    close();
}

// ---------------------------------------------------------------------------
// Масштаб і малювання

namespace {
QRectF viewArea(const QRect &r)
{
    return QRectF(r).adjusted(16, 58, -16, -84);
}
} // namespace

double QuickViewer::fitZoom() const
{
    if (m_logical.isEmpty())
        return 1.0;
    const QRectF a = viewArea(rect());
    const double dpr = devicePixelRatioF();
    const double z = qMin(a.width() * dpr / m_logical.width(), a.height() * dpr / m_logical.height());
    return qMin(z, 1.0); // маленькі фото не розтягуємо
}

void QuickViewer::fit()
{
    m_fit = true;
    m_zoom = fitZoom();
    m_pan = QPointF();
}

QRectF QuickViewer::imageRect() const
{
    if (m_logical.isEmpty())
        return QRectF();
    const double dpr = devicePixelRatioF();
    const QSizeF size(m_logical.width() * m_zoom / dpr, m_logical.height() * m_zoom / dpr);
    const QPointF c = viewArea(rect()).center() + m_pan;
    return QRectF(c.x() - size.width() / 2, c.y() - size.height() / 2, size.width(), size.height());
}

void QuickViewer::clampPan()
{
    const QRectF a = rect();
    const QRectF r = imageRect();
    const QPointF c = viewArea(rect()).center();
    auto clampAxis = [](double pan, double size, double areaSize, double areaStart, double centre) {
        if (size <= areaSize)
            return 0.0;
        const double minCenter = areaStart + areaSize - size / 2; // правий край не далі краю екрана
        const double maxCenter = areaStart + size / 2;
        return qBound(minCenter, centre + pan, maxCenter) - centre;
    };
    m_pan.setX(clampAxis(m_pan.x(), r.width(), a.width(), a.left(), c.x()));
    m_pan.setY(clampAxis(m_pan.y(), r.height(), a.height(), a.top(), c.y()));
}

void QuickViewer::zoomTo(double zoom, QPointF anchor)
{
    if (m_logical.isEmpty())
        return;
    zoom = qBound(qMin(fitZoom(), 0.05), zoom, 16.0);
    const QRectF old = imageRect();
    if (anchor.x() < 0)
        anchor = old.contains(rect().center()) ? QPointF(rect().center()) : old.center();
    const QPointF rel((anchor.x() - old.left()) / old.width(), (anchor.y() - old.top()) / old.height());
    m_zoom = zoom;
    m_fit = qFuzzyCompare(zoom, fitZoom());
    const double dpr = devicePixelRatioF();
    const QSizeF size(m_logical.width() * zoom / dpr, m_logical.height() * zoom / dpr);
    const QPointF topLeft(anchor.x() - rel.x() * size.width(), anchor.y() - rel.y() * size.height());
    m_pan = topLeft + QPointF(size.width() / 2, size.height() / 2) - viewArea(rect()).center();
    clampPan();
    maybeLoadFull();
    updateUi();
    update();
}

void QuickViewer::updateUi()
{
    const QString path = currentPath();
    QString t = QFileInfo(path).fileName();
    if (m_files.size() > 1)
        t += QStringLiteral("    %1 / %2").arg(m_index + 1).arg(m_files.size());
    if (m_logical.isValid())
        t += QStringLiteral("    %1 × %2").arg(m_logical.width()).arg(m_logical.height());
    m_title->setText(t);
    m_title->adjustSize();
    setWindowTitle(QFileInfo(path).fileName() + QStringLiteral(" — Lumio Photo"));
    m_zoomLabel->setText(m_logical.isValid() ? QStringLiteral("%1%").arg(qRound(m_zoom * 100)) : QString());
    m_prev->setEnabled(m_files.size() > 1);
    m_next->setEnabled(m_files.size() > 1);
    setCursor(!m_fit && !m_pixmap.isNull() ? Qt::OpenHandCursor : Qt::ArrowCursor);
    // розкладка
    m_bar->adjustSize();
    m_bar->move((width() - m_bar->width()) / 2, height() - m_bar->height() - 18);
    m_title->move((width() - m_title->width()) / 2, 14);
    if (auto *c = findChild<QToolButton *>(QStringLiteral("closeButton"))) {
        c->adjustSize();
        c->move(width() - c->width() - 14, 12);
        c->raise();
    }
}

void QuickViewer::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(rect(), kBackdrop);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    if (!m_pixmap.isNull()) {
        const QRectF r = imageRect();
        // при сильному збільшенні — чіткі пікселі, інакше — згладжування
        p.setRenderHint(QPainter::SmoothPixmapTransform, m_zoom < 3.0);
        p.drawPixmap(r, m_pixmap, QRectF(m_pixmap.rect()));
    } else if (m_failed) {
        p.setPen(QColor(230, 230, 235));
        p.drawText(rect(), Qt::AlignCenter, tr2("Не вдалося відкрити це зображення", "Could not open this image"));
    }
}

void QuickViewer::resizeEvent(QResizeEvent *)
{
    if (m_fit)
        fit();
    else
        clampPan();
    updateUi();
}

void QuickViewer::wheelEvent(QWheelEvent *e)
{
    const int dy = e->angleDelta().y();
    if (dy == 0)
        return;
    zoomTo(m_zoom * (dy > 0 ? 1.2 : 1 / 1.2), e->position());
}

void QuickViewer::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::BackButton) {
        go(-1);
        return;
    }
    if (e->button() == Qt::ForwardButton) {
        go(1);
        return;
    }
    if (e->button() != Qt::LeftButton)
        return;
    m_pressPos = m_lastMouse = e->position();
    m_pressOnImage = imageRect().contains(e->position());
    m_dragging = false;
}

void QuickViewer::mouseMoveEvent(QMouseEvent *e)
{
    if (!(e->buttons() & Qt::LeftButton))
        return;
    if (!m_dragging && (e->position() - m_pressPos).manhattanLength() > 4 && !m_fit)
        m_dragging = true;
    if (m_dragging) {
        setCursor(Qt::ClosedHandCursor);
        m_pan += e->position() - m_lastMouse;
        clampPan();
        update();
    }
    m_lastMouse = e->position();
}

void QuickViewer::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    if (m_dragging) {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);
        return;
    }
    // клік по тлі навколо фото закриває переглядач (як у Picasa)
    if (!m_pressOnImage && !imageRect().contains(e->position()))
        close();
}

void QuickViewer::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || !imageRect().contains(e->position()))
        return;
    if (m_fit && m_zoom < 0.999)
        zoomTo(1.0, e->position());
    else {
        fit();
        updateUi();
        update();
    }
}

void QuickViewer::keyPressEvent(QKeyEvent *e)
{
    const bool ctrl = e->modifiers() & Qt::ControlModifier;
    switch (e->key()) {
    case Qt::Key_Left:
    case Qt::Key_Up:
    case Qt::Key_PageUp:
    case Qt::Key_Backspace:
        go(-1);
        return;
    case Qt::Key_Right:
    case Qt::Key_Down:
    case Qt::Key_PageDown:
    case Qt::Key_Space:
        go(1);
        return;
    case Qt::Key_Home:
        goTo(0);
        return;
    case Qt::Key_End:
        goTo(m_files.size() - 1);
        return;
    case Qt::Key_Escape:
        if (!m_fit) {
            fit();
            updateUi();
            update();
        } else {
            close();
        }
        return;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        zoomTo(m_zoom * 1.25);
        return;
    case Qt::Key_Minus:
        zoomTo(m_zoom / 1.25);
        return;
    case Qt::Key_0:
    case Qt::Key_Asterisk:
        fit();
        updateUi();
        update();
        return;
    case Qt::Key_1:
        zoomTo(1.0);
        return;
    case Qt::Key_L:
        if (ctrl)
            rotate(false);
        return;
    case Qt::Key_R:
        if (ctrl)
            rotate(true);
        return;
    case Qt::Key_BracketLeft:
        rotate(false);
        return;
    case Qt::Key_BracketRight:
        rotate(true);
        return;
    case Qt::Key_Delete:
        deleteCurrent();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        edit();
        return;
    case Qt::Key_E:
        if (ctrl)
            edit();
        return;
    case Qt::Key_W:
    case Qt::Key_F4:
        if (ctrl)
            close();
        return;
    default:
        QWidget::keyPressEvent(e);
    }
}
