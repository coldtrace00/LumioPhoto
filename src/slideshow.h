#pragma once

#include "photo.h"

#include <QImage>
#include <QTimer>
#include <QVariantAnimation>
#include <QWidget>

// Повноекранне слайдшоу з плавними переходами.
// Пробіл — пауза, ← → — гортати, Esc — вихід.
class Slideshow : public QWidget
{
    Q_OBJECT
public:
    Slideshow(const PhotoList &list, int start, QWidget *parent = nullptr);
    void startShow();

protected:
    void paintEvent(QPaintEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

private:
    QImage loadSlide(int index) const;
    void showSlide(int index, bool animate);
    void next();
    void prev();
    void togglePause();
    void flashHud();

    PhotoList m_list;
    int m_index = 0;
    QImage m_current;
    QImage m_previous;
    double m_fade = 1.0;
    bool m_paused = false;
    bool m_hud = true;
    QTimer m_timer;
    QTimer m_hudTimer;
    QVariantAnimation m_anim;
};
