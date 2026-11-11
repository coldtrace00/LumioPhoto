#include "slideshow.h"
#include "i18n.h"

#include "edits.h"
#include "icons.h"
#include "library.h"

#include <QDateTime>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>
#include <QSettings>

Slideshow::Slideshow(const PhotoList &list, int start, QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint)
    , m_list(list)
    , m_index(qBound(0, start, qMax(0, list.size() - 1)))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);
    setCursor(Qt::BlankCursor);
    setWindowTitle(tr2("Lumio Photo — слайдшоу", "Lumio Photo — slideshow"));

    const int seconds = qBound(1, AppPaths::settings().value(QStringLiteral("slideshow/seconds"), 4).toInt(), 60);
    m_timer.setInterval(seconds * 1000);
    connect(&m_timer, &QTimer::timeout, this, &Slideshow::next);

    m_hudTimer.setSingleShot(true);
    m_hudTimer.setInterval(2200);
    connect(&m_hudTimer, &QTimer::timeout, this, [this]() {
        m_hud = false;
        setCursor(Qt::BlankCursor);
        update();
    });

    m_anim.setDuration(450);
    m_anim.setStartValue(0.0);
    m_anim.setEndValue(1.0);
    m_anim.setEasingCurve(QEasingCurve::InOutQuad);
    connect(&m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_fade = v.toDouble();
        update();
    });
}

void Slideshow::startShow()
{
    showFullScreen();
    activateWindow();
    showSlide(m_index, false);
    m_timer.start();
    flashHud();
}

QImage Slideshow::loadSlide(int index) const
{
    if (index < 0 || index >= m_list.size())
        return QImage();
    const Photo &p = m_list.at(index);
    int maxSide = 1920;
    if (QScreen *s = screen())
        maxSide = int(qMax(s->size().width(), s->size().height()) * s->devicePixelRatio());
    QImage img = Edits::load(p.path, maxSide);
    if (!p.edits.isEmpty())
        img = Edits::apply(img, EditState::fromJson(p.edits));
    return img;
}

void Slideshow::showSlide(int index, bool animate)
{
    if (m_list.isEmpty())
        return;
    m_index = index;
    m_previous = m_current;
    m_current = loadSlide(index);
    if (animate && !m_previous.isNull()) {
        m_fade = 0;
        m_anim.stop();
        m_anim.start();
    } else {
        m_fade = 1;
        update();
    }
}

void Slideshow::next()
{
    if (m_list.size() > 1)
        showSlide((m_index + 1) % m_list.size(), true);
}

void Slideshow::prev()
{
    if (m_list.size() > 1)
        showSlide((m_index - 1 + m_list.size()) % m_list.size(), true);
}

void Slideshow::togglePause()
{
    m_paused = !m_paused;
    if (m_paused)
        m_timer.stop();
    else
        m_timer.start();
    flashHud();
}

void Slideshow::flashHud()
{
    m_hud = true;
    setCursor(Qt::ArrowCursor);
    m_hudTimer.start();
    update();
}

void Slideshow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    auto drawFitted = [&](const QImage &img, double opacity) {
        if (img.isNull())
            return;
        const qreal dpr = devicePixelRatioF();
        QSizeF s(img.width() / dpr, img.height() / dpr);
        s.scale(size(), Qt::KeepAspectRatio);
        QRectF r(QPointF(0, 0), s);
        r.moveCenter(QRectF(rect()).center());
        p.setOpacity(opacity);
        p.drawImage(r, img);
    };
    if (m_fade < 1.0)
        drawFitted(m_previous, 1.0 - m_fade);
    drawFitted(m_current, m_fade);
    p.setOpacity(1.0);

    if (m_hud && !m_list.isEmpty()) {
        const Photo &ph = m_list.at(m_index);
        const QString text = tr2("%1   •   %2 з %3   •   %4", "%1   •   %2 of %3   •   %4")
                                 .arg(ph.caption.isEmpty() ? ph.name : ph.caption)
                                 .arg(m_index + 1)
                                 .arg(m_list.size())
                                 .arg(m_paused ? tr2("пауза (пробіл — продовжити)", "paused (Space to continue)")
                                               : tr2("пробіл — пауза, Esc — вихід", "Space — pause, Esc — exit"));
        QFont f = font();
        f.setPixelSize(15);
        p.setFont(f);
        const QFontMetrics fm(f);
        QRect tr(0, 0, fm.horizontalAdvance(text) + 32, fm.height() + 16);
        tr.moveCenter(QPoint(width() / 2, height() - 50));
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 160));
        p.drawRoundedRect(tr, 10, 10);
        p.setPen(QColor(240, 240, 240));
        p.drawText(tr, Qt::AlignCenter, text);
    }
}

void Slideshow::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
    case Qt::Key_Escape:
        close();
        break;
    case Qt::Key_Space:
    case Qt::Key_P:
        togglePause();
        break;
    case Qt::Key_Right:
    case Qt::Key_Down:
    case Qt::Key_PageDown:
        next();
        if (!m_paused)
            m_timer.start();
        flashHud();
        break;
    case Qt::Key_Left:
    case Qt::Key_Up:
    case Qt::Key_PageUp:
        prev();
        if (!m_paused)
            m_timer.start();
        flashHud();
        break;
    default:
        QWidget::keyPressEvent(e);
    }
}

void Slideshow::mouseMoveEvent(QMouseEvent *)
{
    flashHud();
}

void Slideshow::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::RightButton)
        close();
    else
        next();
}

void Slideshow::wheelEvent(QWheelEvent *e)
{
    if (e->angleDelta().y() < 0)
        next();
    else
        prev();
}
