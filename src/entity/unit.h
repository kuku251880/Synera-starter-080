#ifndef UNIT_H
#define UNIT_H

#include <QPoint>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <memory>
#include "skill.h"

enum class UnitOwner
{
    PlayerCtrl,
    EnemyCtrl
};

enum class UnitState
{
    Idle,
    Moving,
    Attacking,
    Casting,
    Dead
};

class Unit
{
public:
    Unit(const QString& name, int hp, int atk, int range, int maxMana,
         UnitOwner owner, const QStringList& traits, SkillType skillType);
    virtual ~Unit() = default;

    // Factory: create the correct subclass from a unit-type name
    static Unit* create(const QString& typeName, UnitOwner owner);

    // Virtual identity — overridden by subclasses to report their canonical type
    virtual QString typeName() const { return m_name; }

    int id() const { return m_id; }
    QString name() const { return m_name; }
    QPoint position() const { return m_position; }
    int hp() const { return m_hp; }
    int maxHp() const { return m_maxHp + m_traitMaxHpBonus; }
    int atk() const { return m_atk + m_traitAtkBonus; }
    int range() const { return m_range + m_traitRangeBonus; }
    int maxMana() const { return qMax(20, m_maxMana + m_traitMaxManaBonus); }
    int baseMaxHp() const { return m_maxHp; }
    int baseAtk() const { return m_atk; }
    int baseRange() const { return m_range; }
    int baseMaxMana() const { return m_maxMana; }
    int mana() const { return m_mana; }
    UnitOwner owner() const { return m_owner; }
    QStringList traits() const { return m_traits; }
    UnitState state() const { return m_state; }
    SkillType skillType() const { return m_skillType; }
    const Skill* skill() const { return m_skill.get(); }
    int attackCooldown() const { return m_attackCooldown; }
    int moveCooldown() const { return m_moveCooldown; }
    int attackInterval() const { return m_attackInterval; }
    int starLevel() const { return m_starLevel; }
    QStringList equipmentNames() const { return m_equipmentNames; }
    int traitManaGainBonus() const { return m_traitManaGainBonus; }
    int traitSkillAmpPercent() const { return m_traitSkillAmpPercent; }
    int traitExtraStrikeChance() const { return m_traitExtraStrikeChance; }
    bool isAlive() const { return m_hp > 0; }

    void setName(const QString& name) { m_name = name; }
    void setPosition(const QPoint& pos) { m_position = pos; }
    void setHp(int hp) { m_hp = hp; }
    void setMaxHp(int maxHp) { m_maxHp = maxHp; }
    void setAtk(int atk) { m_atk = atk; }
    void setRange(int range) { m_range = range; }
    void setMaxMana(int maxMana) { m_maxMana = maxMana; }
    void setMana(int mana) { m_mana = mana; }
    void setOwner(UnitOwner owner) { m_owner = owner; }
    void setTraits(const QStringList& traits) { m_traits = traits; }
    void setState(UnitState state) { m_state = state; }
    void setSkillType(SkillType skillType);
    void setAttackCooldown(int cooldown) { m_attackCooldown = cooldown; }
    void setMoveCooldown(int cooldown) { m_moveCooldown = cooldown; }
    void setAttackInterval(int interval) { m_attackInterval = interval; }
    void setStarLevel(int starLevel) { m_starLevel = starLevel; }
    void addEquipmentName(const QString& name) { m_equipmentNames.append(name); }
    void setTraitBonuses(int maxHpBonus, int atkBonus, int rangeBonus, int maxManaBonus, int manaGainBonus,
                         int skillAmpPercent, int extraStrikeChance);
    void clearTraitBonuses();
    void resetCombatState();

protected:
    static int s_nextId;

    int m_id;
    QString m_name;
    QPoint m_position;
    int m_hp;
    int m_maxHp;
    int m_atk;
    int m_range;
    int m_maxMana;
    int m_mana;
    UnitOwner m_owner;
    QStringList m_traits;
    UnitState m_state;
    SkillType m_skillType;
    std::unique_ptr<Skill> m_skill;
    int m_attackCooldown;
    int m_moveCooldown;
    int m_attackInterval;
    int m_starLevel;
    QStringList m_equipmentNames;
    int m_traitMaxHpBonus;
    int m_traitAtkBonus;
    int m_traitRangeBonus;
    int m_traitMaxManaBonus;
    int m_traitManaGainBonus;
    int m_traitSkillAmpPercent;
    int m_traitExtraStrikeChance;
};

class WarriorUnit : public Unit
{
public:
    explicit WarriorUnit(UnitOwner owner = UnitOwner::PlayerCtrl);
    QString typeName() const override { return QStringLiteral("战士"); }
};

class ArcherUnit : public Unit
{
public:
    explicit ArcherUnit(UnitOwner owner = UnitOwner::PlayerCtrl);
    QString typeName() const override { return QStringLiteral("弓手"); }
};

class MageUnit : public Unit
{
public:
    explicit MageUnit(UnitOwner owner = UnitOwner::PlayerCtrl);
    QString typeName() const override { return QStringLiteral("法师"); }
};

class ReserveUnit : public Unit
{
public:
    explicit ReserveUnit(UnitOwner owner = UnitOwner::PlayerCtrl);
    QString typeName() const override { return QStringLiteral("预备兵"); }
};

class GuardUnit : public Unit
{
public:
    explicit GuardUnit(UnitOwner owner = UnitOwner::PlayerCtrl);
    QString typeName() const override { return QStringLiteral("守卫"); }
};

class EnemyWarriorUnit : public WarriorUnit
{
public:
    explicit EnemyWarriorUnit();
    QString typeName() const override { return QStringLiteral("敌方战士"); }
};

class EnemyArcherUnit : public Unit
{
public:
    explicit EnemyArcherUnit();
    QString typeName() const override { return QStringLiteral("敌方弓手"); }
};

#endif // UNIT_H
