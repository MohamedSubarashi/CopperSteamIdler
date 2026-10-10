#include "Statistics.h"

#include "Settings.h"

#include <QObject>

void Statistics::resetSession()
{
    m_sessionMinutesIdled = 0;
    m_sessionCardsIdled = 0;
    m_remainingCards = 0;
}

void Statistics::setRemainingCards(int value)
{
    m_remainingCards = value;
}

void Statistics::checkCardRemaining(int actual)
{
    if (actual < m_remainingCards)
        increaseCardIdled(static_cast<uint>(m_remainingCards - actual));
    m_remainingCards = actual;
}

void Statistics::increaseMinutesIdled()
{
    ++m_sessionMinutesIdled;
    Settings::instance().setTotalMinutesIdled(Settings::instance().totalMinutesIdled() + 1);
    Settings::instance().save();
}

void Statistics::increaseCardIdled(uint count)
{
    if (count == 0)
        return;
    m_sessionCardsIdled += count;
    Settings::instance().setTotalCardIdled(Settings::instance().totalCardIdled() + count);
    Settings::instance().save();
}

QString Statistics::sessionTimeText() const
{
    const uint hours = m_sessionMinutesIdled / 60;
    const uint minutes = m_sessionMinutesIdled % 60;
    return QObject::tr("%1 hours, %2 minutes idled").arg(hours).arg(minutes);
}

QString Statistics::sessionCardsText() const
{
    return QObject::tr("%1 cards idled").arg(m_sessionCardsIdled);
}
