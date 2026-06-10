#include "skill.h"
#include "unit.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtGlobal>

namespace {
struct SkillConfig
{
    SkillType type;
    QString name;
    int attackMultiplier;
    int flatDamage;
    int heal;
    int radius;
};

int amplifiedDamage(Unit* caster, int baseDamage)
{
    if (!caster) {
        return baseDamage;
    }
    return baseDamage * (100 + caster->traitSkillAmpPercent()) / 100;
}

QString skillTypeKey(SkillType type)
{
    switch (type) {
    case SkillType::PowerStrike:
        return QStringLiteral("PowerStrike");
    case SkillType::SelfHeal:
        return QStringLiteral("SelfHeal");
    case SkillType::ArcaneBurst:
        return QStringLiteral("ArcaneBurst");
    }
    return QStringLiteral("PowerStrike");
}

bool parseSkillType(const QString& key, SkillType* type)
{
    if (key == QStringLiteral("PowerStrike")) {
        *type = SkillType::PowerStrike;
        return true;
    }
    if (key == QStringLiteral("SelfHeal")) {
        *type = SkillType::SelfHeal;
        return true;
    }
    if (key == QStringLiteral("ArcaneBurst")) {
        *type = SkillType::ArcaneBurst;
        return true;
    }
    return false;
}

QVector<SkillConfig> defaultSkillConfigs()
{
    return {
        {SkillType::PowerStrike, QStringLiteral("强力一击"), 2, 20, 0, 0},
        {SkillType::SelfHeal, QStringLiteral("自我治疗"), 0, 0, 45, 0},
        {SkillType::ArcaneBurst, QStringLiteral("奥术爆裂"), 1, 20, 0, 1},
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

QVector<SkillConfig> loadSkillConfigs()
{
    QVector<SkillConfig> configs = defaultSkillConfigs();
    const QString path = projectRelativeFilePath(QStringLiteral("data/skills.json"));
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

    const QJsonArray skills = document.object().value(QStringLiteral("skills")).toArray();
    for (const QJsonValue& value : skills) {
        const QJsonObject object = value.toObject();
        SkillType type = SkillType::PowerStrike;
        if (!parseSkillType(object.value(QStringLiteral("type")).toString(), &type)) {
            continue;
        }

        SkillConfig config;
        config.type = type;
        config.name = object.value(QStringLiteral("name")).toString(skillTypeKey(type));
        config.attackMultiplier = object.value(QStringLiteral("attackMultiplier")).toInt(0);
        config.flatDamage = object.value(QStringLiteral("flatDamage")).toInt(0);
        config.heal = object.value(QStringLiteral("heal")).toInt(0);
        config.radius = object.value(QStringLiteral("radius")).toInt(0);

        for (SkillConfig& existing : configs) {
            if (existing.type == type) {
                existing = config;
                break;
            }
        }
    }

    return configs;
}

const SkillConfig& skillConfig(SkillType type)
{
    static const QVector<SkillConfig> configs = loadSkillConfigs();
    for (const SkillConfig& config : configs) {
        if (config.type == type) {
            return config;
        }
    }
    return configs.first();
}
} // namespace

QString PowerStrikeSkill::name() const
{
    return skillConfig(type()).name;
}

void PowerStrikeSkill::cast(Unit* caster, Unit* target, const QList<Unit*>&,
                            const std::function<void(Unit*, int)>& applyDamage,
                            const std::function<int(const QPoint&, const QPoint&)>&,
                            const std::function<void(const QString&)>& addLog) const
{
    if (!caster || !target) {
        return;
    }

    const SkillConfig& config = skillConfig(type());
    applyDamage(target, amplifiedDamage(caster, caster->atk() * config.attackMultiplier + config.flatDamage));
    addLog(QStringLiteral("%1释放%2。").arg(caster->name(), config.name));
}

QString SelfHealSkill::name() const
{
    return skillConfig(type()).name;
}

void SelfHealSkill::cast(Unit* caster, Unit*, const QList<Unit*>&, const std::function<void(Unit*, int)>&,
                         const std::function<int(const QPoint&, const QPoint&)>&,
                         const std::function<void(const QString&)>& addLog) const
{
    if (!caster) {
        return;
    }

    const SkillConfig& config = skillConfig(type());
    caster->setHp(qMin(caster->maxHp(), caster->hp() + config.heal));
    addLog(QStringLiteral("%1释放%2。").arg(caster->name(), config.name));
}

QString ArcaneBurstSkill::name() const
{
    return skillConfig(type()).name;
}

void ArcaneBurstSkill::cast(Unit* caster, Unit* target, const QList<Unit*>& units,
                            const std::function<void(Unit*, int)>& applyDamage,
                            const std::function<int(const QPoint&, const QPoint&)>& gridDistance,
                            const std::function<void(const QString&)>& addLog) const
{
    if (!caster || !target) {
        return;
    }

    const SkillConfig& config = skillConfig(type());
    const QPoint center = target->position();
    for (Unit* candidate : units) {
        if (!candidate || !candidate->isAlive() || candidate->owner() == caster->owner()) {
            continue;
        }

        if (gridDistance(center, candidate->position()) <= config.radius) {
            applyDamage(candidate,
                        amplifiedDamage(caster, caster->atk() * config.attackMultiplier + config.flatDamage));
        }
    }
    addLog(QStringLiteral("%1释放%2。").arg(caster->name(), config.name));
}

std::unique_ptr<Skill> createSkill(SkillType type)
{
    switch (type) {
    case SkillType::PowerStrike:
        return std::make_unique<PowerStrikeSkill>();
    case SkillType::SelfHeal:
        return std::make_unique<SelfHealSkill>();
    case SkillType::ArcaneBurst:
        return std::make_unique<ArcaneBurstSkill>();
    }

    return std::make_unique<PowerStrikeSkill>();
}
