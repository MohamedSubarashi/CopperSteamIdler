#include "Settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

Settings::Settings()
    : m_s(Settings::dataDirectory() + "/settings.ini", QSettings::IniFormat)
{
}

Settings& Settings::instance()
{
    static Settings s;
    return s;
}

void Settings::upgrade()
{
    // Migrate the old multi-cookie login. Only steamLoginSecure is needed now.
    const QStringList obsoleteCookies = {
        QStringLiteral("sessionid"),
        QStringLiteral("steamLogin"),
        QStringLiteral("steamparental"),
        QStringLiteral("steamRememberLogin"),
        QStringLiteral("steamMachineAuth"),
    };
    bool changed = false;
    for (const QString& key : obsoleteCookies) {
        if (m_s.contains(key)) {
            m_s.remove(key);
            changed = true;
        }
    }

    // Migrate the idling-mode flags into the new selection/strategy layout.
    if (!m_s.contains("SelectionMode")) {
        if (m_s.value("IdlingModeWhitelist", false).toBool()) {
            m_s.setValue("SelectionMode", QStringLiteral("picked"));
            // Old whitelist mode idled one at a time; keep that unless the
            // user opts into simultaneous whitelist idling.
            m_s.setValue("fastMode", false);
            m_s.setValue("OneThenMany", false);
            m_s.setValue("OnlyOneGameIdle", true);
            changed = true;
        } else {
            m_s.setValue("SelectionMode", QStringLiteral("all"));
            changed = true;
        }
    }

    if (!m_s.contains("DoneAction") && m_s.value("ShutdownWindowsOnDone", false).toBool()) {
        m_s.setValue("DoneAction", 1);
        changed = true;
    }

    if (changed)
        m_s.sync();
}

QString Settings::dataDirectory()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return base.isEmpty()
               ? QCoreApplication::applicationDirPath() + "/data"
               : base;
}
