#include "theme.h"

#include "library.h"

#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QStyleFactory>

namespace Theme {

namespace {
int g_dark = -1;

struct Colors {
    const char *window, *base, *text, *mid, *tooltip, *disabled, *placeholder;
    const char *bar, *border, *hover, *checked, *panel, *grid, *input, *inputBorder, *button, *buttonHover,
        *status, *statusText, *groove, *handle, *menu;
};

const Colors kDark = {"#202125", "#18191c", "#e6e8ec", "#303238", "#282a30", "#6e727a", "#82878f",
                      "#202125", "#2c2e33", "#33363d", "#2b4a7d", "#1d1e22", "#18191c", "#2a2c31", "#3a3d44",
                      "#2f3238", "#383b42", "#1b1c20", "#9aa0aa", "#3a3d44", "#e6e8ec", "#26282d"};

const Colors kLight = {"#f3f4f6", "#ffffff", "#1f2328", "#e4e6ea", "#ffffff", "#9aa0a8", "#8a9099",
                       "#f7f8fa", "#dcdfe4", "#e3e6eb", "#cfe0fc", "#eef0f3", "#fafbfc", "#ffffff", "#c9cdd4",
                       "#ffffff", "#eef0f3", "#eceef1", "#5b616b", "#c9cdd4", "#ffffff", "#ffffff"};
} // namespace

bool isDark()
{
    if (g_dark < 0)
        g_dark = AppPaths::settings().value(QStringLiteral("ui/theme"), QStringLiteral("dark")).toString()
                         != QLatin1String("light")
                     ? 1
                     : 0;
    return g_dark == 1;
}

void setDark(bool dark)
{
    AppPaths::settings().setValue(QStringLiteral("ui/theme"), dark ? QStringLiteral("dark") : QStringLiteral("light"));
    AppPaths::settings().sync();
}

QColor iconColor()
{
    return isDark() ? QColor(230, 232, 236) : QColor(52, 58, 66);
}

QColor canvasColor()
{
    return isDark() ? QColor(24, 25, 28) : QColor(232, 234, 238);
}

void apply(QApplication &app)
{
    const Colors &c = isDark() ? kDark : kLight;
    auto col = [](const char *hex) { return QColor(QString::fromLatin1(hex)); };

    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette p;
    p.setColor(QPalette::Window, col(c.window));
    p.setColor(QPalette::WindowText, col(c.text));
    p.setColor(QPalette::Base, col(c.base));
    p.setColor(QPalette::AlternateBase, col(c.mid));
    p.setColor(QPalette::ToolTipBase, col(c.tooltip));
    p.setColor(QPalette::ToolTipText, col(c.text));
    p.setColor(QPalette::Text, col(c.text));
    p.setColor(QPalette::Button, col(c.mid));
    p.setColor(QPalette::ButtonText, col(c.text));
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Highlight, QColor(59, 130, 246));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link, isDark() ? QColor(96, 165, 250) : QColor(37, 99, 235));
    p.setColor(QPalette::PlaceholderText, col(c.placeholder));
    p.setColor(QPalette::Disabled, QPalette::Text, col(c.disabled));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, col(c.disabled));
    p.setColor(QPalette::Disabled, QPalette::WindowText, col(c.disabled));
    app.setPalette(p);

    QString css = QStringLiteral(R"(
        QToolBar { background: @bar; border: none; border-bottom: 1px solid @border; padding: 4px; spacing: 4px; }
        QToolButton { border-radius: 6px; padding: 4px 6px; }
        QToolButton:hover { background: @hover; }
        QToolButton:checked { background: @checked; }
        QTreeWidget#libraryTree { background: @panel; border: none; padding: 6px; }
        QTreeWidget#libraryTree::item { padding: 4px 2px; border-radius: 5px; }
        QTreeWidget#libraryTree::item:selected { background: @checked; color: @text; }
        QListView#photoGrid { background: @grid; border: none; }
        QWidget#tray, QWidget#topBar { background: @bar; border-top: 1px solid @border; }
        QWidget#topBar { border-top: none; border-bottom: 1px solid @border; }
        QWidget#editPanel { background: @panel; border-right: 1px solid @border; }
        InfoPanel { background: @panel; }
        QLineEdit { background: @input; border: 1px solid @inputBorder; border-radius: 6px; padding: 5px 8px; }
        QLineEdit:focus { border-color: #3b82f6; }
        QPushButton { background: @button; border: 1px solid @inputBorder; border-radius: 6px; padding: 6px 12px; }
        QPushButton:hover { background: @buttonHover; }
        QPushButton:default { background: #2563eb; border-color: #2563eb; color: #ffffff; }
        QComboBox { background: @input; border: 1px solid @inputBorder; border-radius: 6px; padding: 4px 8px; }
        QTabBar::tab { padding: 6px 12px; }
        QSplitter::handle { background: @border; }
        QStatusBar { background: @status; color: @statusText; }
        QSlider::groove:horizontal { height: 4px; background: @groove; border-radius: 2px; }
        QSlider::sub-page:horizontal { background: #3b82f6; border-radius: 2px; }
        QSlider::handle:horizontal { background: @handle; border: 1px solid @inputBorder; width: 14px; margin: -6px 0; border-radius: 7px; }
        QMenu { background: @menu; border: 1px solid @inputBorder; padding: 4px; }
        QMenu::item { padding: 6px 22px 6px 10px; border-radius: 4px; }
        QMenu::item:selected { background: @checked; color: @text; }
    )");
    const QList<QPair<const char *, const char *>> vars = {
        {"@bar", c.bar}, {"@border", c.border}, {"@hover", c.hover}, {"@checked", c.checked}, {"@panel", c.panel},
        {"@grid", c.grid}, {"@inputBorder", c.inputBorder}, {"@input", c.input}, {"@buttonHover", c.buttonHover},
        {"@button", c.button}, {"@statusText", c.statusText}, {"@status", c.status}, {"@groove", c.groove},
        {"@handle", c.handle}, {"@menu", c.menu}, {"@text", c.text},
    };
    for (const auto &v : vars)
        css.replace(QLatin1String(v.first), QLatin1String(v.second));
    app.setStyleSheet(css);
}

} // namespace Theme
