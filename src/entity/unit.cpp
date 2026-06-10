#include "unit.h"

int Unit::s_nextId = 0;

Unit::Unit(const QString& name, int hp, int atk, int range, int maxMana, UnitOwner owner, const QStringList& traits,
           SkillType skillType)
    : m_id(s_nextId++), m_name(name), m_position(-1, -1), m_hp(hp), m_maxHp(hp), m_atk(atk), m_range(range),
      m_maxMana(maxMana), m_mana(0), m_owner(owner), m_traits(traits), m_state(UnitState::Idle), m_skillType(skillType),
      m_skill(createSkill(skillType)), m_attackCooldown(0), m_moveCooldown(0), m_attackInterval(8), m_starLevel(1),
      m_traitMaxHpBonus(0), m_traitAtkBonus(0), m_traitRangeBonus(0), m_traitMaxManaBonus(0), m_traitManaGainBonus(0),
      m_traitSkillAmpPercent(0), m_traitExtraStrikeChance(0)
{
}

void Unit::setSkillType(SkillType skillType)
{
    m_skillType = skillType;
    m_skill = createSkill(skillType);
}

void Unit::setTraitBonuses(int maxHpBonus, int atkBonus, int rangeBonus, int maxManaBonus, int manaGainBonus,
                           int skillAmpPercent, int extraStrikeChance)
{
    m_traitMaxHpBonus = maxHpBonus;
    m_traitAtkBonus = atkBonus;
    m_traitRangeBonus = rangeBonus;
    m_traitMaxManaBonus = maxManaBonus;
    m_traitManaGainBonus = manaGainBonus;
    m_traitSkillAmpPercent = skillAmpPercent;
    m_traitExtraStrikeChance = extraStrikeChance;
    m_hp = qMin(m_hp, maxHp());
    m_mana = qMin(m_mana, maxMana());
}

void Unit::clearTraitBonuses()
{
    setTraitBonuses(0, 0, 0, 0, 0, 0, 0);
}

void Unit::resetCombatState()
{
    m_hp = maxHp();
    m_mana = 0;
    m_state = UnitState::Idle;
    m_attackCooldown = 0;
    m_moveCooldown = 0;
}
