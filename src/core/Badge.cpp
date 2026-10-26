#include "Badge.h"

#include "HttpClient.h"
#include "IdleBackend.h"
#include "SteamHtml.h"

#include <QUrl>

Badge::Badge(int appId, const QString& name)
    : m_appId(appId)
    , m_name(name)
{
}

QString Badge::stringId() const
{
    return QString::number(m_appId);
}

bool Badge::inIdle() const
{
    return IdleBackend::instance().isIdling(m_appId);
}

void Badge::idle()
{
    IdleBackend::instance().startIdle(m_appId);
}

void Badge::stopIdle()
{
    IdleBackend::instance().stopIdle(m_appId);
}

Badge::DropStatus Badge::refreshDrops(const QString& profileUrl)
{
    if (profileUrl.isEmpty())
        return DropStatus::Error;

    const QUrl url(QStringLiteral("%1/gamecards/%2").arg(profileUrl, stringId()));
    int status = 0;
    bool ok = false;
    const QString html = HttpClient::instance().getSync(url, 20000, &ok, &status);

    // Never treat a network / rate-limit failure as "no drops".
    if (!ok || html.isEmpty() || HttpClient::instance().isRateLimited())
        return DropStatus::Error;

    int cards = 0;
    double hours = 0.0;
    bool found = false;
    SteamHtml::parseGameCards(html, &cards, &hours, &found);
    if (!found)
        return DropStatus::Error;

    updateStats(cards, hours);
    return cards == 0 ? DropStatus::NoDrops : DropStatus::DropsAvailable;
}

void Badge::updateStats(int remainingCards, double hoursPlayed)
{
    m_remainingCard = remainingCards;
    m_hoursPlayed = hoursPlayed;
}
