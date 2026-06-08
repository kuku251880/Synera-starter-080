#ifndef UNIT_H
#define UNIT_H

#include <QPoint>
#include <QString>
#include <QStringList>

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

enum class SkillType
{
    PowerStrike,
    SelfHeal,
    ArcaneBurst
};

class Unit
{
public:
    Unit(const QString& name = QString("Unit"),
         int hp = 100,
         int atk = 10,
         int range = 1,
         int maxMana = 100,
         UnitOwner owner = UnitOwner::PlayerCtrl,
         const QStringList& traits = QStringList(),
         SkillType skillType = SkillType::PowerStrike);
    ~Unit() = default;

    int id() const { return m_id; }
    QString name() const { return m_name; }
    QPoint position() const { return m_position; }
    int hp() const { return m_hp; }
    int maxHp() const { return m_maxHp; }
    int atk() const { return m_atk; }
    int range() const { return m_range; }
    int maxMana() const { return m_maxMana; }
    int mana() const { return m_mana; }
    UnitOwner owner() const { return m_owner; }
    QStringList traits() const { return m_traits; }
    UnitState state() const { return m_state; }
    SkillType skillType() const { return m_skillType; }
    int attackCooldown() const { return m_attackCooldown; }
    int moveCooldown() const { return m_moveCooldown; }
    int attackInterval() const { return m_attackInterval; }
    int starLevel() const { return m_starLevel; }
    QStringList equipmentNames() const { return m_equipmentNames; }
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
    void setSkillType(SkillType skillType) { m_skillType = skillType; }
    void setAttackCooldown(int cooldown) { m_attackCooldown = cooldown; }
    void setMoveCooldown(int cooldown) { m_moveCooldown = cooldown; }
    void setAttackInterval(int interval) { m_attackInterval = interval; }
    void setStarLevel(int starLevel) { m_starLevel = starLevel; }
    void addEquipmentName(const QString& name) { m_equipmentNames.append(name); }
    void resetCombatState();

private:
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
    int m_attackCooldown;
    int m_moveCooldown;
    int m_attackInterval;
    int m_starLevel;
    QStringList m_equipmentNames;
};

#endif // UNIT_H
