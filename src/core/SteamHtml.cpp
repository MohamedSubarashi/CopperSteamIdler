#include "SteamHtml.h"

#include <QRegularExpression>
#include <QStringList>

namespace {

const QRegularExpression::PatternOptions kDotAll =
    QRegularExpression::DotMatchesEverythingOption;

QString firstGroup(const QString& text, const QRegularExpression& re)
{
    const QRegularExpressionMatch m = re.match(text);
    return m.hasMatch() ? m.captured(1) : QString();
}

double parseHours(const QString& text)
{
    static const QRegularExpression numberRe(QStringLiteral("([0-9][0-9\\.,]*)"));
    const QRegularExpressionMatch m = numberRe.match(text);
    if (!m.hasMatch())
        return 0.0;
    QString value = m.captured(1);
    value.replace(',', '.');
    bool ok = false;
    const double hours = value.toDouble(&ok);
    return ok ? hours : 0.0;
}

int parseCardCount(const QString& text)
{
    static const QRegularExpression numberRe(QStringLiteral("([0-9]+)"));
    const QRegularExpressionMatch m = numberRe.match(text);
    return m.hasMatch() ? m.captured(1).toInt() : 0;
}

} // namespace

namespace SteamHtml {

QString decodeHtml(QString text)
{
    text.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    text.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    text.replace(QStringLiteral("&apos;"), QStringLiteral("'"));

    static const QRegularExpression numericRe(QStringLiteral("&#(x?[0-9a-fA-F]+);"));
    int offset = 0;
    while (true) {
        const QRegularExpressionMatch m = numericRe.match(text, offset);
        if (!m.hasMatch())
            break;
        const QString token = m.captured(1);
        bool ok = false;
        const uint code = token.startsWith(QLatin1Char('x'), Qt::CaseInsensitive)
                              ? token.mid(1).toUInt(&ok, 16)
                              : token.toUInt(&ok, 10);
        if (!ok)
            break;
        text.replace(m.capturedStart(0), m.capturedLength(0), QString(QChar(code)));
        offset = m.capturedStart(0) + 1;
    }
    return text;
}

QString stripTags(QString text)
{
    static const QRegularExpression tagRe(QStringLiteral("<[^>]*>"));
    text.remove(tagRe);
    return text;
}

QList<BadgeRow> parseBadgeRows(const QString& html)
{
    QList<BadgeRow> rows;
    static const QRegularExpression rowRe(QStringLiteral("badge_row is_link"));
    static const QRegularExpression appIdRe(QStringLiteral("gamecards/(\\d+)/"));
    static const QRegularExpression borderRe(QStringLiteral("border=1"),
                                             QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression hoursRe(
        QStringLiteral("badge_title_stats_playtime[^>]*>(.*?)</div>"), kDotAll);
    static const QRegularExpression nameRe(
        QStringLiteral("class=\"badge_title\"[^>]*>(.*?)</div>"), kDotAll);
    static const QRegularExpression cardsRe(
        QStringLiteral("progress_info_bold[^>]*>(.*?)</span>"), kDotAll);

    QList<QRegularExpressionMatch> starts;
    auto it = rowRe.globalMatch(html);
    while (it.hasNext())
        starts.append(it.next());

    for (int i = 0; i < starts.size(); ++i) {
        const int begin = starts[i].capturedStart();
        const int end = (i + 1 < starts.size()) ? starts[i + 1].capturedStart()
                                                : html.size();
        const QString block = html.mid(begin, end - begin);

        BadgeRow row;
        row.border = borderRe.match(block).hasMatch();
        row.appId = firstGroup(block, appIdRe);
        if (row.appId.isEmpty() || row.border)
            continue;

        row.hours = parseHours(firstGroup(block, hoursRe));
        row.name = decodeHtml(stripTags(firstGroup(block, nameRe))).trimmed();
        row.cards = parseCardCount(firstGroup(block, cardsRe));
        rows.append(row);
    }
    return rows;
}

int totalBadgePages(const QString& html)
{
    static const QRegularExpression pageRe(
        QStringLiteral("class=\"pagelink\"[^>]*href=\"[^\"]*[?&]p=(\\d+)\""));
    int maxPage = 1;
    auto it = pageRe.globalMatch(html);
    while (it.hasNext()) {
        const int page = it.next().captured(1).toInt();
        if (page > maxPage)
            maxPage = page;
    }
    return maxPage;
}

bool parseGameCards(const QString& html, int* cards, double* hours, bool* found)
{
    static const QRegularExpression hoursRe(
        QStringLiteral("badge_title_stats_playtime[^>]*>(.*?)</div>"), kDotAll);
    static const QRegularExpression cardsRe(
        QStringLiteral("progress_info_bold[^>]*>(.*?)</span>"), kDotAll);

    const QRegularExpressionMatch cardsMatch = cardsRe.match(html);
    const QRegularExpressionMatch hoursMatch = hoursRe.match(html);

    const bool hasCards = cardsMatch.hasMatch();
    if (found)
        *found = hasCards || hoursMatch.hasMatch();

    if (cards)
        *cards = hasCards ? parseCardCount(cardsMatch.captured(1)) : 0;
    if (hours)
        *hours = hoursMatch.hasMatch() ? parseHours(hoursMatch.captured(1)) : 0.0;

    return hasCards;
}

QString parseSteamIdXml(const QString& xml)
{
    static const QRegularExpression idRe(QStringLiteral("<steamID>(.*?)</steamID>"), kDotAll);
    QString name = decodeHtml(firstGroup(xml, idRe)).trimmed();

    // Steam wraps the persona name in a CDATA section:
    //   <steamID><![CDATA[PlayerName]]></steamID>
    // so unwrap it instead of showing the literal <![CDATA[...]]> markers.
    static const QRegularExpression cdataRe(
        QStringLiteral("^<!\\[CDATA\\[(.*?)\\]\\]>$"), kDotAll);
    const QRegularExpressionMatch cdata = cdataRe.match(name);
    if (cdata.hasMatch())
        name = cdata.captured(1).trimmed();

    return name;
}

QList<OwnedGame> parseOwnedGames(const QString& xml)
{
    QList<OwnedGame> games;
    static const QRegularExpression gameRe(
        QStringLiteral("<game>(.*?)</game>"), kDotAll);
    static const QRegularExpression appRe(
        QStringLiteral("<appID>(\\d+)</appID>"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression nameRe(QStringLiteral("<name>(.*?)</name>"), kDotAll);

    auto it = gameRe.globalMatch(xml);
    while (it.hasNext()) {
        const QString block = it.next().captured(1);
        OwnedGame game;
        game.appId = firstGroup(block, appRe);
        if (game.appId.isEmpty())
            continue;
        game.name = decodeHtml(stripTags(firstGroup(block, nameRe))).trimmed();
        if (game.name.isEmpty())
            game.name = QStringLiteral("App ID: %1").arg(game.appId);
        games.append(game);
    }
    return games;
}

} // namespace SteamHtml
