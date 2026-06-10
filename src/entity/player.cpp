#include "player.h"

Player::Player()
    : m_hp(100)
    , m_gold(10)
    , m_level(1)
    , m_populationLimit(4)
    , m_currentRound(1)
    , m_winStreak(0)
    , m_lossStreak(0)
{}

void Player::addUnit(int unitId)
{
    m_ownedUnitIds.insert(unitId);
}

void Player::removeUnit(int unitId)
{
    m_ownedUnitIds.remove(unitId);
}

void Player::clearUnits()
{
    m_ownedUnitIds.clear();
}

bool Player::ownsUnit(int unitId) const
{
    return m_ownedUnitIds.contains(unitId);
}
