#include "skill.h"
#include "unit.h"
#include <QtGlobal>

namespace {
int amplifiedDamage(Unit* caster, int baseDamage)
{
    if (!caster) {
        return baseDamage;
    }
    return baseDamage * (100 + caster->traitSkillAmpPercent()) / 100;
}
} // namespace

QString PowerStrikeSkill::name() const
{
    return QStringLiteral("强力一击");
}

void PowerStrikeSkill::cast(Unit* caster, Unit* target, const QList<Unit*>&,
                            const std::function<void(Unit*, int)>& applyDamage,
                            const std::function<int(const QPoint&, const QPoint&)>&,
                            const std::function<void(const QString&)>& addLog) const
{
    if (!caster || !target) {
        return;
    }

    applyDamage(target, amplifiedDamage(caster, caster->atk() * 2 + 20));
    addLog(QStringLiteral("%1释放强力一击。").arg(caster->name()));
}

QString SelfHealSkill::name() const
{
    return QStringLiteral("自我治疗");
}

void SelfHealSkill::cast(Unit* caster, Unit*, const QList<Unit*>&, const std::function<void(Unit*, int)>&,
                         const std::function<int(const QPoint&, const QPoint&)>&,
                         const std::function<void(const QString&)>& addLog) const
{
    if (!caster) {
        return;
    }

    caster->setHp(qMin(caster->maxHp(), caster->hp() + 45));
    addLog(QStringLiteral("%1释放自我治疗。").arg(caster->name()));
}

QString ArcaneBurstSkill::name() const
{
    return QStringLiteral("奥术爆裂");
}

void ArcaneBurstSkill::cast(Unit* caster, Unit* target, const QList<Unit*>& units,
                            const std::function<void(Unit*, int)>& applyDamage,
                            const std::function<int(const QPoint&, const QPoint&)>& gridDistance,
                            const std::function<void(const QString&)>& addLog) const
{
    if (!caster || !target) {
        return;
    }

    const QPoint center = target->position();
    for (Unit* candidate : units) {
        if (!candidate || !candidate->isAlive() || candidate->owner() == caster->owner()) {
            continue;
        }

        if (gridDistance(center, candidate->position()) <= 1) {
            applyDamage(candidate, amplifiedDamage(caster, caster->atk() + 20));
        }
    }
    addLog(QStringLiteral("%1释放奥术爆裂。").arg(caster->name()));
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
