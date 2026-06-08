#ifndef EQUIPMENT_H
#define EQUIPMENT_H

#include <QString>

class Unit;

enum class EquipmentType
{
    TrainingSword,
    VitalityArmor,
    SwiftCharm,
    ManaAmulet
};

class Equipment
{
public:
    explicit Equipment(EquipmentType type = EquipmentType::TrainingSword);

    EquipmentType type() const { return m_type; }
    QString name() const;
    QString description() const;
    void applyTo(Unit* unit) const;

private:
    EquipmentType m_type;
};

#endif // EQUIPMENT_H
