#include "equipment.h"
#include "unit.h"
#include <QtGlobal>

Equipment::Equipment(EquipmentType type)
    : m_type(type)
{}

QString Equipment::name() const
{
    switch (m_type) {
    case EquipmentType::TrainingSword:
        return QStringLiteral("训练剑");
    case EquipmentType::VitalityArmor:
        return QStringLiteral("活力甲");
    case EquipmentType::SwiftCharm:
        return QStringLiteral("迅捷符");
    case EquipmentType::ManaAmulet:
        return QStringLiteral("法力护符");
    }
    return QStringLiteral("未知装备");
}

QString Equipment::description() const
{
    switch (m_type) {
    case EquipmentType::TrainingSword:
        return QStringLiteral("攻击 +15");
    case EquipmentType::VitalityArmor:
        return QStringLiteral("生命 +150");
    case EquipmentType::SwiftCharm:
        return QStringLiteral("攻击间隔 -1");
    case EquipmentType::ManaAmulet:
        return QStringLiteral("最大法力 -30");
    }
    return QString();
}

void Equipment::applyTo(Unit* unit) const
{
    if (!unit) {
        return;
    }

    switch (m_type) {
    case EquipmentType::TrainingSword:
        unit->setAtk(unit->atk() + 15);
        break;
    case EquipmentType::VitalityArmor:
        unit->setMaxHp(unit->maxHp() + 150);
        unit->setHp(unit->hp() + 150);
        break;
    case EquipmentType::SwiftCharm:
        unit->setAttackInterval(qMax(2, unit->attackInterval() - 1));
        break;
    case EquipmentType::ManaAmulet:
        unit->setMaxMana(qMax(20, unit->maxMana() - 30));
        unit->setMana(qMin(unit->mana(), unit->maxMana()));
        break;
    }

    unit->addEquipmentName(name());
}
