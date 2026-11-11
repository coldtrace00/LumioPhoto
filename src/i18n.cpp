#include "i18n.h"

#include "library.h"

#include <QLocale>
#include <QSettings>

namespace I18n {

namespace {
int g_lang = -1; // -1 — ще не визначено, 1 — українська, 0 — англійська
}

bool isUkrainian()
{
    if (g_lang < 0) {
        const QString saved = AppPaths::settings().value(QStringLiteral("ui/language")).toString();
        if (saved == QLatin1String("uk")) {
            g_lang = 1;
        } else if (saved == QLatin1String("en")) {
            g_lang = 0;
        } else {
            const QLocale sys = QLocale::system();
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
            const bool ukraine = sys.territory() == QLocale::Ukraine;
#else
            const bool ukraine = sys.country() == QLocale::Ukraine;
#endif
            g_lang = (sys.language() == QLocale::Ukrainian || ukraine) ? 1 : 0;
        }
    }
    return g_lang == 1;
}

void setUkrainian(bool on)
{
    AppPaths::settings().setValue(QStringLiteral("ui/language"), on ? QStringLiteral("uk") : QStringLiteral("en"));
    AppPaths::settings().sync();
}

} // namespace I18n
