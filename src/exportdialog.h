#pragma once

#include "photo.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QSlider;

// Експорт вибраних фото з урахуванням правок: розмір, формат, якість.
// Метадані в експортовані файли не записуються.
class ExportDialog : public QDialog
{
    Q_OBJECT
public:
    ExportDialog(const PhotoList &photos, QWidget *parent = nullptr);
    void accept() override;

private:
    PhotoList m_photos;
    QLineEdit *m_dir;
    QComboBox *m_size;
    QComboBox *m_format;
    QSlider *m_quality;
    QLabel *m_qualityLabel;
};
