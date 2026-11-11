#include "mainwindow.h"
#include "i18n.h"

#include "edits.h"
#include "fileassoc.h"
#include "updater.h"
#include "exportdialog.h"
#include "icons.h"
#include "infopanel.h"
#include "library.h"
#include "photomodel.h"
#include "scanner.h"
#include "slideshow.h"
#include "theme.h"
#include "thumbcache.h"
#include "util.h"
#include "viewerpage.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QClipboard>
#include <QPainter>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <QtPrintSupport/qtprintsupportglobal.h>
#if QT_CONFIG(printdialog)
#include <QPrintDialog>
#include <QPrinter>
#endif

#include <algorithm>

namespace {
const int kTypeRole = Qt::UserRole;
const int kValueRole = Qt::UserRole + 1;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Lumio Photo"));
    setWindowIcon(Icons::appIcon());
    m_scanner = new Scanner(this);
    buildUi();

    connect(m_scanner, &Scanner::progress, this, [this](int found, const QString &folder) {
        m_status->setText(tr2("Пошук фото… знайдено %1  •  %2", "Looking for photos… found %1  •  %2").arg(found).arg(QDir::toNativeSeparators(folder)));
    });
    connect(m_scanner, &Scanner::finishedScan, this, [this](int added, int updated, int removed) {
        reloadLibrary();
        if (added || removed || updated)
            statusBar()->showMessage(tr2("Бібліотеку оновлено: нових %1, змінених %2, видалених %3", "Library updated: %1 new, %2 changed, %3 removed")
                                         .arg(added).arg(updated).arg(removed), 6000);
    });

    QSettings &s = AppPaths::settings();
    const QByteArray geo = s.value(QStringLiteral("window/geometry")).toByteArray();
    if (!geo.isEmpty())
        restoreGeometry(geo);
    else
        resize(1280, 820);
}

MainWindow::~MainWindow()
{
    m_scanner->cancel();
    m_scanner->wait();
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    AppPaths::settings().setValue(QStringLiteral("window/geometry"), saveGeometry());
    AppPaths::settings().setValue(QStringLiteral("window/thumbSize"), m_sizeSlider->value());
    AppPaths::settings().setValue(QStringLiteral("window/sort"), m_sort->currentIndex());
    AppPaths::settings().setValue(QStringLiteral("window/info"), m_infoToggle->isChecked());
    m_scanner->cancel();
    ThumbCache::instance()->cancelPending();
    e->accept();
}

// ---------------------------------------------------------------------------
// Інтерфейс

