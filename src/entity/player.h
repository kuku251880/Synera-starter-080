#ifndef PLAYER_H
#define PLAYER_H

#include <QSet>

class Player
{
public:
    Player();
    ~Player() = default;

    int hp() const { return m_hp; }
    int gold() const { return m_gold; }
    int level() const { return m_level; }
    int populationLimit() const { return m_populationLimit; }
    int currentRound() const { return m_currentRound; }
    QSet<int> ownedUnitIds() const { return m_ownedUnitIds; }

    void setHp(int hp) { m_hp = hp; }
    void setGold(int gold) { m_gold = gold; }
    void setLevel(int level) { m_level = level; }
    void setPopulationLimit(int populationLimit) { m_populationLimit = populationLimit; }
    void setCurrentRound(int currentRound) { m_currentRound = currentRound; }

    void addUnit(int unitId);
    void removeUnit(int unitId);
    bool ownsUnit(int unitId) const;

private:
    int m_hp;
    int m_gold;
    int m_level;
    int m_populationLimit;
    int m_currentRound;
    QSet<int> m_ownedUnitIds;
};

#endif // PLAYER_H
