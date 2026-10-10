#include "SteamProfile.h"

#include "HttpClient.h"
#include "Settings.h"
#include "SteamHtml.h"

#include <QObject>
#include <QUrl>

namespace SteamProfile {

QString steamId()
{
    QString secure = Settings::instance().steamLoginSecure();
    if (secure.isEmpty())
        return QString();

    secure = QUrl::fromPercentEncoding(secure.toUtf8());
    const int pipe = secure.indexOf(QLatin1Char('|'));
    if (pipe >= 0)
        secure = secure.left(pipe);
    return secure.trimmed();
}

QString steamUrl()
{
    const QString id = steamId();
    if (id.isEmpty())
        return QString();
    return QStringLiteral("https://steamcommunity.com/profiles/%1").arg(id);
}

QString signedAs()
{
    const QString id = steamId();
    if (id.isEmpty())
        return QString();

    const QString xml = HttpClient::instance().getSync(
        QUrl(QStringLiteral("https://steamcommunity.com/profiles/%1/?xml=1").arg(id)));
    const QString name = SteamHtml::parseSteamIdXml(xml);
    return QObject::tr("signed in as %1").arg(name.isEmpty() ? QStringLiteral("User %1").arg(id)
                                                             : name);
}

} // namespace SteamProfile
