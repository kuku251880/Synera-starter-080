#include "unit.h"
#include "core/game.h"

// 静态变量初始化：棋子身份证号从 0 开始自增
int Unit::s_nextId = 0;

// 基类构造函数：把棋子的名字、血量、蓝量等所有初始属性全部填好
Unit::Unit(const QString& name, int hp, int atk, int range, int maxMana, UnitOwner owner, const QStringList& traits,
           SkillType skillType)
    : m_id(s_nextId++), m_name(name), m_position(-1, -1), m_hp(hp), m_maxHp(hp), m_atk(atk), m_range(range),
      m_maxMana(maxMana), m_mana(0), m_owner(owner), m_traits(traits), m_state(UnitState::Idle), m_skillType(skillType),
      m_skill(createSkill(skillType)), m_attackCooldown(0), m_moveCooldown(0), m_attackInterval(GameConstants::kDefaultAttackInterval), m_starLevel(1),
      m_traitMaxHpBonus(0), m_traitAtkBonus(0), m_traitRangeBonus(0), m_traitMaxManaBonus(0), m_traitManaGainBonus(0),
      m_traitSkillAmpPercent(0), m_traitExtraStrikeChance(0)
{
}

// 修改技能：改变棋子的技能类型，并重新创建对应的技能执行器
void Unit::setSkillType(SkillType skillType)
{
    m_skillType = skillType;
    m_skill = createSkill(skillType);
}

// 注入羁绊加成：把算出来的羁绊数值存进去，并用 qMin 确保当前血量/蓝量不超过上限
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

// 清空羁绊加成：一键把所有临时羁绊加成全部变回 0
void Unit::clearTraitBonuses()
{
    setTraitBonuses(0, 0, 0, 0, 0, 0, 0);
}

// 重置战斗状态：开战前血量回满、蓝量清空、状态变回待机、冷却清零
void Unit::resetCombatState()
{
    m_hp = maxHp();
    m_mana = 0;
    m_state = UnitState::Idle;
    m_attackCooldown = 0;
    m_moveCooldown = 0;
}

// ─── 子类构造函数：给不同职业的棋子填入各自的“出厂属性” ───────────────────────

// 我方战士：120血，近战，带有前排/人类标签，技能是强力一击
WarriorUnit::WarriorUnit(UnitOwner owner)
    : Unit(QStringLiteral("战士"), 120, 14, 1, 80, owner,
           {QStringLiteral("前排"), QStringLiteral("人类")}, SkillType::PowerStrike)
{
}

// 我方弓手：80血，3格射程，带有游侠/人类标签，技能是强力一击
ArcherUnit::ArcherUnit(UnitOwner owner)
    : Unit(QStringLiteral("弓手"), 80, 18, 3, 60, owner,
           {QStringLiteral("游侠"), QStringLiteral("人类")}, SkillType::PowerStrike)
{
}

// 我方法师：70血高攻击，3格射程，带有奥术/人类标签，技能是奥术爆裂
MageUnit::MageUnit(UnitOwner owner)
    : Unit(QStringLiteral("法师"), 70, 22, 3, 100, owner,
           {QStringLiteral("奥术"), QStringLiteral("人类")}, SkillType::ArcaneBurst)
{
}

// 我方预备兵：95血，带有前排/游侠标签，技能是自我治疗
ReserveUnit::ReserveUnit(UnitOwner owner)
    : Unit(QStringLiteral("预备兵"), 95, 12, 1, 70, owner,
           {QStringLiteral("前排"), QStringLiteral("游侠")}, SkillType::SelfHeal)
{
}

// 我方守卫：140高血量，带有前排/奥术标签，技能是自我治疗
GuardUnit::GuardUnit(UnitOwner owner)
    : Unit(QStringLiteral("守卫"), 140, 10, 1, 90, owner,
           {QStringLiteral("前排"), QStringLiteral("奥术")}, SkillType::SelfHeal)
{
}

// 敌方战士：固定属于敌方阵营（EnemyCtrl），带有前排/敌人标签
EnemyWarriorUnit::EnemyWarriorUnit()
    : Unit(QStringLiteral("敌方战士"), 120, 14, 1, 80, UnitOwner::EnemyCtrl,
           {QStringLiteral("前排"), QStringLiteral("敌人")}, SkillType::PowerStrike)
{
}

// 敌方弓手：固定属于敌方阵营（EnemyCtrl），血量略高，技能是奥术爆裂
EnemyArcherUnit::EnemyArcherUnit()
    : Unit(QStringLiteral("敌方弓手"), 85, 18, 3, 60, UnitOwner::EnemyCtrl,
           {QStringLiteral("游侠"), QStringLiteral("敌人")}, SkillType::ArcaneBurst)
{
}

// ─── 工厂生产模式 ─────────────────────────────────────────────────────

// 棋子制造工厂：根据传入的名字字符串，自动 new 出对应的棋子对象并返回
Unit* Unit::create(const QString& typeName, UnitOwner owner)
{
    // 判断是不是玩家阵营的棋子，是就创建对应的我方棋子
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

    // 判断是不是敌方阵营的棋子，是就创建对应的敌方棋子
    if (typeName == QStringLiteral("敌方战士") || typeName == QStringLiteral("Enemy Warrior")) {
        return new EnemyWarriorUnit();
    }
    if (typeName == QStringLiteral("敌方弓手") || typeName == QStringLiteral("Enemy Archer")) {
        return new EnemyArcherUnit();
    }

    // 兜底机制：如果名字输错了，默认生成一个普通的 100 血白板棋子
    return new Unit(typeName, 100, 10, 1, 100, owner, {QStringLiteral("普通")}, SkillType::PowerStrike);
}