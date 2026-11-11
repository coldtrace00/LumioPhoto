#pragma once

#include "photo.h"

#include <QHash>
#include <QMainWindow>

class InfoPanel;
class PhotoModel;
class QComboBox;
class QLabel;
class QLineEdit;
class QListView;
class QSlider;
class QStackedWidget;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class Scanner;
class ThumbDelegate;
class ViewerPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void startup(const QString &openPath = QString());
    void openFile(const QString &path); // фото з Провідника / «Відкрити за допомогою»
    void bringToFront();

protected:
    void closeEvent(QCloseEvent *e) override;

private:
    enum ViewType { ViewAll = 1, ViewStarred, ViewRecent, ViewEdited, ViewFolder, ViewAlbum, ViewAlbumsHeader, ViewFoldersHeader };

    void buildUi();
    QWidget *buildLibraryPage();
    void applyTheme();

    void reloadLibrary();
    void rebuildTree();
    void applyView();
    bool matchesView(const Photo &p, const QSet<qint64> &albumIds) const;

    QVector<Photo> selectedPhotos() const;
    QVector<qint64> selectedIds() const;
    void storePhoto(const Photo &p);       // оновити в пам'яті + в моделі
    void removePhotos(const QSet<qint64> &ids);
    void updateSelectionInfo();

    void addFolder();
    void rescan();
    void openViewer(int row);
    void closeViewer();
    void startSlideshow(const PhotoList &list, int index);
    void exportPhotos(const PhotoList &list);

    void toggleStarSelected();
    void rotateSelected(bool clockwise);
    void deleteSelected();
    void copySelected();
    void renameSelected();
    void printSelected();
    void wallpaperSelected();
    void addSelectedToAlbum(qint64 albumId);
    void newAlbumFromSelection();
    void removeSelectedFromAlbum();

    void showGridMenu(const QPoint &pos);
    void showTreeMenu(const QPoint &pos);
    void showAbout();
    void checkForUpdates();

    PhotoList m_photos;
    QHash<qint64, int> m_indexById;

    QStackedWidget *m_stack;
    QTreeWidget *m_tree;
    QListView *m_grid;
    PhotoModel *m_model;
    ThumbDelegate *m_delegate;
    ViewerPage *m_viewer;
    InfoPanel *m_info;
    QLineEdit *m_search;
    QComboBox *m_sort;
    QSlider *m_sizeSlider;
    QLabel *m_selectionLabel;
    QLabel *m_status;
    QLabel *m_emptyHint;
    QToolButton *m_infoToggle;
    Scanner *m_scanner;

    int m_viewType = ViewAll;
    QString m_viewValue;
};
