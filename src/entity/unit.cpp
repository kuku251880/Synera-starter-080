#include "unit.h"
#include "core/game.h"

int Unit::s_nextId = 0;

Unit::Unit(const QString& name, int hp, int atk, int range, int maxMana, UnitOwner owner, const QStringList& traits,
           SkillType skillType)
    : m_id(s_nextId++), m_name(name), m_position(-1, -1), m_hp(hp), m_maxHp(hp), m_atk(atk), m_range(range),
      m_maxMana(maxMana), m_mana(0), m_owner(owner), m_traits(traits), m_state(UnitState::Idle), m_skillType(skillType),
      m_skill(createSkill(skillType)), m_attackCooldown(0), m_moveCooldown(0), m_attackInterval(GameConstants::kDefaultAttackInterval), m_starLevel(1),
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

// ─── Subclass constructors ───────────────────────────────────────

WarriorUnit::WarriorUnit(UnitOwner owner)
    : Unit(QStringLiteral("战士"), 120, 14, 1, 80, owner,
           {QStringLiteral("前排"), QStringLiteral("人类")}, SkillType::PowerStrike)
{
}

ArcherUnit::ArcherUnit(UnitOwner owner)
    : Unit(QStringLiteral("弓手"), 80, 18, 3, 60, owner,
           {QStringLiteral("游侠"), QStringLiteral("人类")}, SkillType::PowerStrike)
{
}

MageUnit::MageUnit(UnitOwner owner)
    : Unit(QStringLiteral("法师"), 70, 22, 3, 100, owner,
           {QStringLiteral("奥术"), QStringLiteral("人类")}, SkillType::ArcaneBurst)
{
}

ReserveUnit::ReserveUnit(UnitOwner owner)
    : Unit(QStringLiteral("预备兵"), 95, 12, 1, 70, owner,
           {QStringLiteral("前排"), QStringLiteral("游侠")}, SkillType::SelfHeal)
{
}

GuardUnit::GuardUnit(UnitOwner owner)
    : Unit(QStringLiteral("守卫"), 140, 10, 1, 90, owner,
           {QStringLiteral("前排"), QStringLiteral("奥术")}, SkillType::SelfHeal)
{
}

EnemyWarriorUnit::EnemyWarriorUnit()
    : WarriorUnit(UnitOwner::EnemyCtrl)
{
    setName(QStringLiteral("敌方战士"));
    setTraits({QStringLiteral("前排"), QStringLiteral("敌人")});
}

EnemyArcherUnit::EnemyArcherUnit()
    : Unit(QStringLiteral("敌方弓手"), 85, 18, 3, 60, UnitOwner::EnemyCtrl,
           {QStringLiteral("游侠"), QStringLiteral("敌人")}, SkillType::ArcaneBurst)
{
}

// ─── Factory ─────────────────────────────────────────────────────

Unit* Unit::create(const QString& typeName, UnitOwner owner)
{
    // Player units
    if (typeName == QStringLiteral("战士") || typeName == QStringLiteral("Warrior")) {
        return new WarriorUnit(owner);
    }
    if (typeName == QStringLiteral("弓手") || typeName == QStringLiteral("Archer")) {
        return new ArcherUnit(owner);
    }
    if (typeName == QStringLiteral("法师") || typeName == QStringLiteral("Mage")) {
        return new MageUnit(owner);
    }
    if (typeName == QStringLiteral("预备兵") || typeName == QStringLiteral("Reserve")) {
        return new ReserveUnit(owner);
    }
    if (typeName == QStringLiteral("守卫") || typeName == QStringLiteral("Guard")) {
        return new GuardUnit(owner);
    }
    // Enemy units
    if (typeName == QStringLiteral("敌方战士") || typeName == QStringLiteral("Enemy Warrior")) {
        return new EnemyWarriorUnit();
    }
    if (typeName == QStringLiteral("敌方弓手") || typeName == QStringLiteral("Enemy Archer")) {
        return new EnemyArcherUnit();
    }
    // Fallback
    return new Unit(typeName, 100, 10, 1, 100, owner, {QStringLiteral("普通")}, SkillType::PowerStrike);
}