void MainWindow::buildUi()
{
    QToolBar *tb = addToolBar(tr2("Головна", "Main"));
    tb->setObjectName(QStringLiteral("mainToolbar"));
    tb->setMovable(false);
    tb->setIconSize(QSize(20, 20));
    tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    QAction *aAdd = tb->addAction(Icons::icon(Icons::FolderAdd), tr2("Додати папку", "Add folder"));
    aAdd->setToolTip(tr2("Додати папку з фото до бібліотеки", "Add a folder with photos to the library"));
    connect(aAdd, &QAction::triggered, this, &MainWindow::addFolder);
    QAction *aRefresh = tb->addAction(Icons::icon(Icons::Refresh), tr2("Оновити", "Refresh"));
    aRefresh->setShortcut(QKeySequence::Refresh);
    aRefresh->setToolTip(tr2("Знайти нові та змінені фото (F5)", "Find new and changed photos (F5)"));
    connect(aRefresh, &QAction::triggered, this, &MainWindow::rescan);
    tb->addSeparator();
    QAction *aShow = tb->addAction(Icons::icon(Icons::Slideshow), tr2("Слайдшоу", "Slideshow"));
    connect(aShow, &QAction::triggered, this, [this]() {
        const QModelIndex cur = m_grid->currentIndex();
        startSlideshow(m_model->photos(), cur.isValid() ? cur.row() : 0);
    });
    QAction *aExport = tb->addAction(Icons::icon(Icons::Export), tr2("Експорт", "Export"));
    aExport->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));
    connect(aExport, &QAction::triggered, this, [this]() { exportPhotos(selectedPhotos()); });

    auto *spacer = new QWidget(tb);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tb->addWidget(spacer);

    m_search = new QLineEdit(tb);
    m_search->setPlaceholderText(tr2("Пошук: назва, тег, підпис, папка…", "Search: name, tag, caption, folder…"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(Icons::icon(Icons::Search), QLineEdit::LeadingPosition);
    m_search->setFixedWidth(280);
    tb->addWidget(m_search);

    m_sort = new QComboBox(tb);
    m_sort->addItem(tr2("Нові спершу", "Newest first"));
    m_sort->addItem(tr2("Старі спершу", "Oldest first"));
    m_sort->addItem(tr2("За назвою", "By name"));
    m_sort->addItem(tr2("За розміром", "By size"));
    m_sort->setCurrentIndex(qBound(0, AppPaths::settings().value(QStringLiteral("window/sort"), 0).toInt(), 3));
    tb->addWidget(m_sort);

    m_infoToggle = new QToolButton(tb);
    m_infoToggle->setIcon(Icons::icon(Icons::Info));
    m_infoToggle->setCheckable(true);
    m_infoToggle->setToolTip(tr2("Інформація про фото", "Photo information"));
    tb->addWidget(m_infoToggle);
    auto *gear = new QToolButton(tb);
    gear->setIcon(Icons::icon(Icons::Gear));
    gear->setToolTip(tr2("Налаштування й оновлення", "Settings and updates"));
    gear->setPopupMode(QToolButton::InstantPopup);
    auto *gearMenu = new QMenu(gear);
    QMenu *langMenu = gearMenu->addMenu(QStringLiteral("Мова / Language"));
    QAction *aUk = langMenu->addAction(QStringLiteral("Українська"));
    QAction *aEn = langMenu->addAction(QStringLiteral("English"));
    aUk->setCheckable(true);
    aEn->setCheckable(true);
    aUk->setChecked(I18n::isUkrainian());
    aEn->setChecked(!I18n::isUkrainian());
    auto switchLang = [this](bool uk) {
        if (uk == I18n::isUkrainian())
            return;
        I18n::setUkrainian(uk);
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Lumio Photo"),
            uk ? QStringLiteral("Мову змінено на українську. Перезапустити програму зараз?\n\nLanguage changed to Ukrainian. Restart now?")
               : QStringLiteral("Language changed to English. Restart now?\n\nМову змінено на англійську. Перезапустити зараз?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (answer == QMessageBox::Yes) {
            close();
            QProcess::startDetached(QCoreApplication::applicationFilePath(), {});
            qApp->quit();
        }
    };
    connect(aUk, &QAction::triggered, this, [switchLang]() { switchLang(true); });
    connect(aEn, &QAction::triggered, this, [switchLang]() { switchLang(false); });
    QMenu *themeMenu = gearMenu->addMenu(tr2("Тема", "Theme"));
    QAction *aDark = themeMenu->addAction(tr2("Темна", "Dark"));
    QAction *aLight = themeMenu->addAction(tr2("Світла", "Light"));
    aDark->setCheckable(true);
    aLight->setCheckable(true);
    aDark->setChecked(Theme::isDark());
    aLight->setChecked(!Theme::isDark());
    auto switchTheme = [this](bool dark) {
        if (dark == Theme::isDark())
            return;
        Theme::setDark(dark);
        if (QMessageBox::question(this, QStringLiteral("Lumio Photo"),
                                  tr2("Тему змінено. Перезапустити програму зараз?", "Theme changed. Restart now?"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
            == QMessageBox::Yes) {
            close();
            QProcess::startDetached(QCoreApplication::applicationFilePath(), {});
            qApp->quit();
        }
    };
    connect(aDark, &QAction::triggered, this, [switchTheme]() { switchTheme(true); });
    connect(aLight, &QAction::triggered, this, [switchTheme]() { switchTheme(false); });
    gearMenu->addSeparator();
    if (FileAssoc::isSupported())
        gearMenu->addAction(Icons::icon(Icons::Photos), tr2("Відкривати фото в Lumio Photo…", "Open photos with Lumio Photo…"), this,
                            []() { FileAssoc::openDefaultAppsSettings(); });
    gearMenu->addAction(Icons::icon(Icons::Refresh), tr2("Перевірити й оновити…", "Check for updates…"), this,
                        &MainWindow::checkForUpdates);
    gearMenu->addAction(Icons::icon(Icons::Info), tr2("Про програму", "About"), this, &MainWindow::showAbout);
    gear->setMenu(gearMenu);
    tb->addWidget(gear);

    // --- ліва панель: бібліотека
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setIconSize(QSize(18, 18));
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setMinimumWidth(220);
    m_tree->setObjectName(QStringLiteral("libraryTree"));
    connect(m_tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *it) {
        if (!it)
            return;
        const int type = it->data(0, kTypeRole).toInt();
        if (type == ViewAlbumsHeader || type == ViewFoldersHeader)
            return;
        m_viewType = type;
        m_viewValue = it->data(0, kValueRole).toString();
        applyView();
    });
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &MainWindow::showTreeMenu);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(buildLibraryPage());
    m_viewer = new ViewerPage(this);
    m_stack->addWidget(m_viewer);

    m_info = new InfoPanel(this);
    m_info->setFixedWidth(280);
    m_info->setObjectName(QStringLiteral("infoPanel"));
    m_info->setVisible(false);
    connect(m_infoToggle, &QToolButton::toggled, this, [this](bool on) {
        m_info->setVisible(on && m_stack->currentIndex() == 0);
        updateSelectionInfo();
    });
    m_infoToggle->setChecked(AppPaths::settings().value(QStringLiteral("window/info"), false).toBool());

    auto *split = new QSplitter(this);
    split->addWidget(m_tree);
    split->addWidget(m_stack);
    split->addWidget(m_info);
    split->setStretchFactor(1, 1);
    split->setCollapsible(1, false);
    split->setSizes({250, 900, 280});
    setCentralWidget(split);

    m_status = new QLabel(this);
    statusBar()->addWidget(m_status, 1);

    connect(m_search, &QLineEdit::textChanged, this, &MainWindow::applyView);
    connect(m_sort, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::applyView);

    connect(m_info, &InfoPanel::tagsEdited, this, [this](qint64 id, const QString &t) {
        Library::setTags(id, t);
        const int i = m_indexById.value(id, -1);
        if (i >= 0) {
            m_photos[i].tags = t;
            storePhoto(m_photos[i]);
        }
    });
    connect(m_info, &InfoPanel::captionEdited, this, [this](qint64 id, const QString &c) {
        Library::setCaption(id, c);
        const int i = m_indexById.value(id, -1);
        if (i >= 0) {
            m_photos[i].caption = c;
            storePhoto(m_photos[i]);
        }
    });

    connect(m_viewer, &ViewerPage::closed, this, &MainWindow::closeViewer);
    connect(m_viewer, &ViewerPage::photoChanged, this, &MainWindow::storePhoto);
    connect(m_viewer, &ViewerPage::photoRemoved, this, [this](qint64 id) { removePhotos({id}); });
    connect(m_viewer, &ViewerPage::slideshowRequested, this, &MainWindow::startSlideshow);
    connect(m_viewer, &ViewerPage::exportRequested, this, &MainWindow::exportPhotos);

    auto *full = new QAction(this);
    full->setShortcut(QKeySequence(Qt::Key_F11));
    connect(full, &QAction::triggered, this, [this]() { isFullScreen() ? showNormal() : showFullScreen(); });
    addAction(full);
}

QWidget *MainWindow::buildLibraryPage()
{
    auto *page = new QWidget(this);
    auto *l = new QVBoxLayout(page);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);

    m_model = new PhotoModel(this);
    m_delegate = new ThumbDelegate(this);
    m_grid = new QListView(page);
    m_grid->setObjectName(QStringLiteral("photoGrid"));
    m_grid->setModel(m_model);
    m_grid->setItemDelegate(m_delegate);
    m_grid->setViewMode(QListView::IconMode);
    m_grid->setResizeMode(QListView::Adjust);
    m_grid->setMovement(QListView::Static);
    m_grid->setUniformItemSizes(true);
    m_grid->setLayoutMode(QListView::Batched);
    m_grid->setBatchSize(400);
    m_grid->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_grid->setDragEnabled(true);
    m_grid->setDragDropMode(QAbstractItemView::DragOnly);
    m_grid->setMouseTracking(true);
    m_grid->setSpacing(0);
    m_grid->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_grid->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_grid, &QListView::activated, this, [this](const QModelIndex &i) { openViewer(i.row()); });
    connect(m_grid, &QListView::customContextMenuRequested, this, &MainWindow::showGridMenu);
    connect(m_grid->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::updateSelectionInfo);

    m_emptyHint = new QLabel(page);
    m_emptyHint->setAlignment(Qt::AlignCenter);
    m_emptyHint->setWordWrap(true);
    m_emptyHint->setStyleSheet(QStringLiteral("color: #8a909a; font-size: 15px;"));
    m_emptyHint->hide();

    auto *gridHolder = new QWidget(page);
    auto *gh = new QVBoxLayout(gridHolder);
    gh->setContentsMargins(0, 0, 0, 0);
    gh->addWidget(m_grid);
    gh->addWidget(m_emptyHint);
    l->addWidget(gridHolder, 1);

    // --- нижній «лоток» дій
    auto *tray = new QWidget(page);
    tray->setObjectName(QStringLiteral("tray"));
    auto *tl = new QHBoxLayout(tray);
    tl->setContentsMargins(10, 6, 10, 6);
    m_selectionLabel = new QLabel(tray);
    m_selectionLabel->setMinimumWidth(170);
    tl->addWidget(m_selectionLabel);
    tl->addStretch();

    auto addTrayButton = [&](Icons::Id icon, const QString &tip, auto fn) {
        auto *b = new QToolButton(tray);
        b->setIcon(Icons::icon(icon));
        b->setIconSize(QSize(22, 22));
        b->setToolTip(tip);
        b->setAutoRaise(true);
        connect(b, &QToolButton::clicked, this, fn);
        tl->addWidget(b);
        return b;
    };
    addTrayButton(Icons::Star, tr2("Улюблене (S)", "Favorite (S)"), [this]() { toggleStarSelected(); });
    addTrayButton(Icons::RotateLeft, tr2("Повернути ліворуч (Ctrl+L)", "Rotate left (Ctrl+L)"), [this]() { rotateSelected(false); });
    addTrayButton(Icons::RotateRight, tr2("Повернути праворуч (Ctrl+R)", "Rotate right (Ctrl+R)"), [this]() { rotateSelected(true); });
    QToolButton *albumBtn = addTrayButton(Icons::Album, tr2("Додати до альбому", "Add to album"), [] {});
    albumBtn->setPopupMode(QToolButton::InstantPopup);
    auto *albumMenu = new QMenu(albumBtn);
    connect(albumMenu, &QMenu::aboutToShow, this, [this, albumMenu]() {
        albumMenu->clear();
        for (const Library::Album &a : Library::albums())
            albumMenu->addAction(a.name, this, [this, id = a.id]() { addSelectedToAlbum(id); });
        if (!albumMenu->isEmpty())
            albumMenu->addSeparator();
        albumMenu->addAction(Icons::icon(Icons::Plus), tr2("Новий альбом…", "New album…"), this, &MainWindow::newAlbumFromSelection);
    });
    albumBtn->setMenu(albumMenu);
    addTrayButton(Icons::Export, tr2("Експорт (Ctrl+E)", "Export (Ctrl+E)"), [this]() { exportPhotos(selectedPhotos()); });
    addTrayButton(Icons::Copy, tr2("Копіювати (Ctrl+C)", "Copy (Ctrl+C)"), [this]() { copySelected(); });
    addTrayButton(Icons::Print, tr2("Друк (Ctrl+P)", "Print (Ctrl+P)"), [this]() { printSelected(); });
    addTrayButton(Icons::Trash, tr2("Видалити в кошик (Delete)", "Move to Recycle Bin (Delete)"), [this]() { deleteSelected(); });

    tl->addSpacing(16);
    m_sizeSlider = new QSlider(Qt::Horizontal, tray);
    m_sizeSlider->setRange(90, 340);
    m_sizeSlider->setFixedWidth(140);
    m_sizeSlider->setToolTip(tr2("Розмір мініатюр", "Thumbnail size"));
    connect(m_sizeSlider, &QSlider::valueChanged, this, [this](int v) {
        m_delegate->setThumbSize(v);
        m_grid->setGridSize(QSize(v + 14, v + 14));
    });
    m_sizeSlider->setValue(qBound(90, AppPaths::settings().value(QStringLiteral("window/thumbSize"), 170).toInt(), 340));
    tl->addWidget(m_sizeSlider);
    l->addWidget(tray);

    // --- гарячі клавіші сітки
    auto addKey = [this, page](const QKeySequence &ks, auto fn) {
        auto *a = new QAction(page);
        a->setShortcut(ks);
        a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        connect(a, &QAction::triggered, this, fn);
        page->addAction(a);
    };
    addKey(QKeySequence(Qt::Key_S), [this]() { toggleStarSelected(); });
    addKey(QKeySequence(Qt::Key_Delete), [this]() { deleteSelected(); });
    addKey(QKeySequence(QStringLiteral("Ctrl+L")), [this]() { rotateSelected(false); });
    addKey(QKeySequence(QStringLiteral("Ctrl+R")), [this]() { rotateSelected(true); });
    addKey(QKeySequence::Copy, [this]() { copySelected(); });
    addKey(QKeySequence(Qt::Key_F2), [this]() { renameSelected(); });
    addKey(QKeySequence::Print, [this]() { printSelected(); });
    return page;
}

