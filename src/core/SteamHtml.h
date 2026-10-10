#pragma once

#include <QList>
#include <QString>

// Lightweight extractors for the Steam Community pages we scrape. The original
// app used HtmlAgilityPack XPath queries; these functions reproduce the same
// selectors without a full DOM dependency.
namespace SteamHtml {

struct BadgeRow
{
    QString appId;
    QString name;
    double hours = 0.0;
    int cards = 0;
    bool border = false;
};

struct OwnedGame
{
    QString appId;
    QString name;
};

QString decodeHtml(QString text);
QString stripTags(QString text);

// Parses the badge list page (div.badge_row.is_link).
QList<BadgeRow> parseBadgeRows(const QString& html);

// Highest ?p= number found on the badge list page (1 when none).
int totalBadgePages(const QString& html);

// Parses a single gamecards/{appid} page.
bool parseGameCards(const QString& html, int* cards, double* hours,
                    bool* found = nullptr);

// Parses <steamID> from the ?xml=1 profile response.
QString parseSteamIdXml(const QString& xml);

// Parses the owned games list (games?tab=all&xml=1).
QList<OwnedGame> parseOwnedGames(const QString& xml);

} // namespace SteamHtml
