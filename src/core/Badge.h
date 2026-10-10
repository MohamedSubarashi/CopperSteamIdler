#pragma once

#include <QString>

// A single game in the badge list together with its remaining card drops.
class Badge
{
public:
    // Result of re-scraping a game's card page. A network error is distinct
    // from a genuine "no drops left" so the idle loop never falsely concludes.
    enum class DropStatus
    {
        DropsAvailable,
        NoDrops,
        Error
    };

    Badge() = default;
    explicit Badge(int appId, const QString& name = QString());

    int appId() const { return m_appId; }
    void setAppId(int value) { m_appId = value; }

    QString stringId() const;

    QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    int remainingCard() const { return m_remainingCard; }
    void setRemainingCard(int value) { m_remainingCard = value; }

    double hoursPlayed() const { return m_hoursPlayed; }
    void setHoursPlayed(double value) { m_hoursPlayed = value; }

    // Whitelist / whole-library entries are idled regardless of card drops.
    bool alwaysIdle() const { return m_alwaysIdle; }
    void setAlwaysIdle(bool value) { m_alwaysIdle = value; }

    bool inIdle() const;
    void idle();
    void stopIdle();

    // Re-scrapes the gamecards page. On Error the stored stats are unchanged.
    DropStatus refreshDrops(const QString& profileUrl);

    void updateStats(int remainingCards, double hoursPlayed);

    bool operator==(const Badge& other) const { return m_appId == other.m_appId; }
    bool operator!=(const Badge& other) const { return !(*this == other); }

private:
    int m_appId = 0;
    QString m_name;
    int m_remainingCard = 0;
    double m_hoursPlayed = 0.0;
    bool m_alwaysIdle = false;
};