// ---------------------------------------------------------------------------
// Дані

void MainWindow::startup(const QString &openPath)
{
    QString err;
    if (!Library::open(&err)) {
        QMessageBox::critical(this, QStringLiteral("Lumio Photo"), tr2("Не вдалося відкрити бібліотеку:\n%1", "Could not open the library:\n%1").arg(err));
        return;
    }
    if (!AppPaths::isPortable())
        FileAssoc::ensureRegistered(); // щоб Lumio Photo був у «Відкрити за допомогою»
    reloadLibrary();
    if (!openPath.isEmpty()) {
        // відкрито з Провідника: одразу показуємо фото, без вітального вікна
        openFile(openPath);
        rescan();
        return;
    }
    if (Library::roots().isEmpty()) {
        const QString pics = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        QMessageBox box(QMessageBox::Question, tr2("Ласкаво просимо до Lumio Photo", "Welcome to Lumio Photo"),
                        tr2("Звідки брати фото?\n\nLumio Photo лише читає ваші папки — нічого не "
                                       "переміщує, не завантажує в інтернет і не змінює файли.", "Where should photos come from?\n\nLumio Photo only reads your folders — it never moves, uploads or modifies files."),
                        QMessageBox::NoButton, this);
        QPushButton *usePics = box.addButton(tr2("Папка «Зображення»", "Pictures folder"), QMessageBox::AcceptRole);
        QPushButton *choose = box.addButton(tr2("Вибрати папку…", "Choose a folder…"), QMessageBox::ActionRole);
        box.addButton(tr2("Пізніше", "Later"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == usePics && !pics.isEmpty()) {
            Library::addRoot(pics);
        } else if (box.clickedButton() == choose) {
            addFolder();
            return;
        }
    }
    rescan();
}

void MainWindow::rescan()
{
    const QStringList roots = Library::roots();
    if (roots.isEmpty()) {
        reloadLibrary();
        return;
    }
    if (m_scanner->isRunning())
        return;
    m_status->setText(tr2("Пошук фото…", "Looking for photos…"));
    m_scanner->start(roots);
}

void MainWindow::reloadLibrary()
{
    m_photos = Library::loadAll();
    // фото, відкриті з Провідника поза папками бібліотеки, у бібліотеці не показуємо
    const QStringList roots = Library::roots();
    m_photos.erase(std::remove_if(m_photos.begin(), m_photos.end(),
                                  [&roots](const Photo &p) { return !Library::isUnderRoots(p.path, roots); }),
                   m_photos.end());
    m_indexById.clear();
    m_indexById.reserve(m_photos.size());
    for (int i = 0; i < m_photos.size(); ++i)
        m_indexById.insert(m_photos[i].id, i);
    rebuildTree();
    applyView();
    qint64 total = 0;
    for (const Photo &p : std::as_const(m_photos))
        total += p.size;
    m_status->setText(tr2("У бібліотеці %1 фото  •  %2", "%1 photos in the library  •  %2").arg(m_photos.size()).arg(Util::humanSize(total)));
}

void MainWindow::rebuildTree()
{
    const int keepType = m_viewType;
    const QString keepValue = m_viewValue;
    m_tree->blockSignals(true);
    m_tree->clear();

    int starred = 0, recent = 0, edited = 0;
    const qint64 monthAgo = QDateTime::currentSecsSinceEpoch() - 30LL * 24 * 3600;
    QHash<QString, int> perFolder;
    for (const Photo &p : std::as_const(m_photos)) {
        if (p.starred) ++starred;
        if (p.mtime >= monthAgo) ++recent;
        if (!p.edits.isEmpty()) ++edited;
        ++perFolder[p.folder];
    }

    auto mk = [this](QTreeWidgetItem *parent, Icons::Id icon, const QString &text, int type, const QString &value) {
        auto *it = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
        it->setText(0, text);
        it->setIcon(0, Icons::icon(icon));
        it->setData(0, kTypeRole, type);
        it->setData(0, kValueRole, value);
        return it;
    };
    QTreeWidgetItem *select = nullptr;
    auto consider = [&](QTreeWidgetItem *it) {
        if (it->data(0, kTypeRole).toInt() == keepType && it->data(0, kValueRole).toString() == keepValue)
            select = it;
    };

    consider(mk(nullptr, Icons::Photos, tr2("Усі фото (%1)", "All photos (%1)").arg(m_photos.size()), ViewAll, QString()));
    consider(mk(nullptr, Icons::StarFilled, tr2("Улюблені (%1)", "Favorites (%1)").arg(starred), ViewStarred, QString()));
    consider(mk(nullptr, Icons::Clock, tr2("Нові за 30 днів (%1)", "Last 30 days (%1)").arg(recent), ViewRecent, QString()));
    consider(mk(nullptr, Icons::Edit, tr2("Відредаговані (%1)", "Edited (%1)").arg(edited), ViewEdited, QString()));

    QTreeWidgetItem *albums = mk(nullptr, Icons::Album, tr2("Альбоми", "Albums"), ViewAlbumsHeader, QString());
    albums->setFlags(albums->flags() & ~Qt::ItemIsSelectable);
    for (const Library::Album &a : Library::albums())
        consider(mk(albums, Icons::Album, QStringLiteral("%1 (%2)").arg(a.name).arg(a.count), ViewAlbum, QString::number(a.id)));
    albums->setExpanded(true);

    QTreeWidgetItem *folders = mk(nullptr, Icons::Folder, tr2("Папки", "Folders"), ViewFoldersHeader, QString());
    folders->setFlags(folders->flags() & ~Qt::ItemIsSelectable);
    folders->setExpanded(true);
    for (const QString &root : Library::roots()) {
        QHash<QString, QTreeWidgetItem *> nodes;
        QHash<QTreeWidgetItem *, int> counts;
        QTreeWidgetItem *rootItem = mk(folders, Icons::Folder, QDir::toNativeSeparators(root), ViewFolder, root);
        rootItem->setToolTip(0, QDir::toNativeSeparators(root));
        nodes.insert(root, rootItem);
        QStringList subfolders;
        for (auto it = perFolder.constBegin(); it != perFolder.constEnd(); ++it)
            if (it.key() == root || it.key().startsWith(root + QLatin1Char('/')))
                subfolders << it.key();
        subfolders.sort(Qt::CaseInsensitive);
        for (const QString &f : std::as_const(subfolders)) {
            const int n = perFolder.value(f);
            // ланцюжок проміжних папок
            QString cur = root;
            QTreeWidgetItem *parent = rootItem;
            counts[rootItem] += n;
            const QStringList parts = f.mid(root.size()).split(QLatin1Char('/'), Qt::SkipEmptyParts);
            for (const QString &part : parts) {
                cur += QLatin1Char('/') + part;
                QTreeWidgetItem *node = nodes.value(cur);
                if (!node) {
                    node = mk(parent, Icons::Folder, part, ViewFolder, cur);
                    node->setToolTip(0, QDir::toNativeSeparators(cur));
                    nodes.insert(cur, node);
                }
                counts[node] += n;
                parent = node;
            }
        }
        for (auto it = counts.constBegin(); it != counts.constEnd(); ++it)
            it.key()->setText(0, QStringLiteral("%1 (%2)").arg(it.key()->text(0)).arg(it.value()));
        for (QTreeWidgetItem *node : std::as_const(nodes))
            consider(node);
        rootItem->setExpanded(true);
    }

    m_tree->blockSignals(false);
    if (!select)
        select = m_tree->topLevelItem(0);
    m_tree->setCurrentItem(select);
    if (select) {
        m_viewType = select->data(0, kTypeRole).toInt();
        m_viewValue = select->data(0, kValueRole).toString();
    }
}

bool MainWindow::matchesView(const Photo &p, const QSet<qint64> &albumIds) const
{
    switch (m_viewType) {
    case ViewStarred:
        return p.starred;
    case ViewRecent:
        return p.mtime >= QDateTime::currentSecsSinceEpoch() - 30LL * 24 * 3600;
    case ViewEdited:
        return !p.edits.isEmpty();
    case ViewFolder:
        return p.folder == m_viewValue || p.folder.startsWith(m_viewValue + QLatin1Char('/'));
    case ViewAlbum:
        return albumIds.contains(p.id);
    default:
        return true;
    }
}

void MainWindow::applyView()
{
    QSet<qint64> albumIds;
    if (m_viewType == ViewAlbum)
        albumIds = Library::albumPhotos(m_viewValue.toLongLong());

    const QStringList tokens = m_search->text().toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    PhotoList out;
    out.reserve(m_photos.size());
    for (const Photo &p : std::as_const(m_photos)) {
        if (!matchesView(p, albumIds))
            continue;
        if (!tokens.isEmpty()) {
            const QString hay = (p.name + QLatin1Char(' ') + p.tags + QLatin1Char(' ') + p.caption + QLatin1Char(' ')
                                 + QFileInfo(p.folder).fileName())
                                    .toLower();
            bool ok = true;
            for (const QString &t : tokens)
                if (!hay.contains(t)) {
                    ok = false;
                    break;
                }
            if (!ok)
                continue;
        }
        out.append(p);
    }

    switch (m_sort->currentIndex()) {
    case 1:
        std::stable_sort(out.begin(), out.end(), [](const Photo &a, const Photo &b) { return a.taken < b.taken; });
        break;
    case 2:
        std::stable_sort(out.begin(), out.end(), [](const Photo &a, const Photo &b) {
            return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
        });
        break;
    case 3:
        std::stable_sort(out.begin(), out.end(), [](const Photo &a, const Photo &b) { return a.size > b.size; });
        break;
    default:
        std::stable_sort(out.begin(), out.end(), [](const Photo &a, const Photo &b) { return a.taken > b.taken; });
        break;
    }

    ThumbCache::instance()->cancelPending(); // старі запити більше не потрібні
    m_model->setPhotos(out);

    const bool empty = out.isEmpty();
    m_grid->setVisible(!empty);
    m_emptyHint->setVisible(empty);
    if (empty) {
        if (Library::roots().isEmpty())
            m_emptyHint->setText(tr2("У бібліотеці ще немає папок.\n\nНатисніть «Додати папку» вгорі.", "The library has no folders yet.\n\nClick “Add folder” at the top."));
        else if (m_scanner->isRunning())
            m_emptyHint->setText(tr2("Шукаю фото…", "Looking for photos…"));
        else
            m_emptyHint->setText(tr2("Тут немає фото.", "No photos here."));
    }
    updateSelectionInfo();
}

QVector<Photo> MainWindow::selectedPhotos() const
{
    QVector<Photo> out;
    QModelIndexList rows = m_grid->selectionModel()->selectedIndexes();
    std::sort(rows.begin(), rows.end(), [](const QModelIndex &a, const QModelIndex &b) { return a.row() < b.row(); });
    for (const QModelIndex &i : std::as_const(rows))
        out.append(m_model->photo(i.row()));
    return out;
}

QVector<qint64> MainWindow::selectedIds() const
{
    QVector<qint64> out;
    for (const Photo &p : selectedPhotos())
        out.append(p.id);
    return out;
}

void MainWindow::storePhoto(const Photo &p)
{
    const int i = m_indexById.value(p.id, -1);
    if (i >= 0)
        m_photos[i] = p;
    m_model->updatePhoto(p);
    if (m_stack->currentIndex() == 1)
        m_viewer->updatePhoto(p);
    updateSelectionInfo();
}

void MainWindow::removePhotos(const QSet<qint64> &ids)
{
    for (int i = m_photos.size() - 1; i >= 0; --i)
        if (ids.contains(m_photos[i].id))
            m_photos.removeAt(i);
    m_indexById.clear();
    for (int i = 0; i < m_photos.size(); ++i)
        m_indexById.insert(m_photos[i].id, i);
    m_model->removeIds(ids);
    updateSelectionInfo();
}

void MainWindow::updateSelectionInfo()
{
    const QVector<Photo> sel = selectedPhotos();
    if (sel.isEmpty())
        m_selectionLabel->setText(tr2("Фото: %1", "Photos: %1").arg(m_model->rowCount()));
    else
        m_selectionLabel->setText(tr2("Вибрано: %1 з %2", "Selected: %1 of %2").arg(sel.size()).arg(m_model->rowCount()));

    if (!m_info->isVisible())
        return;
    if (sel.size() == 1) {
        m_info->setPhoto(sel.first());
    } else if (sel.isEmpty()) {
        m_info->clear();
    } else {
        qint64 total = 0;
        for (const Photo &p : sel)
            total += p.size;
        m_info->setMultiple(sel.size(), total);
    }
}

// ---------------------------------------------------------------------------
// Дії

void MainWindow::addFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr2("Додати папку з фото", "Add a folder with photos"),
                                                          QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
    if (dir.isEmpty())
        return;
    const QString clean = QDir::cleanPath(QDir::fromNativeSeparators(dir));
    for (const QString &r : Library::roots()) {
        if (clean == r || clean.startsWith(r + QLatin1Char('/'))) {
            QMessageBox::information(this, tr2("Додати папку", "Add folder"),
                                     tr2("Ця папка вже є в бібліотеці (у складі «%1»).", "This folder is already in the library (as part of “%1”).").arg(QDir::toNativeSeparators(r)));
            return;
        }
    }
    // якщо нова папка містить уже додані — замінюємо їх на неї
    for (const QString &r : Library::roots())
        if (r.startsWith(clean + QLatin1Char('/')))
            Library::removeRoot(r);
    Library::addRoot(clean);
    reloadLibrary();
    rescan();
}

