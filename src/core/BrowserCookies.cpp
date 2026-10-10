#include "BrowserCookies.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QVariant>

namespace {

QString firefoxProfilesRoot()
{
    const QString roaming = qEnvironmentVariable("APPDATA") + QStringLiteral("/Mozilla/Firefox");
    if (QFile::exists(roaming + QStringLiteral("/profiles.ini")))
        return roaming;
    const QString local = qEnvironmentVariable("LOCALAPPDATA") + QStringLiteral("/Mozilla/Firefox");
    if (QFile::exists(local + QStringLiteral("/profiles.ini")))
        return local;
    return roaming;
}

// The profile path listed in profiles.ini is relative to the Firefox root
// unless it is absolute.
QString resolveProfile(const QString& root, const QString& path)
{
    if (path.isEmpty())
        return QString();
    if (QFileInfo(path).isAbsolute())
        return QDir::cleanPath(path);
    return QDir::cleanPath(root + QLatin1Char('/') + path);
}

bool hasCookies(const QString& dir)
{
    return !dir.isEmpty() && QFile::exists(QDir(dir).filePath(QStringLiteral("cookies.sqlite")));
}

} // namespace

namespace BrowserCookies {

Browser defaultBrowser()
{
    QSettings reg(QStringLiteral(
                      "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\Shell\\Associations"
                      "\\UrlAssociations\\http\\UserChoice"),
                  QSettings::NativeFormat);
    const QString progId = reg.value(QStringLiteral("ProgId")).toString();
    if (progId.startsWith(QLatin1String("FirefoxURL")))
        return Browser::Firefox;
    if (progId == QLatin1String("ChromeHTML"))
        return Browser::Chrome;
    if (progId == QLatin1String("MSEdgeHTM"))
        return Browser::Edge;
    if (progId.startsWith(QLatin1String("BraveHTML")))
        return Browser::Brave;
    return Browser::Unknown;
}

QString browserName(Browser browser)
{
    switch (browser) {
    case Browser::Firefox:
        return QStringLiteral("Firefox");
    case Browser::Chrome:
        return QStringLiteral("Chrome");
    case Browser::Edge:
        return QStringLiteral("Edge");
    case Browser::Brave:
        return QStringLiteral("Brave");
    case Browser::Unknown:
        break;
    }
    return QStringLiteral("default browser");
}

QString firefoxProfileDir()
{
    const QString root = firefoxProfilesRoot();

    // 1) Modern layout: [Install<hash>] Default=<relative profile path>.
    QSettings ini(root + QStringLiteral("/profiles.ini"), QSettings::IniFormat);
    for (const QString& group : ini.childGroups()) {
        if (!group.startsWith(QLatin1String("Install")))
            continue;
        ini.beginGroup(group);
        const QString def = ini.value(QStringLiteral("Default")).toString();
        ini.endGroup();
        const QString dir = resolveProfile(root, def);
        if (hasCookies(dir))
            return dir;
    }

    // 2) Legacy layout: [ProfileN] with Default=1.
    for (const QString& group : ini.childGroups()) {
        if (!group.startsWith(QLatin1String("Profile")))
            continue;
        ini.beginGroup(group);
        const bool isDefault = ini.value(QStringLiteral("Default"), 0).toInt() == 1;
        const QString path = ini.value(QStringLiteral("Path")).toString();
        ini.endGroup();
        if (isDefault) {
            const QString dir = resolveProfile(root, path);
            if (hasCookies(dir))
                return dir;
        }
    }

    // 3) Fallback: the most recently used profile that has a cookie database.
    const QDir profiles(root + QStringLiteral("/Profiles"));
    const QFileInfoList candidates =
        profiles.entryInfoList(QStringList() << QStringLiteral("*/cookies.sqlite"), QDir::Files);
    QFileInfo best;
    for (const QFileInfo& fi : candidates) {
        if (!best.exists() || fi.lastModified() > best.lastModified())
            best = fi;
    }
    if (best.exists())
        return best.absolutePath();

    return QString();
}

QString firefoxCookie(const QString& cookieName, const QString& hostContains)
{
    const QString profile = firefoxProfileDir();
    if (profile.isEmpty())
        return QString();

    const QString source = QDir(profile).filePath(QStringLiteral("cookies.sqlite"));
    if (!QFile::exists(source))
        return QString();

    // Work on a throwaway copy so a running Firefox is never locked or altered.
    QTemporaryDir tmp(QDir::temp().filePath(QStringLiteral("copper-ff-cookies-XXXXXX")));
    if (!tmp.isValid())
        return QString();

    const QString copyPath = QDir(tmp.path()).filePath(QStringLiteral("cookies.sqlite"));
    if (!QFile::copy(source, copyPath))
        return QString();
    for (const QString& suffix : {QStringLiteral("-wal"), QStringLiteral("-shm")}) {
        const QString sidecar = source + suffix;
        if (QFile::exists(sidecar))
            QFile::copy(sidecar, copyPath + suffix);
    }

    QString value;
    const QString connection =
        QStringLiteral("copper_ff_%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        db.setDatabaseName(copyPath);
        if (db.open()) {
            QSqlQuery query(db);
            query.prepare(QStringLiteral("SELECT value FROM moz_cookies "
                                         "WHERE name = ? AND host LIKE ? "
                                         "ORDER BY lastAccessed DESC LIMIT 1"));
            query.addBindValue(cookieName);
            query.addBindValue(QStringLiteral("%") + hostContains + QStringLiteral("%"));
            if (query.exec() && query.next())
                value = query.value(0).toString();
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connection);
    return value;
}

} // namespace BrowserCookies
