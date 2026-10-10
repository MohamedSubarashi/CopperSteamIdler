#pragma once

#include <QString>

// Session + lifetime idling statistics, persisted through Settings.
class Statistics
{
public:
    void resetSession();

    void setRemainingCards(int value);
    void checkCardRemaining(int actual);

    void increaseMinutesIdled();
    void increaseCardIdled(uint count);

    int remainingCards() const { return m_remainingCards; }
    uint sessionMinutesIdled() const { return m_sessionMinutesIdled; }
    uint sessionCardsIdled() const { return m_sessionCardsIdled; }

    QString sessionTimeText() const;
    QString sessionCardsText() const;

private:
    int m_remainingCards = 0;
    uint m_sessionMinutesIdled = 0;
    uint m_sessionCardsIdled = 0;
};
