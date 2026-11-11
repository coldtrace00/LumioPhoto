#pragma once

#include <QImage>
#include <QRectF>
#include <QString>

// Неруйнівні правки (як у Picasa): зберігаються в базі як JSON,
// оригінальний файл не змінюється, доки користувач сам не натисне «Зберегти у файл».
struct EditState {
    int rotate = 0;          // чверті оберту за годинниковою стрілкою: 0..3
    bool flip = false;       // дзеркально по горизонталі
    double straighten = 0;   // вирівнювання горизонту, градуси (-20..20)
    QRectF crop;             // нормалізовані 0..1 координати після повороту; порожній — без обрізання

    bool autoColor = false;    // автоколір (вирівнювання кожного каналу) — «Мені пощастить»
    bool autoContrast = false; // автоконтраст за яскравістю
    int brightness = 0;        // -100..100
    int contrast = 0;          // -100..100
    int saturation = 0;        // -100..100
    int warmth = 0;            // -100..100 (холодніше / тепліше)
    int shadows = 0;           // 0..100 — висвітлити тіні
    int highlights = 0;        // 0..100 — приглушити світлі ділянки

    bool blackWhite = false;
    bool sepia = false;
    int sharpen = 0;   // 0..100
    int vignette = 0;  // 0..100

    bool isIdentity() const;
    bool hasGeometry() const { return rotate != 0 || flip || straighten != 0 || !crop.isEmpty(); }
    QString toJson() const;
    static EditState fromJson(const QString &json);
    bool operator==(const EditState &o) const { return toJson() == o.toJson(); }
    bool operator!=(const EditState &o) const { return !(*this == o); }
};

namespace Edits {

// Геометрія: поворот, віддзеркалення, вирівнювання (без обрізання crop)
QImage applyGeometry(const QImage &src, const EditState &e, bool includeCrop = true);
// Повернути / віддзеркалити ТЕ, що бачить користувач (з урахуванням обрізання)
void rotate(EditState &e, bool clockwise);
void mirror(EditState &e);
// Розмір результату для зображення розміром src (без декодування)
QSize editedSize(const QSize &src, const EditState &e);
// Повний конвеєр: геометрія + тон + колір + ефекти
QImage apply(const QImage &src, const EditState &e);
// Завантажити файл з урахуванням орієнтації EXIF (лише для показу) і зменшити до maxSide (0 — без зменшення)
QImage load(const QString &path, int maxSide = 0, QSize *originalSize = nullptr);

} // namespace Edits
