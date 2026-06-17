#ifndef UNIT_H
#define UNIT_H

#include <QPoint>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <memory>
#include "skill.h"

//创建敌我归属
enum class UnitOwner
{
    PlayerCtrl,
    EnemyCtrl
};

//创建单位状态机
enum class UnitState
{
    Idle,//待机
    Moving,//移动
    Attacking,//攻击
    Casting,//施法
    Dead//死亡
};

class Unit
{
protected:
    static int s_nextId;

    //基础身份与空间属性
    int m_id;
    QString m_name;
    QPoint m_position;
    UnitOwner m_owner;

    //核心战斗三维
    int m_hp;
    int m_maxHp;
    int m_atk;
    int m_range;

    //法力值与技能
    int m_maxMana;
    int m_mana;
    SkillType m_skillType;
    std::unique_ptr<Skill> m_skill;

    //状态与冷却（计时器）
    UnitState m_state;
    int m_attackCooldown;
    int m_moveCooldown;
    int m_attackInterval;

    //养成与羁绊加成属性
    int m_starLevel;
    QStringList m_traits;// 棋子自带的标签（如：{"前排", "人类"}）
    int m_traitMaxHpBonus; // 羁绊带来的额外生命加成
    int m_traitAtkBonus; // 羁绊带来的额外攻击加成
    int m_traitRangeBonus; // 羁绊带来的额外射程加成
    int m_traitMaxManaBonus; // 羁绊带来的额外最大法力值加成
    int m_traitManaGainBonus;// 羁绊带来的额外每次普攻回蓝加成
    int m_traitSkillAmpPercent;// 奥术羁绊：技能伤害百分比强化
    int m_traitExtraStrikeChance;// 游侠羁绊：普攻触发连击的概率
    QStringList m_equipmentNames;// 装备

public:
    // 构造函数：初始化棋子核心属性
    Unit(const QString& name, int hp, int atk, int range, int maxMana,
         UnitOwner owner, const QStringList& traits, SkillType skillType);

    // 析构函数：安全释放内存
    virtual ~Unit() = default;

           // 工厂模式：通过名字（如"战士"）直接创建对应棋子对象
    static Unit* create(const QString& typeName, UnitOwner owner);

           // 虚函数：返回棋子的真实职业名称
    virtual QString typeName() const { return m_name; }

           // ─── 战斗实时属性（基础值 + 羁绊加成） ─────────────────────────
    int id() const { return m_id; }                                     // 拿棋子的唯一身份证号
    QString name() const { return m_name; }                             // 拿棋子名字
    QPoint position() const { return m_position; }                       // 拿棋子当前坐标
    int hp() const { return m_hp; }                                     // 拿当前血量
    int maxHp() const { return m_maxHp + m_traitMaxHpBonus; }           // 算上羁绊的总动态最大血量
    int atk() const { return m_atk + m_traitAtkBonus; }                 // 算上羁绊的总动态攻击力
    int range() const { return m_range + m_traitRangeBonus; }           // 算上羁绊的总动态射程
    int maxMana() const { return qMax(20, m_maxMana + m_traitMaxManaBonus); } // 算上羁绊的总动态最大蓝量

           // ─── 干净的基础属性（不受羁绊干扰，用于存档或升星） ──────────────────
    int baseMaxHp() const { return m_maxHp; }                           // 裸体最大血量
    int baseAtk() const { return m_atk; }                               // 裸体攻击力
    int baseRange() const { return m_range; }                           // 裸体射程
    int baseMaxMana() const { return m_maxMana; }                       // 裸体最大蓝量

    int mana() const { return m_mana; }                                 // 当前蓝量
    UnitOwner owner() const { return m_owner; }                         // 是谁的棋子（敌/友）
    QStringList traits() const { return m_traits; }                     // 棋子的羁绊标签列表
    UnitState state() const { return m_state; }                         // 棋子现在的动作状态
    SkillType skillType() const { return m_skillType; }                 // 技能类型
    const Skill* skill() const { return m_skill.get(); }                // 技能逻辑指针
    int attackCooldown() const { return m_attackCooldown; }             // 普攻还要等多少帧
    int moveCooldown() const { return m_moveCooldown; }                 // 移动还要等多少帧
    int attackInterval() const { return m_attackInterval; }             // 固定攻击间隔（控制攻速）
    int starLevel() const { return m_starLevel; }                       // 当前星级
    QStringList equipmentNames() const { return m_equipmentNames; }     // 已穿装备的名字列表
    int traitManaGainBonus() const { return m_traitManaGainBonus; }     // 羁绊额外加的回蓝速度
    int traitSkillAmpPercent() const { return m_traitSkillAmpPercent; } // 奥术羁绊大招伤害加成百分比
    int traitExtraStrikeChance() const { return m_traitExtraStrikeChance; } // 游侠羁绊连击概率
    bool isAlive() const { return m_hp > 0; }                           // 是否活着