void MainWindow::openViewer(int row)
{
    if (row < 0 || row >= m_model->rowCount())
        return;
    m_info->setVisible(false);
    m_stack->setCurrentIndex(1);
    m_viewer->open(m_model->photos(), row);
}

void MainWindow::openFile(const QString &rawPath)
{
    const QFileInfo fi(rawPath);
    if (!fi.isFile()) {
        QMessageBox::warning(this, QStringLiteral("Lumio Photo"),
                             tr2("Файл не знайдено:\n%1", "File not found:\n%1").arg(QDir::toNativeSeparators(rawPath)));
        return;
    }
    const QStringList suffixes = Scanner::supportedSuffixes();
    if (!suffixes.contains(fi.suffix().toLower())) {
        QMessageBox::warning(this, QStringLiteral("Lumio Photo"),
                             tr2("Цей формат не підтримується:\n%1", "This format is not supported:\n%1").arg(fi.fileName()));
        return;
    }

    // сусідні фото в тій самій папці — щоб гортати стрілками, як у звичайному переглядачі
    QStringList filters;
    for (const QString &s : suffixes)
        filters << QStringLiteral("*.") + s;
    QFileInfoList entries = QDir(fi.absolutePath()).entryInfoList(filters, QDir::Files | QDir::Readable);
    std::sort(entries.begin(), entries.end(), [](const QFileInfo &a, const QFileInfo &b) {
        return Util::naturalLess(a.fileName(), b.fileName());
    });
    int index = -1;
    for (int i = 0; i < entries.size(); ++i)
        if (entries[i] == fi) {
            index = i;
            break;
        }
    if (index < 0) {
        entries.prepend(fi);
        index = 0;
    }
    // у дуже великих папках беремо «вікно» навколо відкритого фото, щоб відкриття було миттєвим
    const int kWindow = 300;
    if (entries.size() > 2 * kWindow) {
        const int from = qMax(0, index - kWindow);
        entries = entries.mid(from, 2 * kWindow);
        index -= from;
    }
    QStringList paths;
    paths.reserve(entries.size());
    for (const QFileInfo &e : std::as_const(entries))
        paths << QDir::cleanPath(e.absoluteFilePath());
    const QString target = paths.at(index);

    const PhotoList list = Library::ensurePhotos(paths);
    int row = 0;
    for (int i = 0; i < list.size(); ++i)
        if (QString::compare(list[i].path, target, Qt::CaseInsensitive) == 0) {
            row = i;
            break;
        }
    if (list.isEmpty())
        return;
    m_info->setVisible(false);
    m_stack->setCurrentIndex(1);
    m_viewer->open(list, row);
    bringToFront();
}

