#include "equipment.h"
#include "unit.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtGlobal>

namespace {
struct EquipmentConfig
{
    EquipmentType type;
    QString name;
    QStringList aliases;
    QString description;
    QString rarity;
    QString rarityColor;
    int atkBonus;
    int maxHpBonus;
    int hpBonus;
    int attackIntervalBonus;
    int maxManaBonus;
};

QString equipmentTypeKey(EquipmentType type)
{
    switch (type) {
    case EquipmentType::TrainingSword:
        return QStringLiteral("TrainingSword");
    case EquipmentType::VitalityArmor:
        return QStringLiteral("VitalityArmor");
    case EquipmentType::SwiftCharm:
        return QStringLiteral("SwiftCharm");
    case EquipmentType::ManaAmulet:
        return QStringLiteral("ManaAmulet");
    }
    return QStringLiteral("TrainingSword");
}

bool parseEquipmentType(const QString& key, EquipmentType* type)
{
    if (key == QStringLiteral("TrainingSword")) {
        *type = EquipmentType::TrainingSword;
        return true;
    }
    if (key == QStringLiteral("VitalityArmor")) {
        *type = EquipmentType::VitalityArmor;
        return true;
    }
    if (key == QStringLiteral("SwiftCharm")) {
        *type = EquipmentType::SwiftCharm;
        return true;
    }
    if (key == QStringLiteral("ManaAmulet")) {
        *type = EquipmentType::ManaAmulet;
        return true;
    }
    return false;
}

QStringList jsonArrayToStringList(const QJsonArray& array)
{
    QStringList values;
    for (const QJsonValue& value : array) {
        const QString text = value.toString();
        if (!text.isEmpty()) {
            values.append(text);
        }
    }
    return values;
}

QVector<EquipmentConfig> defaultEquipmentConfigs()
{
    return {
        {EquipmentType::TrainingSword,
         QStringLiteral("训练剑"),
         {QStringLiteral("Training Sword")},
         QStringLiteral("攻击 +15"),
         QStringLiteral("普通"),
         QStringLiteral("#aeb5c1"),
         15,
         0,
         0,
         0,
         0},
        {EquipmentType::VitalityArmor,
         QStringLiteral("活力甲"),
         {QStringLiteral("Vitality Armor")},
         QStringLiteral("生命 +150"),
         QStringLiteral("稀有"),
         QStringLiteral("#64d2ff"),
         0,
         150,
         150,
         0,
         0},
        {EquipmentType::SwiftCharm,
         QStringLiteral("迅捷符"),
         {QStringLiteral("Swift Charm")},
         QStringLiteral("攻击间隔 -1"),
         QStringLiteral("稀有"),
         QStringLiteral("#64d2ff"),
         0,
         0,
         0,
         -1,
         0},
        {EquipmentType::ManaAmulet,
         QStringLiteral("法力护符"),
         {QStringLiteral("Mana Amulet")},
         QStringLiteral("最大法力 -30"),
         QStringLiteral("精良"),
         QStringLiteral("#a78bfa"),
         0,
         0,
         0,
         0,
         -30},
    };
}

QString projectRelativeFilePath(const QString& relativePath)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString roots[] = {
        appDir,
        QFileInfo(appDir + QStringLiteral("/..")).canonicalFilePath(),
        QFileInfo(appDir + QStringLiteral("/../..")).canonicalFilePath(),
    };

    for (const QString& root : roots) {
        if (root.isEmpty()) {
            continue;
        }

        const QFileInfo candidate(root + QStringLiteral("/") + relativePath);
        if (candidate.exists()) {
            return candidate.canonicalFilePath();
        }
    }
    return QString();
}

QVector<EquipmentConfig> loadEquipmentConfigs()
{
    QVector<EquipmentConfig> configs = defaultEquipmentConfigs();
    const QString path = projectRelativeFilePath(QStringLiteral("data/equipment.json"));
    if (path.isEmpty()) {
        return configs;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return configs;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return configs;
    }

    const QJsonArray equipmentArray = document.object().value(QStringLiteral("equipment")).toArray();
    for (const QJsonValue& value : equipmentArray) {
        const QJsonObject object = value.toObject();
        EquipmentType type = EquipmentType::TrainingSword;
        if (!parseEquipmentType(object.value(QStringLiteral("type")).toString(), &type)) {
            continue;
        }

        EquipmentConfig config;
        config.type = type;
        config.name = object.value(QStringLiteral("name")).toString(equipmentTypeKey(type));
        config.aliases = jsonArrayToStringList(object.value(QStringLiteral("aliases")).toArray());
        config.description = object.value(QStringLiteral("description")).toString();
        config.rarity = object.value(QStringLiteral("rarity")).toString(QStringLiteral("普通"));
        config.rarityColor = object.value(QStringLiteral("rarityColor")).toString(QStringLiteral("#aeb5c1"));
        config.atkBonus = object.value(QStringLiteral("atkBonus")).toInt(0);
        config.maxHpBonus = object.value(QStringLiteral("maxHpBonus")).toInt(0);
        config.hpBonus = object.value(QStringLiteral("hpBonus")).toInt(0);
        config.attackIntervalBonus = object.value(QStringLiteral("attackIntervalBonus")).toInt(0);
        config.maxManaBonus = object.value(QStringLiteral("maxManaBonus")).toInt(0);

        for (EquipmentConfig& existing : configs) {
            if (existing.type == type) {
                existing = config;
                break;
            }
        }
    }

    return configs;
}

const EquipmentConfig& equipmentConfig(EquipmentType type)
{
    static const QVector<EquipmentConfig> configs = loadEquipmentConfigs();
    for (const EquipmentConfig& config : configs) {
        if (config.type == type) {
            return config;
        }
    }
    return configs.first();
}
} // namespace

Equipment::Equipment(EquipmentType type) : m_type(type) {}

Equipment Equipment::fromName(const QString& name)
{
    static const QVector<EquipmentConfig> configs = loadEquipmentConfigs();
    for (const EquipmentConfig& config : configs) {
        if (name == config.name || config.aliases.contains(name)) {
            return Equipment(config.type);
        }
    }
    return Equipment(EquipmentType::TrainingSword);
}

QString Equipment::name() const
{
    return equipmentConfig(m_type).name;
}

QString Equipment::description() const
{
    return equipmentConfig(m_type).description;
}

QString Equipment::rarity() const
{
    return equipmentConfig(m_type).rarity;
}

QString Equipment::rarityColor() const
{
    return equipmentConfig(m_type).rarityColor;
}

void Equipment::applyTo(Unit* unit) const
{
    if (!unit) {
        return;
    }

    const EquipmentConfig& config = equipmentConfig(m_type);
    if (config.atkBonus != 0) {
        unit->setAtk(unit->atk() + config.atkBonus);
    }
    if (config.maxHpBonus != 0) {
        unit->setMaxHp(unit->maxHp() + config.maxHpBonus);
    }
    if (config.hpBonus != 0) {
        unit->setHp(unit->hp() + config.hpBonus);
    }
    if (config.attackIntervalBonus != 0) {
        unit->setAttackInterval(qMax(2, unit->attackInterval() + config.attackIntervalBonus));
    }
    if (config.maxManaBonus != 0) {
        unit->setMaxMana(qMax(20, unit->maxMana() + config.maxManaBonus));
        unit->setMana(qMin(unit->mana(), unit->maxMana()));
    }

    unit->addEquipmentName(name());
}
