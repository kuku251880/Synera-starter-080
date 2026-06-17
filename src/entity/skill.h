#ifndef SKILL_H
#define SKILL_H

#include <QList>
#include <QPoint>
#include <QString>
#include <functional>
#include <memory>

class Unit;

enum class SkillType
{
    PowerStrike,
    SelfHeal,
    ArcaneBurst
};

class Skill
{
public:
    virtual ~Skill() = default;

    virtual SkillType type() const = 0;
    virtual QString name() const = 0;
    virtual void cast(Unit* caster, Unit* target, const QList<Unit*>& units,
                      const std::function<void(Unit*, int)>& applyDamage,
                      const std::function<int(const QPoint&, const QPoint&)>& gridDistance,
                      const std::function<void(const QString&)>& addLog,
                      const std::function<void(Unit*)>& flashSkill) const = 0;
};

class PowerStrikeSkill : public Skill
{
public:
    SkillType type() const override { return SkillType::PowerStrike; }
    QString name() const override;
    void cast(Unit* caster, Unit* target, const QList<Unit*>& units, const std::function<void(Unit*, int)>& applyDamage,
              const std::function<int(const QPoint&, const QPoint&)>& gridDistance,
              const std::function<void(const QString&)>& addLog,
              const std::function<void(Unit*)>& flashSkill) const override;
};

class SelfHealSkill : public Skill
{
public:
    SkillType type() const override { return SkillType::SelfHeal; }
    QString name() const override;
    void cast(Unit* caster, Unit* target, const QList<Unit*>& units, const std::function<void(Unit*, int)>& applyDamage,
              const std::function<int(const QPoint&, const QPoint&)>& gridDistance,
              const std::function<void(const QString&)>& addLog,
              const std::function<void(Unit*)>& flashSkill) const override;
};

class ArcaneBurstSkill : public Skill
{
public:
    SkillType type() const override { return SkillType::ArcaneBurst; }
    QString name() const override;
    void cast(Unit* caster, Unit* target, const QList<Unit*>& units, const std::function<void(Unit*, int)>& applyDamage,
              const std::function<int(const QPoint&, const QPoint&)>& gridDistance,
              const std::function<void(const QString&)>& addLog,
              const std::function<void(Unit*)>& flashSkill) const override;
};

std::unique_ptr<Skill> createSkill(SkillType type);

#endif // SKILL_H