           // ─── 属性修改器（修改棋子内部变量） ─────────────────────────────
    void setName(const QString& name) { m_name = name; }                // 改名字
    void setPosition(const QPoint& pos) { m_position = pos; }           // 改格子坐标
    void setHp(int hp) { m_hp = hp; }                                   // 改当前血量（掉血/回血）
    void setMaxHp(int maxHp) { m_maxHp = maxHp; }                       // 改基础最大血量（升星/穿装备）
    void setAtk(int atk) { m_atk = atk; }                               // 改基础攻击力（升星/穿装备）
    void setRange(int range) { m_range = range; }                       // 改基础射程
    void setMaxMana(int maxMana) { m_maxMana = maxMana; }               // 改基础最大蓝量上限
    void setMana(int mana) { m_mana = mana; }                           // 改当前蓝量
    void setOwner(UnitOwner owner) { m_owner = owner; }                 // 改敌我归属
    void setTraits(const QStringList& traits) { m_traits = traits; }     // 改羁绊标签
    void setState(UnitState state) { m_state = state; }                 // 改动作状态（变走路、攻击等）
    void setSkillType(SkillType skillType);                             // 换技能
    void setAttackCooldown(int cooldown) { m_attackCooldown = cooldown; } // 设置普攻冷却
    void setMoveCooldown(int cooldown) { m_moveCooldown = cooldown; }   // 设置移动冷却
    void setAttackInterval(int interval) { m_attackInterval = interval; } // 改基础攻速
    void setStarLevel(int starLevel) { m_starLevel = starLevel; }       // 改星级
    void addEquipmentName(const QString& name) { m_equipmentNames.append(name); } // 穿上一件新装备

    // ─── 局内控制函数 ─────────────────────────────────────────────
    void setTraitBonuses(int maxHpBonus, int atkBonus, int rangeBonus, int maxManaBonus, int manaGainBonus,
                         int skillAmpPercent, int extraStrikeChance);   // 注入本轮激发的羁绊加成
    void clearTraitBonuses();                                           // 清空所有临时羁绊加成
    void resetCombatState();                                            // 战前准备：回满血、空蓝、重置状态
};

// ─── 具体的棋子派生子类 ───────────────────────────────────────────
class WarriorUnit : public Unit {
public:
    explicit WarriorUnit(UnitOwner owner = UnitOwner::PlayerCtrl);       // 战士（默认我方）
    QString typeName() const override { return QStringLiteral("战士"); }
};

class ArcherUnit : public Unit {
public:
    explicit ArcherUnit(UnitOwner owner = UnitOwner::PlayerCtrl);       // 弓手（默认我方）
    QString typeName() const override { return QStringLiteral("弓手"); }
};

class MageUnit : public Unit {
public:
    explicit MageUnit(UnitOwner owner = UnitOwner::PlayerCtrl);       // 法师（默认我方）
    QString typeName() const override { return QStringLiteral("法师"); }
};

class ReserveUnit : public Unit {
public:
    explicit ReserveUnit(UnitOwner owner = UnitOwner::PlayerCtrl);       // 预备兵（默认我方）
    QString typeName() const override { return QStringLiteral("预备兵"); }
};

class GuardUnit : public Unit {
public:
    explicit GuardUnit(UnitOwner owner = UnitOwner::PlayerCtrl);       // 守卫（默认我方）
    QString typeName() const override { return QStringLiteral("守卫"); }
};

class EnemyWarriorUnit : public Unit {
public:
    explicit EnemyWarriorUnit();                                       // 敌方战士
    QString typeName() const override { return QStringLiteral("敌方战士"); }
};

class EnemyArcherUnit : public Unit {
public:
    explicit EnemyArcherUnit();                                        // 敌方弓手
    QString typeName() const override { return QStringLiteral("敌方弓手"); }
};

#endif // UNIT_H