void MainWindow::bringToFront()
{
    if (isMinimized())
        showNormal();
    show();
    raise();
    activateWindow();
}

void MainWindow::checkForUpdates()
{
    UpdateDialog dlg({QStringLiteral("Lumio Photo"), QStringLiteral("coldtrace00/LumioPhoto"),
                      QStringLiteral("LumioPhoto-Setup.exe"), QCoreApplication::applicationVersion()},
                     this);
    dlg.exec();
}

void MainWindow::closeViewer()
{
    qint64 id = m_viewer->hasPhoto() ? m_viewer->current().id : 0;
    m_stack->setCurrentIndex(0);
    m_info->setVisible(m_infoToggle->isChecked());
    const int row = m_model->rowOfId(id);
    if (row >= 0) {
        const QModelIndex idx = m_model->index(row);
        m_grid->setCurrentIndex(idx);
        m_grid->scrollTo(idx, QAbstractItemView::PositionAtCenter);
    }
    rebuildTree(); // оновити лічильники (улюблені, відредаговані)
    m_grid->setFocus();
}

void MainWindow::startSlideshow(const PhotoList &list, int index)
{
    if (list.isEmpty())
        return;
    auto *s = new Slideshow(list, index);
    s->setWindowIcon(Icons::appIcon());
    s->startShow();
}

