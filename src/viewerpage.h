#pragma once

#include "edits.h"
#include "photo.h"

#include <QImage>
#include <QTimer>
#include <QWidget>

class ImageCanvas;
class InfoPanel;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class QStackedWidget;
class QToolButton;

// Перегляд одного фото + неруйнівний редактор (як у Picasa:
// «Основне», «Тон», «Ефекти»). Правки зберігаються в базі одразу;
// сам файл змінюється лише за кнопкою «Зберегти у файл».
class ViewerPage : public QWidget
{
    Q_OBJECT
public:
    explicit ViewerPage(QWidget *parent = nullptr);

    void open(const PhotoList &list, int index);
    bool hasPhoto() const { return m_index >= 0 && m_index < m_list.size(); }
    const Photo &current() const { return m_list.at(m_index); }
    void updatePhoto(const Photo &p); // ззовні змінили (теги тощо)

signals:
    void closed();
    void photoChanged(const Photo &p);
    void photoRemoved(qint64 id);
    void slideshowRequested(const PhotoList &list, int index);
    void exportRequested(const PhotoList &list);

protected:
    void keyPressEvent(QKeyEvent *e) override;

private:
    QWidget *buildEditPanel();
    QWidget *buildTopBar();
    QSlider *addSlider(QWidget *parent, class QVBoxLayout *l, const QString &title, int min, int max, int EditState::*field);

    void go(int delta);
    void loadCurrent();
    void onBaseLoaded(int gen, const QImage &img, const QSize &orig);
    void schedulePreview();
    void renderPreview();
    void maybeLoadFullRes();
    void onFullLoaded(int gen, const QString &stateJson, const QImage &img);

    void beginChange();
    void changed(bool commitNow = true);
    void commitEdits();
    void setState(const EditState &s);
    void syncControls();
    void updateTitle();

    void startCrop();
    void finishCrop(bool apply);
    void startStraighten();
    void finishStraighten(bool apply);

    void toggleStar();
    void deleteCurrent();
    void saveToFile();
    void restoreOriginal();
    void setAsWallpaper();

    PhotoList m_list;
    int m_index = -1;
    int m_gen = 0;
    QImage m_base;      // джерело для попереднього перегляду (зменшене)
    QSize m_origSize;   // справжній розмір файлу (з урахуванням орієнтації)
    QImage m_fullEdited;
    QString m_fullKey;
    bool m_fullLoading = false;

    EditState m_state;
    QVector<EditState> m_undo;
    QTimer m_previewTimer;
    QTimer m_commitTimer;
    bool m_syncing = false;

    enum class Mode { View, Crop, Straighten } m_mode = Mode::View;
    EditState m_beforeMode;

    ImageCanvas *m_canvas;
    InfoPanel *m_info;
    QLabel *m_title;
    QToolButton *m_starBtn;
    QToolButton *m_infoBtn;
    QStackedWidget *m_panelStack; // 0 — звичайні правки, 1 — обрізання, 2 — вирівнювання
    QComboBox *m_aspect;
    QSlider *m_straightenSlider;
    QLabel *m_straightenLabel;
    QPushButton *m_undoBtn;
    QPushButton *m_restoreBtn;
    QToolButton *m_autoContrastBtn;
    QToolButton *m_autoColorBtn;
    QToolButton *m_bwBtn;
    QToolButton *m_sepiaBtn;
    QList<QPair<QSlider *, int EditState::*>> m_sliders;
};
