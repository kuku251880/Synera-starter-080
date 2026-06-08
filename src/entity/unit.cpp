#include "unit.h"

int Unit::s_nextId = 0;

Unit::Unit(const QString& name,
           int hp,
           int atk,
           int range,
           int maxMana,
           UnitOwner owner,
           const QStringList& traits)
    : m_id(s_nextId++)
    , m_name(name)
    , m_position(-1, -1)
    , m_hp(hp)
    , m_maxHp(hp)
    , m_atk(atk)
    , m_range(range)
    , m_maxMana(maxMana)
    , m_mana(0)
    , m_owner(owner)
    , m_traits(traits)
{}