void MainWindow::exportPhotos(const PhotoList &list)
{
    if (list.isEmpty()) {
        QMessageBox::information(this, tr2("Експорт", "Export"), tr2("Спершу виберіть фото.", "Select photos first."));
        return;
    }
    ExportDialog dlg(list, this);
    dlg.exec();
}

void MainWindow::toggleStarSelected()
{
    QVector<Photo> sel = selectedPhotos();
    if (sel.isEmpty())
        return;
    // якщо хоч одне без зірочки — ставимо всім, інакше знімаємо
    const bool newState = std::any_of(sel.begin(), sel.end(), [](const Photo &p) { return !p.starred; });
    QVector<qint64> ids;
    for (Photo &p : sel) {
        p.starred = newState;
        ids << p.id;
        storePhoto(p);
    }
    Library::setStarred(ids, newState);
    rebuildTree();
}

void MainWindow::rotateSelected(bool clockwise)
{
    for (Photo p : selectedPhotos()) {
        EditState e = EditState::fromJson(p.edits);
        Edits::rotate(e, clockwise);
        p.edits = e.toJson();
        Library::setEdits(p.id, p.edits);
        ThumbCache::instance()->invalidate(p.path);
        storePhoto(p);
    }
}

void MainWindow::deleteSelected()
{
    const QVector<Photo> sel = selectedPhotos();
    if (sel.isEmpty())
        return;
    const QString q = sel.size() == 1 ? tr2("Перемістити «%1» у кошик?", "Move “%1” to the Recycle Bin?").arg(sel.first().name)
                                      : tr2("Перемістити вибрані фото (%1 шт.) у кошик?", "Move the selected photos (%1) to the Recycle Bin?").arg(sel.size());
    if (QMessageBox::question(this, tr2("Видалення", "Delete"), q, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes)
        return;
    QSet<qint64> removed;
    QStringList failed;
    for (const Photo &p : sel) {
        if (Util::moveToTrash(p.path)) {
            Library::remove(p.id);
            removed.insert(p.id);
        } else {
            failed << p.name;
        }
    }
    removePhotos(removed);
    rebuildTree();
    if (!failed.isEmpty())
        QMessageBox::warning(this, tr2("Видалення", "Delete"),
                             tr2("Не вдалося перемістити в кошик:\n%1", "Could not move to the Recycle Bin:\n%1").arg(failed.join(QLatin1Char('\n'))));
}

void MainWindow::copySelected()
{
    const QVector<Photo> sel = selectedPhotos();
    if (sel.isEmpty())
        return;
    auto *md = new QMimeData;
    QList<QUrl> urls;
    for (const Photo &p : sel)
        urls << QUrl::fromLocalFile(p.path);
    md->setUrls(urls);
    if (sel.size() == 1) {
        QImage img = Edits::load(sel.first().path, 4096);
        if (!sel.first().edits.isEmpty())
            img = Edits::apply(img, EditState::fromJson(sel.first().edits));
        md->setImageData(img);
    }
    QApplication::clipboard()->setMimeData(md);
    statusBar()->showMessage(tr2("Скопійовано: %1", "Copied: %1").arg(sel.size()), 2500);
}

void MainWindow::renameSelected()
{
    const QVector<Photo> sel = selectedPhotos();
    if (sel.size() != 1)
        return;
    Photo p = sel.first();
    const QFileInfo fi(p.path);
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr2("Перейменувати", "Rename"), tr2("Нова назва:", "New name:"),
                                               QLineEdit::Normal, fi.completeBaseName(), &ok)
                             .trimmed();
    if (!ok || name.isEmpty() || name == fi.completeBaseName())
        return;
    if (name.contains(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*]")))) {
        QMessageBox::warning(this, tr2("Перейменувати", "Rename"), tr2("Назва містить недопустимі символи.", "The name contains invalid characters."));
        return;
    }
    const QString target = fi.absoluteDir().filePath(name + QLatin1Char('.') + fi.suffix());
    if (QFile::exists(target) || !QFile::rename(p.path, target)) {
        QMessageBox::warning(this, tr2("Перейменувати", "Rename"), tr2("Не вдалося перейменувати файл.", "Could not rename the file."));
        return;
    }
    Library::rename(p.id, target);
    p.path = QFileInfo(target).absoluteFilePath();
    p.name = QFileInfo(target).fileName();
    storePhoto(p);
}

void MainWindow::printSelected()
{
    const QVector<Photo> sel = selectedPhotos();
    if (sel.isEmpty())
        return;
#if QT_CONFIG(printdialog)
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dlg(&printer, this);
    dlg.setWindowTitle(tr2("Друк фото (%1)", "Print photos (%1)").arg(sel.size()));
    if (dlg.exec() != QDialog::Accepted)
        return;
    QPainter painter(&printer);
    for (int i = 0; i < sel.size(); ++i) {
        if (i > 0)
            printer.newPage();
        QImage img = Edits::load(sel[i].path, 4000);
        if (!sel[i].edits.isEmpty())
            img = Edits::apply(img, EditState::fromJson(sel[i].edits));
        const QRect page = painter.viewport();
        QSize s = img.size();
        s.scale(page.size(), Qt::KeepAspectRatio);
        const QRect target(page.x() + (page.width() - s.width()) / 2, page.y() + (page.height() - s.height()) / 2, s.width(),
                           s.height());
        painter.drawImage(target, img);
    }
#endif
}

void MainWindow::wallpaperSelected()
{
    const QVector<Photo> sel = selectedPhotos();
    if (sel.size() != 1)
        return;
    QImage img = Edits::load(sel.first().path, 3840);
    if (!sel.first().edits.isEmpty())
        img = Edits::apply(img, EditState::fromJson(sel.first().edits));
    QString err;
    if (!Util::setWallpaper(img, &err))
        QMessageBox::warning(this, tr2("Шпалери", "Wallpaper"), err);
}

void MainWindow::addSelectedToAlbum(qint64 albumId)
{
    const QVector<qint64> ids = selectedIds();
    if (ids.isEmpty())
        return;
    Library::addToAlbum(albumId, ids);
    rebuildTree();
    statusBar()->showMessage(tr2("Додано до альбому: %1", "Added to album: %1").arg(ids.size()), 2500);
}

void MainWindow::newAlbumFromSelection()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr2("Новий альбом", "New album"), tr2("Назва альбому:", "Album name:"),
                                               QLineEdit::Normal, QString(), &ok)
                             .trimmed();
    if (!ok || name.isEmpty())
        return;
    const qint64 id = Library::createAlbum(name);
    if (id)
        addSelectedToAlbum(id);
    rebuildTree();
}

void MainWindow::removeSelectedFromAlbum()
{
    if (m_viewType != ViewAlbum)
        return;
    Library::removeFromAlbum(m_viewValue.toLongLong(), selectedIds());
    rebuildTree();
    applyView();
}

void MainWindow::showGridMenu(const QPoint &pos)
{
    const QModelIndex idx = m_grid->indexAt(pos);
    if (idx.isValid() && !m_grid->selectionModel()->isSelected(idx))
        m_grid->setCurrentIndex(idx);
    const QVector<Photo> sel = selectedPhotos();
    if (sel.isEmpty())
        return;
    QMenu menu(this);
    menu.addAction(Icons::icon(Icons::Edit), tr2("Відкрити й редагувати", "Open and edit"), this, [this, idx]() {
        openViewer(idx.isValid() ? idx.row() : m_grid->currentIndex().row());
    });
    menu.addAction(Icons::icon(Icons::Open), tr2("Відкрити в іншій програмі", "Open in another app"), this, [sel]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(sel.first().path));
    });
    menu.addSeparator();
    menu.addAction(Icons::icon(Icons::StarFilled, QColor(255, 200, 60)), tr2("Улюблене\tS", "Favorite\tS"), this,
                   &MainWindow::toggleStarSelected);
    menu.addAction(Icons::icon(Icons::RotateLeft), tr2("Повернути ліворуч\tCtrl+L", "Rotate left\tCtrl+L"), this, [this]() { rotateSelected(false); });
    menu.addAction(Icons::icon(Icons::RotateRight), tr2("Повернути праворуч\tCtrl+R", "Rotate right\tCtrl+R"), this, [this]() { rotateSelected(true); });
    QMenu *albums = menu.addMenu(Icons::icon(Icons::Album), tr2("Додати до альбому", "Add to album"));
    for (const Library::Album &a : Library::albums())
        albums->addAction(a.name, this, [this, id = a.id]() { addSelectedToAlbum(id); });
    if (!albums->isEmpty())
        albums->addSeparator();
    albums->addAction(Icons::icon(Icons::Plus), tr2("Новий альбом…", "New album…"), this, &MainWindow::newAlbumFromSelection);
    if (m_viewType == ViewAlbum)
        menu.addAction(tr2("Прибрати з цього альбому", "Remove from this album"), this, &MainWindow::removeSelectedFromAlbum);
    menu.addSeparator();
    menu.addAction(Icons::icon(Icons::Export), tr2("Експорт…\tCtrl+E", "Export…\tCtrl+E"), this, [this]() { exportPhotos(selectedPhotos()); });
    menu.addAction(Icons::icon(Icons::Copy), tr2("Копіювати\tCtrl+C", "Copy\tCtrl+C"), this, &MainWindow::copySelected);
    menu.addAction(Icons::icon(Icons::Print), tr2("Друк…\tCtrl+P", "Print…\tCtrl+P"), this, &MainWindow::printSelected);
    if (sel.size() == 1) {
        menu.addAction(Icons::icon(Icons::Wallpaper), tr2("Встановити як шпалери", "Set as wallpaper"), this, &MainWindow::wallpaperSelected);
        menu.addAction(Icons::icon(Icons::Folder), tr2("Показати в папці", "Show in folder"), this, [sel]() { Util::showInFolder(sel.first().path); });
        menu.addAction(tr2("Перейменувати…\tF2", "Rename…\tF2"), this, &MainWindow::renameSelected);
    }
    menu.addSeparator();
    menu.addAction(Icons::icon(Icons::Trash), tr2("Видалити в кошик\tDelete", "Move to Recycle Bin\tDelete"), this, &MainWindow::deleteSelected);
    menu.exec(m_grid->viewport()->mapToGlobal(pos));
}

void MainWindow::showTreeMenu(const QPoint &pos)
{
    QTreeWidgetItem *it = m_tree->itemAt(pos);
    QMenu menu(this);
    const int type = it ? it->data(0, kTypeRole).toInt() : 0;
    const QString value = it ? it->data(0, kValueRole).toString() : QString();

    if (type == ViewAlbum) {
        const qint64 id = value.toLongLong();
        menu.addAction(Icons::icon(Icons::Slideshow), tr2("Слайдшоу альбому", "Album slideshow"), this, [this, it]() {
            m_tree->setCurrentItem(it);
            startSlideshow(m_model->photos(), 0);
        });
        menu.addAction(tr2("Перейменувати…", "Rename…"), this, [this, id, it]() {
            QString name = it->text(0);
            name = name.left(name.lastIndexOf(QStringLiteral(" (")));
            bool ok = false;
            const QString n = QInputDialog::getText(this, tr2("Перейменувати альбом", "Rename album"), tr2("Назва:", "Name:"),
                                                    QLineEdit::Normal, name, &ok).trimmed();
            if (ok && !n.isEmpty()) {
                Library::renameAlbum(id, n);
                rebuildTree();
            }
        });
        menu.addAction(Icons::icon(Icons::Trash), tr2("Видалити альбом", "Delete album"), this, [this, id]() {
            if (QMessageBox::question(this, tr2("Видалити альбом", "Delete album"),
                                      tr2("Видалити альбом? Самі фото залишаться на місці.", "Delete the album? The photos themselves stay where they are."))
                == QMessageBox::Yes) {
                Library::deleteAlbum(id);
                m_viewType = ViewAll;
                rebuildTree();
                applyView();
            }
        });
    } else if (type == ViewAlbumsHeader) {
        menu.addAction(Icons::icon(Icons::Plus), tr2("Новий альбом…", "New album…"), this, [this]() {
            bool ok = false;
            const QString n = QInputDialog::getText(this, tr2("Новий альбом", "New album"), tr2("Назва альбому:", "Album name:"),
                                                    QLineEdit::Normal, QString(), &ok).trimmed();
            if (ok && !n.isEmpty()) {
                Library::createAlbum(n);
                rebuildTree();
            }
        });
    } else if (type == ViewFolder) {
        menu.addAction(Icons::icon(Icons::Folder), tr2("Показати в провіднику", "Show in Explorer"), this, [value]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(value));
        });
        menu.addAction(Icons::icon(Icons::Slideshow), tr2("Слайдшоу папки", "Folder slideshow"), this, [this, it]() {
            m_tree->setCurrentItem(it);
            startSlideshow(m_model->photos(), 0);
        });
        if (Library::roots().contains(value)) {
            menu.addSeparator();
            menu.addAction(tr2("Прибрати з бібліотеки", "Remove from library"), this, [this, value]() {
                if (QMessageBox::question(this, tr2("Прибрати папку", "Remove folder"),
                                          tr2("Прибрати «%1» з бібліотеки?\n\nФайли на диску НЕ видаляються — "
                                                         "Lumio Photo просто перестане їх показувати.", "Remove “%1” from the library?\n\nFiles on disk are NOT deleted — Lumio Photo will simply stop showing them.")
                                              .arg(QDir::toNativeSeparators(value)))
                    == QMessageBox::Yes) {
                    Library::removeRoot(value);
                    m_viewType = ViewAll;
                    reloadLibrary();
                }
            });
        }
    } else if (type == ViewFoldersHeader) {
        menu.addAction(Icons::icon(Icons::FolderAdd), tr2("Додати папку…", "Add folder…"), this, &MainWindow::addFolder);
    }
    if (!menu.isEmpty())
        menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

void MainWindow::showAbout()
{
    QMessageBox box(this);
    box.setWindowTitle(tr2("Про Lumio Photo", "About Lumio Photo"));
    box.setIconPixmap(Icons::appPixmap(72));
    box.setTextFormat(Qt::RichText);
    box.setText(tr2("<h3>Lumio Photo %1</h3>"
        "<p>Швидкий локальний організатор і редактор фото — відкрита заміна Picasa.</p>"
        "<p><b>Приватність:</b> жодної телеметрії, аналітики чи хмари. Єдиний мережевий запит — перевірка "
        "оновлень на GitHub, і лише коли ви самі її запускаєте. Метадані у ваші файли не записуються; зірочки, "
        "теги, альбоми й правки зберігаються лише в базі програми:<br><code>%2</code></p>"
        "<p>Ліцензія MIT. Qt %3.<br><a href=\"https://github.com/coldtrace00/LumioPhoto\">github.com/coldtrace00/LumioPhoto</a></p>",
        "<h3>Lumio Photo %1</h3><p>A fast local photo organizer and editor — an open replacement for Picasa.</p>"
        "<p><b>Privacy:</b> no telemetry, analytics or cloud. The only network request is the update check on GitHub, "
        "and only when you start it yourself. No metadata is written to your files; stars, tags, albums and edits are "
        "stored only in the program\'s database:<br><code>%2</code></p>"
        "<p>MIT license. Qt %3.<br><a href=\"https://github.com/coldtrace00/LumioPhoto\">github.com/coldtrace00/LumioPhoto</a></p>")
                    .arg(QCoreApplication::applicationVersion(), QDir::toNativeSeparators(AppPaths::dataDir()),
                         QString::fromLatin1(qVersion())));
    box.exec();
}
