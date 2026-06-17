#include "game.h"
#include "entity/skill.h"
#include "entity/unit.h"
#include "gui/griditem.h"
#include "gui/unititem.h"
#include <QBuffer>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QTextStream>
#include <QTimer>
#include <QtMath>
#include <algorithm>
#include <array>
#include <exception>
#include <limits>
#include <stdexcept>

namespace {
constexpr qreal kZGrid = 0.0;
constexpr qreal kZUnit = 1.0;
constexpr qreal kZDraggingUnit = 2.0;
constexpr int kCurrentSaveVersion = 2;

std::runtime_error makeRuntimeError(const QString& message)
{
    return std::runtime_error(message.toUtf8().constData());
}

QJsonArray stringListToJsonArray(const QStringList& values)
{
    QJsonArray array;
    for (const QString& value : values) {
        array.append(value);
    }
    return array;
}

QJsonArray stringVectorToJsonArray(const QVector<QString>& values)
{
    QJsonArray array;
    for (const QString& value : values) {
        array.append(value);
    }
    return array;
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

QJsonObject migrateSaveObject(QJsonObject saveObject)
{
    const int version = saveObject.value(QStringLiteral("version")).toInt(1);
    if (version < 2) {
        if (!saveObject.contains(QStringLiteral("shop"))) {
            saveObject.insert(QStringLiteral("shop"), QJsonArray());
        }
        if (!saveObject.contains(QStringLiteral("equipment"))) {
            saveObject.insert(QStringLiteral("equipment"), QJsonArray());
        }
        if (!saveObject.contains(QStringLiteral("achievements"))) {
            saveObject.insert(QStringLiteral("achievements"), QJsonArray());
        }
        if (!saveObject.contains(QStringLiteral("units"))) {
            saveObject.insert(QStringLiteral("units"), QJsonArray());
        }
    }

    saveObject.insert(QStringLiteral("version"), kCurrentSaveVersion);
    return saveObject;
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

SkillType skillTypeFromKey(const QString& key, SkillType fallback)
{
    if (key == QStringLiteral("PowerStrike")) {
        return SkillType::PowerStrike;
    }
    if (key == QStringLiteral("SelfHeal")) {
        return SkillType::SelfHeal;
    }
    if (key == QStringLiteral("ArcaneBurst")) {
        return SkillType::ArcaneBurst;
    }
    return fallback;
}

struct TraitThreshold
{
    int count;
    QString description;
    int teamAtkBonus;
    int traitMaxHpBonus;
    int traitRangeBonus;
    int traitMaxManaBonus;
    int traitManaGainBonus;
    int traitSkillAmpPercent;
    int traitExtraStrikeChance;
};

struct TraitRule
{
    QString name;
    QVector<TraitThreshold> thresholds;
};

struct ActiveTraitBonus
{
    QString name;
    int count;
    TraitThreshold threshold;
};

struct RoundEvent
{
    QString key;
    QString description;
    int every;
    int priority;
    int goldBonus;
};

struct EnemyRoundTemplate
{
    QString name;
    int baseMaxHp;
    int baseAtk;
    int hpGrowth;
    int atkGrowth;
    int eliteMaxHpBonus;
    int eliteAtkBonus;
};

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

QVector<TraitRule> defaultTraitRules()
{
    return {
        {QStringLiteral("人类"),
         {{2, QStringLiteral("全队攻击+10"), 10, 0, 0, 0, 0, 0, 0},
          {4, QStringLiteral("全队攻击+25"), 25, 0, 0, 0, 0, 0, 0}}},
        {QStringLiteral("前排"),
         {{2, QStringLiteral("前排生命+120"), 0, 120, 0, 0, 0, 0, 0},
          {3, QStringLiteral("前排生命+240"), 0, 240, 0, 0, 0, 0, 0}}},
        {QStringLiteral("游侠"), {{2, QStringLiteral("普攻35%连击"), 0, 0, 0, 0, 0, 0, 35}}},
        {QStringLiteral("奥术"), {{2, QStringLiteral("技能+25%，法力更快"), 0, 0, 0, -10, 10, 25, 0}}},
    };
}

QVector<TraitRule> loadTraitRules()
{
    const QString path = projectRelativeFilePath(QStringLiteral("data/traits.json"));
    if (path.isEmpty()) {
        return defaultTraitRules();
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return defaultTraitRules();
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return defaultTraitRules();
    }

    QVector<TraitRule> rules;
    const QJsonArray traitArray = document.object().value(QStringLiteral("traits")).toArray();
    for (const QJsonValue& traitValue : traitArray) {
        const QJsonObject traitObject = traitValue.toObject();
        TraitRule rule;
        rule.name = traitObject.value(QStringLiteral("name")).toString();
        if (rule.name.isEmpty()) {
            continue;
        }

        const QJsonArray thresholdArray = traitObject.value(QStringLiteral("thresholds")).toArray();
        for (const QJsonValue& thresholdValue : thresholdArray) {
            const QJsonObject thresholdObject = thresholdValue.toObject();
            TraitThreshold threshold;
            threshold.count = thresholdObject.value(QStringLiteral("count")).toInt(0);
            threshold.description = thresholdObject.value(QStringLiteral("description")).toString();
            threshold.teamAtkBonus = thresholdObject.value(QStringLiteral("teamAtkBonus")).toInt(0);
            threshold.traitMaxHpBonus = thresholdObject.value(QStringLiteral("traitMaxHpBonus")).toInt(0);
            threshold.traitRangeBonus = thresholdObject.value(QStringLiteral("traitRangeBonus")).toInt(0);
            threshold.traitMaxManaBonus = thresholdObject.value(QStringLiteral("traitMaxManaBonus")).toInt(0);
            threshold.traitManaGainBonus = thresholdObject.value(QStringLiteral("traitManaGainBonus")).toInt(0);
            threshold.traitSkillAmpPercent = thresholdObject.value(QStringLiteral("traitSkillAmpPercent")).toInt(0);
            threshold.traitExtraStrikeChance = thresholdObject.value(QStringLiteral("traitExtraStrikeChance")).toInt(0);
            if (threshold.count > 0) {
                rule.thresholds.append(threshold);
            }
        }

        if (!rule.thresholds.isEmpty()) {
            std::sort(rule.thresholds.begin(), rule.thresholds.end(),
                      [](const TraitThreshold& a, const TraitThreshold& b) { return a.count < b.count; });
            rules.append(rule);
        }
    }

    return rules.isEmpty() ? defaultTraitRules() : rules;
}

const QVector<TraitRule>& traitRules()
{
    static const QVector<TraitRule> rules = loadTraitRules();
    return rules;
}

QVector<ActiveTraitBonus> activeTraitBonuses(const QHash<QString, int>& counts)
{
    QVector<ActiveTraitBonus> active;
    for (const TraitRule& rule : traitRules()) {
        const int count = counts.value(rule.name);
        const TraitThreshold* selected = nullptr;
        for (const TraitThreshold& threshold : rule.thresholds) {
            if (count >= threshold.count) {
                selected = &threshold;
            }
        }
        if (selected) {
            active.append({rule.name, count, *selected});
        }
    }
    return active;
}

QVector<RoundEvent> defaultRoundEvents()
{
    return {
        {QStringLiteral("elite"), QStringLiteral("精英来袭：本轮敌人属性提升。"), 5, 20, 0},
        {QStringLiteral("harvest"), QStringLiteral("丰收回合：准备阶段额外获得3金币。"), 3, 10, 3},
    };
}

QVector<RoundEvent> loadRoundEvents()
{
    const QString path = projectRelativeFilePath(QStringLiteral("data/events.json"));
    if (path.isEmpty()) {
        return defaultRoundEvents();
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return defaultRoundEvents();
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return defaultRoundEvents();
    }

    QVector<RoundEvent> events;
    const QJsonArray eventArray = document.object().value(QStringLiteral("events")).toArray();
    for (const QJsonValue& value : eventArray) {
        const QJsonObject object = value.toObject();
        RoundEvent event;
        event.key = object.value(QStringLiteral("key")).toString();
        event.description = object.value(QStringLiteral("description")).toString();
        event.every = object.value(QStringLiteral("every")).toInt(0);
        event.priority = object.value(QStringLiteral("priority")).toInt(0);
        event.goldBonus = object.value(QStringLiteral("goldBonus")).toInt(0);
        if (!event.key.isEmpty() && !event.description.isEmpty() && event.every > 0) {
            events.append(event);
        }
    }
    return events.isEmpty() ? defaultRoundEvents() : events;
}

const QVector<RoundEvent>& roundEvents()
{
    static const QVector<RoundEvent> events = loadRoundEvents();
    return events;
}

RoundEvent roundEventForRound(int round)
{
    RoundEvent selected;
    selected.key = QStringLiteral("none");
    selected.description = QStringLiteral("无");
    selected.every = 0;
    selected.priority = -1;
    selected.goldBonus = 0;

    if (round <= 0) {
        return selected;
    }

    for (const RoundEvent& event : roundEvents()) {
        if (event.every <= 0 || round % event.every != 0 || event.priority < selected.priority) {
            continue;
        }
        selected = event;
    }
    return selected;
}

QVector<EnemyRoundTemplate> defaultEnemyRoundTemplates()
{
    return {
        {QStringLiteral("敌方战士"), 120, 15, 20, 2, 80, 5},
        {QStringLiteral("敌方弓手"), 85, 18, 20, 2, 60, 5},
    };
}

QVector<EnemyRoundTemplate> loadEnemyRoundTemplates()
{
    const QString path = projectRelativeFilePath(QStringLiteral("data/enemy_waves.json"));
    if (path.isEmpty()) {
        return defaultEnemyRoundTemplates();
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return defaultEnemyRoundTemplates();
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return defaultEnemyRoundTemplates();
    }

    QVector<EnemyRoundTemplate> enemies;
    const QJsonArray enemyArray = document.object().value(QStringLiteral("enemies")).toArray();
    for (const QJsonValue& value : enemyArray) {
        const QJsonObject object = value.toObject();
        EnemyRoundTemplate enemy;
        enemy.name = object.value(QStringLiteral("name")).toString();
        enemy.baseMaxHp = object.value(QStringLiteral("baseMaxHp")).toInt(100);
        enemy.baseAtk = object.value(QStringLiteral("baseAtk")).toInt(10);
        enemy.hpGrowth = object.value(QStringLiteral("hpGrowth")).toInt(0);
        enemy.atkGrowth = object.value(QStringLiteral("atkGrowth")).toInt(0);
        enemy.eliteMaxHpBonus = object.value(QStringLiteral("eliteMaxHpBonus")).toInt(0);
        enemy.eliteAtkBonus = object.value(QStringLiteral("eliteAtkBonus")).toInt(0);
        if (!enemy.name.isEmpty()) {
            enemies.append(enemy);
        }
    }
    return enemies.isEmpty() ? defaultEnemyRoundTemplates() : enemies;
}

const QVector<EnemyRoundTemplate>& enemyRoundTemplates()
{
    static const QVector<EnemyRoundTemplate> enemies = loadEnemyRoundTemplates();
    return enemies;
}

LogCategory inferLogCategory(const QString& message)
{
    if (message.contains(QStringLiteral("保存")) || message.contains(QStringLiteral("读取"))) {
        return LogCategory::SaveLoad;
    }
    if (message.contains(QStringLiteral("释放"))) {
        return LogCategory::Skill;
    }
    if (message.contains(QStringLiteral("攻击")) || message.contains(QStringLiteral("伤害")) ||
        message.contains(QStringLiteral("阵亡")) || message.contains(QStringLiteral("胜利")) ||
        message.contains(QStringLiteral("失败"))) {
        return LogCategory::Combat;
    }
    if (message.contains(QStringLiteral("金币")) || message.contains(QStringLiteral("购买")) ||
        message.contains(QStringLiteral("刷新商店")) || message.contains(QStringLiteral("升级"))) {
        return LogCategory::Economy;
    }
    if (message.contains(QStringLiteral("羁绊")) || message.contains(QStringLiteral("成就"))) {
        return LogCategory::Trait;
    }
    return LogCategory::System;
}

QString logCategoryText(LogCategory category)
{
    switch (category) {
    case LogCategory::System:
        return QStringLiteral("系统");
    case LogCategory::Combat:
        return QStringLiteral("战斗");
    case LogCategory::Skill:
        return QStringLiteral("技能");
    case LogCategory::Economy:
        return QStringLiteral("经济");
    case LogCategory::SaveLoad:
        return QStringLiteral("存档");
    case LogCategory::Trait:
        return QStringLiteral("羁绊");
    }
    return QStringLiteral("系统");
}

QString logCategoryColor(LogCategory category)
{
    switch (category) {
    case LogCategory::System:
        return QStringLiteral("#9fa6b2");
    case LogCategory::Combat:
        return QStringLiteral("#f08c74");
    case LogCategory::Skill:
        return QStringLiteral("#a78bfa");
    case LogCategory::Economy:
        return QStringLiteral("#ffd36a");
    case LogCategory::SaveLoad:
        return QStringLiteral("#58c28d");
    case LogCategory::Trait:
        return QStringLiteral("#64d2ff");
    }
    return QStringLiteral("#9fa6b2");
}
} // namespace

Game::Game(QObject* parent)
    : QObject(parent), m_benchSlots(GameConstants::kBenchSlotCount, nullptr), m_shopSlots(GameConstants::kShopSlotCount), m_scene(new QGraphicsScene(this)),
      m_leftInfoPanel(nullptr), m_infoPanel(nullptr), m_combatTimer(new QTimer(this)), m_countdownTimer(nullptr),
      m_resultTimer(nullptr), m_resultOverlay(nullptr), m_resultText(nullptr),
      m_countdownOverlay(nullptr), m_countdownText(nullptr), m_countdownValue(0), m_dragActive(false),
      m_activeUnitId(-1), m_selectedUnitId(-1), m_sourceGrid(-1, -1), m_phase(GamePhase::Prepare),
      m_lastResult(QStringLiteral("请布置你的阵容。")), m_currentEvent(QStringLiteral("无")), m_eventRewardRound(0),
      m_rows(Board::ROWS), m_benchSlotCount(GameConstants::kBenchSlotCount), m_cellSize(GameConstants::kCellSize), m_cellGap(GameConstants::kCellGap), m_benchGap(GameConstants::kBenchGap)
{
    m_combatTimer->setInterval(GameConstants::kCombatTickIntervalMs);
    connect(m_combatTimer, &QTimer::timeout, this, &Game::updateCombat);
}

Game::~Game()
{
    qDeleteAll(m_units);
    m_units.clear();
}

void Game::initialize()
{
    createStarterUnitsIfNeeded();
    rollShop();
    buildScene();
    reset();
}

void Game::reset()
{
    m_combatTimer->stop();
    m_phase = GamePhase::Prepare;
    m_lastResult = QStringLiteral("请布置你的阵容。");
    m_currentEvent = QStringLiteral("无");
    m_achievements.clear();
    m_logs.clear();
    m_eventRewardRound = 0;
    m_player.setHp(GameConstants::kInitialPlayerHp);
    m_player.setGold(GameConstants::kInitialPlayerGold);
    m_player.setLevel(1);
    m_player.setPopulationLimit(GameConstants::kInitialPopulation);
    m_player.setCurrentRound(1);
    m_player.setWinStreak(0);
    m_player.setLossStreak(0);
    rollShop();
    setupRoundBoard();
    addLog(QStringLiteral("新游戏开始。"));
}

void Game::startCombat()
{
    if (m_phase != GamePhase::Prepare || m_combatTimer->isActive()) {
        return;
    }

    if (playerBoardUnitCount() == 0 || sideDefeated(UnitOwner::EnemyCtrl)) {
        m_lastResult = QStringLiteral("请先部署单位再开始战斗。");
        updateInfoPanel();
        return;
    }

    // Reset all combat state, apply traits
    refreshTraitBonuses();
    for (Unit* unit : m_units) {
        if (!unit) continue;
        if (unit->isAlive() && m_board.isValidPosition(unit->position()) &&
            m_board.getUnitAt(unit->position()) == unit) {
            unit->resetCombatState();
        }
    }
    syncFromBoard();

    // Enter countdown phase instead of jumping straight into combat
    startCountdown();
}

void Game::startCountdown()
{
    m_phase = GamePhase::PreCombat;
    m_lastResult = QStringLiteral("准备战斗...");
    m_countdownValue = 3;

    if (!m_countdownTimer) {
        m_countdownTimer = new QTimer(this);
        m_countdownTimer->setSingleShot(false);
        connect(m_countdownTimer, &QTimer::timeout, this, &Game::tickCountdown);
    }

    // Show countdown overlay
    if (!m_countdownOverlay) {
        const QRectF sceneRect = m_scene->sceneRect();
        const QRectF overlayRect(sceneRect.left(), sceneRect.top(),
                                 sceneRect.width(), sceneRect.height());
        m_countdownOverlay = m_scene->addRect(overlayRect,
            QPen(Qt::NoPen), QBrush(QColor(0, 0, 0, 120)));
        m_countdownOverlay->setZValue(5.0);

        QFont countFont;
        countFont.setPointSize(64);
        countFont.setBold(true);
        m_countdownText = m_scene->addText(QString(), countFont);
        m_countdownText->setDefaultTextColor(QColor(255, 218, 107));
        m_countdownText->setZValue(5.1);
    } else {
        m_countdownOverlay->setVisible(true);
        m_countdownText->setVisible(true);
    }
    tickCountdown();

    m_countdownTimer->start(700); // one tick every 700ms
}

void Game::tickCountdown()
{
    if (!m_countdownOverlay || !m_countdownText) return;

    if (m_countdownValue > 0) {
        const QString number = QString::number(m_countdownValue);
        m_countdownText->setPlainText(number);
        const QRectF sceneRect = m_scene->sceneRect();
        const QRectF textRect = m_countdownText->boundingRect();
        m_countdownText->setPos(sceneRect.center().x() - textRect.width() / 2,
                                 sceneRect.center().y() - textRect.height() / 2);

        // Flash scale effect via font size
        QFont f = m_countdownText->font();
        f.setPointSize(64);
        m_countdownText->setFont(f);
        m_countdownValue--;
    } else {
        // "战斗开始!" flash
        m_countdownText->setPlainText(QStringLiteral("战斗开始!"));
        QFont f = m_countdownText->font();
        f.setPointSize(36);
        m_countdownText->setFont(f);
        const QRectF sceneRect = m_scene->sceneRect();
        const QRectF textRect = m_countdownText->boundingRect();
        m_countdownText->setPos(sceneRect.center().x() - textRect.width() / 2,
                                 sceneRect.center().y() - textRect.height() / 2);

        m_countdownTimer->stop();

        // Hide overlay after brief delay, then start combat
        QTimer::singleShot(400, this, [this]() {
            if (m_countdownOverlay) {
                m_countdownOverlay->setVisible(false);
                m_countdownText->setVisible(false);
            }
            m_phase = GamePhase::Combat;
            m_lastResult = QStringLiteral("战斗进行中。");
            addLog(QStringLiteral("第%1轮战斗开始。").arg(m_player.currentRound()));
            updateInfoPanel();
            m_combatTimer->start();
        });
    }
}

void Game::setupRoundBoard(bool preservePlayerLayout)
{
    // Save player unit board positions before clearing (if preserving layout)
    QHash<int, QPoint> savedPositions;
    if (preservePlayerLayout) {
        for (Unit* unit : m_units) {
            if (!unit || unit->owner() != UnitOwner::PlayerCtrl) continue;
            const QPoint pos = unit->position();
            if (m_board.isValidPosition(pos) && m_board.getUnitAt(pos) == unit) {
                savedPositions[unit->id()] = pos;
            }
        }
    }

    m_board.clear();
    m_benchSlots.fill(nullptr, m_benchSlotCount);
    updateRoundEvent();
    generateEnemyRound(m_player.currentRound());

    const QPoint playerPositions[] = {QPoint(0, 7), QPoint(1, 7), QPoint(2, 7)};
    const QPoint enemyPositions[] = {QPoint(5, 0), QPoint(6, 0)};
    constexpr int kPlayerDefaultCount = 3;

    // Reset all units before repositioning
    for (Unit* unit : m_units) {
        if (!unit) continue;
        unit->clearTraitBonuses();
        unit->resetCombatState();
    }

    int benchSlot = 0;
    int deployedEnemies = 0;

    // Place enemies
    for (Unit* unit : m_units) {
        if (!unit || unit->owner() != UnitOwner::EnemyCtrl) continue;
        if (deployedEnemies < GameConstants::kEnemyDeployCount) {
            m_board.addUnit(unit, enemyPositions[deployedEnemies]);
            ++deployedEnemies;
        }
    }

    // Place player units
    const int populationCap = m_player.populationLimit();
    int boardSlotsUsed = 0;
    QSet<int> placedIds;

    // Pass 1: restore saved board positions (if preserving layout)
    if (preservePlayerLayout) {
        for (Unit* unit : m_units) {
            if (!unit || unit->owner() != UnitOwner::PlayerCtrl) continue;
            if (boardSlotsUsed >= populationCap) break;
            auto savedIt = savedPositions.constFind(unit->id());
            if (savedIt != savedPositions.constEnd()) {
                const QPoint& pos = savedIt.value();
                if (m_board.isValidPosition(pos) && m_board.isPlayerHalf(pos) && !m_board.hasUnitAt(pos)) {
                    m_board.addUnit(unit, pos);
                    placedIds.insert(unit->id());
                    ++boardSlotsUsed;
                }
            }
        }
    }

    // Pass 2: fill remaining board slots with default positions
    int posIdx = 0;
    for (Unit* unit : m_units) {
        if (!unit || unit->owner() != UnitOwner::PlayerCtrl) continue;
        if (placedIds.contains(unit->id())) continue;
        if (boardSlotsUsed >= populationCap) break;

        while (posIdx < kPlayerDefaultCount) {
            const QPoint& pos = playerPositions[posIdx];
            ++posIdx;
            if (!m_board.hasUnitAt(pos)) {
                m_board.addUnit(unit, pos);
                placedIds.insert(unit->id());
                ++boardSlotsUsed;
                break;
            }
        }
    }

    // Pass 3: remaining units go to bench
    for (Unit* unit : m_units) {
        if (!unit || unit->owner() != UnitOwner::PlayerCtrl) continue;
        if (placedIds.contains(unit->id())) continue;

        if (benchSlot < m_benchSlotCount) {
            m_benchSlots[benchSlot] = unit;
            unit->setPosition(QPoint(benchSlot, Board::ROWS));
            ++benchSlot;
        }
    }

    syncFromBoard();
}

void Game::handleDragStarted(int unitId, const QPoint& sourceGrid, const QPointF&)
{
    if (m_phase != GamePhase::Prepare) {
        return;
    }

    Unit* unit = findUnitById(unitId);
    if (!unit || unit->owner() != UnitOwner::PlayerCtrl || unitAtGrid(sourceGrid) != unit) {
        return;
    }

    m_dragActive = true;
    m_activeUnitId = unitId;
    m_sourceGrid = sourceGrid;

    UnitItem* item = findUnitItem(unitId);
    if (item) {
        item->setZValue(kZDraggingUnit);
    }

    showDropHints(unitId, sourceGrid, QPoint(-1, -1));
}

void Game::handleDragMoved(int unitId, const QPoint&, const QPointF& scenePos)
{
    if (!m_dragActive) {
        return;
    }

    const QPoint target = worldToGrid(scenePos);
    showDropHints(unitId, m_sourceGrid, target);

    // Highlight sell zone when dragging over it
    if (m_sellZoneItem) {
        const bool overSellZone = m_sellZoneRect.contains(scenePos);
        m_sellZoneItem->setBrush(QBrush(overSellZone
            ? QColor(180, 40, 40, 220)
            : QColor(60, 25, 25, 180)));
        m_sellZoneItem->setPen(QPen(overSellZone
            ? QColor(255, 80, 80)
            : QColor(180, 60, 60), 2));

        // Update sell zone text with unit price when hovering
        if (m_sellZoneText) {
            if (overSellZone) {
                Unit* draggedUnit = findUnitById(unitId);
                if (draggedUnit) {
                    const int price = GameConstants::kUnitCost * draggedUnit->starLevel();
                    m_sellZoneText->setPlainText(
                        QStringLiteral("出售 %1 可获得 %2 金币").arg(draggedUnit->name()).arg(price));
                    const QRectF tr = m_sellZoneText->boundingRect();
                    m_sellZoneText->setPos(m_sellZoneRect.center().x() - tr.width() / 2,
                                           m_sellZoneRect.center().y() - tr.height() / 2);
                }
            } else {
                m_sellZoneText->setPlainText(QStringLiteral("将角色拖动到此处来出售"));
                const QRectF tr = m_sellZoneText->boundingRect();
                m_sellZoneText->setPos(m_sellZoneRect.center().x() - tr.width() / 2,
                                       m_sellZoneRect.center().y() - tr.height() / 2);
            }
        }
    }
}

void Game::handleDropCommand(int unitId, const QPoint&, const QPointF& scenePos)
{
    if (!m_dragActive) {
        return;
    }

    // Sell zone drop: sell the dragged unit
    if (m_sellZoneRect.contains(scenePos)) {
        clearGridHighlights();

        UnitItem* item = findUnitItem(m_activeUnitId);
        if (item) {
            item->setZValue(kZUnit);
        }

        m_dragActive = false;
        m_activeUnitId = -1;
        m_sourceGrid = QPoint(-1, -1);

        sellSelectedUnit(unitId);
        syncFromBoard();
        return;
    }

    const QPoint target = worldToGrid(scenePos);

    clearGridHighlights();
    if (canApplyDrop(unitId, m_sourceGrid, target)) {
        applyDrop(unitId, m_sourceGrid, target);
    }

    UnitItem* item = findUnitItem(m_activeUnitId);
    if (item) {
        item->setZValue(kZUnit);
    }

    m_dragActive = false;
    m_activeUnitId = -1;
    m_sourceGrid = QPoint(-1, -1);

    syncFromBoard();
}

void Game::handleUnitSelected(int unitId)
{
    if (!findUnitById(unitId)) {
        return;
    }

    m_selectedUnitId = unitId;
    for (UnitItem* item : m_unitItems) {
        if (item) {
            item->setSelectedActive(item->unitId() == m_selectedUnitId);
        }
    }
    updateInfoPanel();
}

void Game::buyShopUnit(int slot)
{
    if (m_phase != GamePhase::Prepare || slot < 0 || slot >= m_shopSlots.size()) {
        return;
    }

    const QString unitName = m_shopSlots.at(slot);
    if (unitName.isEmpty()) {
        m_lastResult = QStringLiteral("这个商店位置已经为空。");
        updateInfoPanel();
        return;
    }

    if (m_player.gold() < GameConstants::kUnitCost) {
        m_lastResult = QStringLiteral("金币不足。");
        updateInfoPanel();
        return;
    }

    Unit* unit = createUnitFromTemplate(unitName, UnitOwner::PlayerCtrl);
    if (!addUnitToBench(unit)) {
        delete unit;
        m_lastResult = QStringLiteral("备战区已满。");
        updateInfoPanel();
        return;
    }

    m_player.setGold(m_player.gold() - GameConstants::kUnitCost);
    m_units.append(unit);
    m_player.addUnit(unit->id());
    m_shopSlots[slot].clear();
    m_selectedUnitId = unit->id();
    addLog(QStringLiteral("购买了%1。").arg(unit->name()));
    tryMergeUnits();
    checkAchievements();
    buildScene();
    syncFromBoard();
}

void Game::rerollShop()
{
    if (m_phase != GamePhase::Prepare) {
        return;
    }

    if (m_player.gold() < GameConstants::kRerollCost) {
        m_lastResult = QStringLiteral("金币不足，无法刷新商店。");
        updateInfoPanel();
        return;
    }

    m_player.setGold(m_player.gold() - GameConstants::kRerollCost);
    rollShop();
    m_lastResult = QStringLiteral("商店已刷新。");
    addLog(QStringLiteral("刷新商店，花费%1金币。").arg(GameConstants::kRerollCost));
    checkAchievements();
    updateInfoPanel();
}

void Game::levelUp()
{
    if (m_phase != GamePhase::Prepare) {
        return;
    }

    if (m_player.level() >= GameConstants::kMaxLevel) {
        m_lastResult = QStringLiteral("已经达到最高等级。");
        updateInfoPanel();
        return;
    }
    if (m_player.gold() < GameConstants::levelUpCost(m_player.level())) {
        m_lastResult = QStringLiteral("金币不足，无法升级。");
        updateInfoPanel();
        return;
    }

    m_player.setGold(m_player.gold() - GameConstants::levelUpCost(m_player.level()));
    m_player.setLevel(m_player.level() + 1);
    m_player.setPopulationLimit(qMin(GameConstants::kMaxPopulation, m_player.populationLimit() + 1));
    m_lastResult = QStringLiteral("升级成功，人口上限提升。");
    addLog(QStringLiteral("升级到%1级，人口上限为%2。").arg(m_player.level()).arg(m_player.populationLimit()));
    checkAchievements();
    updateInfoPanel();
}

void Game::equipSelectedUnit()
{
    if (m_phase != GamePhase::Prepare || m_equipmentPool.isEmpty()) {
        m_lastResult = QStringLiteral("当前没有可用装备。");
        updateInfoPanel();
        return;
    }

    Unit* unit = findUnitById(m_selectedUnitId);
    if (!unit || unit->owner() != UnitOwner::PlayerCtrl) {
        m_lastResult = QStringLiteral("请先选择一个己方单位。");
        updateInfoPanel();
        return;
    }

    if (unit->equipmentNames().size() >= GameConstants::kMaxEquipmentPerUnit) {
        m_lastResult = QStringLiteral("该单位装备数量已满。");
        updateInfoPanel();
        return;
    }

    Equipment equipment = m_equipmentPool.takeFirst();
    equipment.applyTo(unit);
    m_lastResult = QStringLiteral("已给%1装备%2。").arg(unit->name(), equipment.name());
    addLog(m_lastResult);
    checkAchievements();
    syncFromBoard();
}

void Game::sellSelectedUnit(int unitId)
{
    if (m_phase != GamePhase::Prepare) {
        return;
    }

    const int targetId = (unitId >= 0) ? unitId : m_selectedUnitId;
    Unit* unit = findUnitById(targetId);
    if (!unit || unit->owner() != UnitOwner::PlayerCtrl) {
        m_lastResult = QStringLiteral("请先选择一个己方单位来出售。");
        updateInfoPanel();
        return;
    }

    const int sellPrice = GameConstants::kUnitCost * unit->starLevel();
    const QString unitName = unit->name();

    const QPoint pos = unit->position();
    if (m_board.isValidPosition(pos) && m_board.getUnitAt(pos) == unit) {
        m_board.removeUnit(unit);
    } else {
        const int benchSlot = benchIndexOf(unit);
        if (benchSlot >= 0) {
            m_benchSlots[benchSlot] = nullptr;
        }
    }

    m_player.removeUnit(unit->id());
    m_units.removeOne(unit);
    delete unit;

    m_player.setGold(m_player.gold() + sellPrice);
    m_selectedUnitId = -1;
    m_lastResult = QStringLiteral("出售了%1，获得%2金币。").arg(unitName).arg(sellPrice);
    addLog(m_lastResult);
    checkAchievements();
    buildScene();
    syncFromBoard();
}

void Game::saveGame()
{
    saveGame(1);
}

void Game::saveGame(int slot)
{
    try {
        const int normalizedSlot = qBound(1, slot, 3);
        QFile file(saveFileName(normalizedSlot));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            throw makeRuntimeError(QStringLiteral("无法打开存档文件。"));
        }

        QJsonObject root;
        root.insert(QStringLiteral("format"), QStringLiteral("SyneraSave"));
        root.insert(QStringLiteral("version"), kCurrentSaveVersion);
        root.insert(QStringLiteral("savedAt"), QDateTime::currentDateTime().toString(Qt::ISODate));

        QJsonObject playerObject;
        playerObject.insert(QStringLiteral("hp"), m_player.hp());
        playerObject.insert(QStringLiteral("gold"), m_player.gold());
        playerObject.insert(QStringLiteral("level"), m_player.level());
        playerObject.insert(QStringLiteral("populationLimit"), m_player.populationLimit());
        playerObject.insert(QStringLiteral("currentRound"), m_player.currentRound());
        playerObject.insert(QStringLiteral("winStreak"), m_player.winStreak());
        playerObject.insert(QStringLiteral("lossStreak"), m_player.lossStreak());
        root.insert(QStringLiteral("player"), playerObject);

        root.insert(QStringLiteral("shop"), stringVectorToJsonArray(m_shopSlots));

        QJsonArray equipmentArray;
        for (const Equipment& equipment : m_equipmentPool) {
            equipmentArray.append(equipment.name());
        }
        root.insert(QStringLiteral("equipment"), equipmentArray);
        root.insert(QStringLiteral("achievements"), stringListToJsonArray(m_achievements));

        QJsonArray unitArray;
        for (Unit* unit : m_units) {
            if (!unit || unit->owner() == UnitOwner::EnemyCtrl) {
                continue;
            }

            QJsonObject unitObject;
            unitObject.insert(QStringLiteral("name"), unit->name());
            unitObject.insert(QStringLiteral("starLevel"), unit->starLevel());

            // Compute equipment-free base stats so that equipment can be
            // cleanly re-applied on load.  Subtracts total equipment bonuses
            // from the stored base values.
            int cleanMaxHp = unit->baseMaxHp();
            int cleanAtk = unit->baseAtk();
            int cleanRange = unit->baseRange();
            int cleanMaxMana = unit->baseMaxMana();
            int cleanAttackInterval = unit->attackInterval();
            int cleanHp = unit->hp();
            for (const QString& eqName : unit->equipmentNames()) {
                Equipment eq = equipmentFromName(eqName);
                cleanMaxHp -= eq.maxHpBonus();
                cleanAtk -= eq.atkBonus();
                cleanMaxMana -= eq.maxManaBonus();
                cleanAttackInterval -= eq.attackIntervalBonus();
                cleanHp -= eq.hpBonus();
            }

            unitObject.insert(QStringLiteral("hp"), qMin(qMax(0, cleanHp), qMax(1, cleanMaxHp)));
            unitObject.insert(QStringLiteral("maxHp"), qMax(1, cleanMaxHp));
            unitObject.insert(QStringLiteral("atk"), cleanAtk);
            unitObject.insert(QStringLiteral("range"), cleanRange);
            unitObject.insert(QStringLiteral("maxMana"), cleanMaxMana);
            unitObject.insert(QStringLiteral("mana"), qMin(unit->mana(), qMax(20, cleanMaxMana)));
            unitObject.insert(QStringLiteral("attackInterval"), qMax(2, cleanAttackInterval));
            unitObject.insert(QStringLiteral("skillType"), skillTypeKey(unit->skillType()));
            unitObject.insert(QStringLiteral("traits"), stringListToJsonArray(unit->traits()));
            unitObject.insert(QStringLiteral("equipmentNames"), stringListToJsonArray(unit->equipmentNames()));

            QJsonObject locationObject;
            locationObject.insert(QStringLiteral("type"), QStringLiteral("hidden"));
            QPoint pos = unit->position();
            if (m_board.isValidPosition(pos) && m_board.getUnitAt(pos) == unit) {
                locationObject.insert(QStringLiteral("type"), QStringLiteral("board"));
                locationObject.insert(QStringLiteral("x"), pos.x());
                locationObject.insert(QStringLiteral("y"), pos.y());
            } else {
                const int benchSlot = benchIndexOf(unit);
                if (benchSlot >= 0) {
                    locationObject.insert(QStringLiteral("type"), QStringLiteral("bench"));
                    locationObject.insert(QStringLiteral("slot"), benchSlot);
                }
            }
            unitObject.insert(QStringLiteral("location"), locationObject);

            unitArray.append(unitObject);
        }
        root.insert(QStringLiteral("units"), unitArray);

        const QByteArray saveData = QJsonDocument(root).toJson(QJsonDocument::Indented);
        if (file.write(saveData) != saveData.size() || !file.flush()) {
            throw makeRuntimeError(QStringLiteral("写入存档文件失败：%1").arg(file.errorString()));
        }

        m_lastResult = QStringLiteral("已保存到存档 %1。").arg(normalizedSlot);
        addLog(QStringLiteral("保存游戏到存档 %1。").arg(normalizedSlot));
        updateInfoPanel();
    } catch (const std::exception& error) {
        m_lastResult = QStringLiteral("保存失败：%1").arg(QString::fromUtf8(error.what()));
        updateInfoPanel();
    } catch (...) {
        m_lastResult = QStringLiteral("保存失败：未知错误。");
        updateInfoPanel();
    }
}

void Game::loadGame()
{
    loadGame(1);
}

void Game::loadGame(int slot)
{
    try {
        const int normalizedSlot = qBound(1, slot, 3);
        const QString jsonPath = saveFileName(normalizedSlot);
        const QString selectedPath = QFile::exists(jsonPath) ? jsonPath : legacySaveFileName(normalizedSlot);
        QFile file(selectedPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            throw makeRuntimeError(QStringLiteral("没有找到存档 %1。").arg(normalizedSlot));
        }

        const QByteArray saveData = file.readAll();
        if (file.error() != QFileDevice::NoError) {
            throw makeRuntimeError(QStringLiteral("读取存档文件失败：%1").arg(file.errorString()));
        }

        m_combatTimer->stop();
        m_phase = GamePhase::Prepare;
        m_board.clear();
        m_benchSlots.fill(nullptr, m_benchSlotCount);
        qDeleteAll(m_units);
        m_units.clear();
        m_player.clearUnits();
        m_equipmentPool.clear();
        m_achievements.clear();
        m_logs.clear();
        m_dragActive = false;
        m_activeUnitId = -1;
        m_selectedUnitId = -1;
        m_player.setWinStreak(0);
        m_player.setLossStreak(0);

        if (saveData.trimmed().startsWith('{')) {
            loadJsonSaveData(saveData);
        } else {
            loadLegacySaveData(saveData);
        }

        finalizeLoadedGame(normalizedSlot);
    } catch (const std::exception& error) {
        m_lastResult = QStringLiteral("读取存档失败：%1").arg(QString::fromUtf8(error.what()));
        updateInfoPanel();
    } catch (...) {
        m_lastResult = QStringLiteral("读取存档失败：未知错误。");
        updateInfoPanel();
    }
}

void Game::loadJsonSaveData(const QByteArray& saveData)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(saveData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        throw makeRuntimeError(QStringLiteral("存档 JSON 格式错误。"));
    }

    const QJsonObject root = migrateSaveObject(document.object());
    const QJsonObject playerObject = root.value(QStringLiteral("player")).toObject();
    m_player.setHp(playerObject.value(QStringLiteral("hp")).toInt(m_player.hp()));
    m_player.setGold(playerObject.value(QStringLiteral("gold")).toInt(m_player.gold()));
    m_player.setLevel(playerObject.value(QStringLiteral("level")).toInt(m_player.level()));
    m_player.setPopulationLimit(
        playerObject.value(QStringLiteral("populationLimit")).toInt(m_player.populationLimit()));
    m_player.setCurrentRound(playerObject.value(QStringLiteral("currentRound")).toInt(m_player.currentRound()));
    m_player.setWinStreak(playerObject.value(QStringLiteral("winStreak")).toInt(0));
    m_player.setLossStreak(playerObject.value(QStringLiteral("lossStreak")).toInt(0));

    m_shopSlots.fill(QString(), 5);
    const QJsonArray shopArray = root.value(QStringLiteral("shop")).toArray();
    for (int i = 0; i < shopArray.size() && i < m_shopSlots.size(); ++i) {
        m_shopSlots[i] = shopArray.at(i).toString();
    }

    for (const QString& equipmentName : jsonArrayToStringList(root.value(QStringLiteral("equipment")).toArray())) {
        m_equipmentPool.append(equipmentFromName(equipmentName));
    }

    m_achievements = jsonArrayToStringList(root.value(QStringLiteral("achievements")).toArray());

    const QJsonArray unitArray = root.value(QStringLiteral("units")).toArray();
    for (const QJsonValue& value : unitArray) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject unitObject = value.toObject();
        const QString unitName = unitObject.value(QStringLiteral("name")).toString();
        if (unitName.isEmpty()) {
            continue;
        }

        Unit* unit = createUnitFromTemplate(unitName, UnitOwner::PlayerCtrl);
        unit->setSkillType(
            skillTypeFromKey(unitObject.value(QStringLiteral("skillType")).toString(), unit->skillType()));
        const QStringList savedTraits = jsonArrayToStringList(unitObject.value(QStringLiteral("traits")).toArray());
        if (!savedTraits.isEmpty()) {
            unit->setTraits(savedTraits);
        }
        unit->setStarLevel(unitObject.value(QStringLiteral("starLevel")).toInt(unit->starLevel()));
        unit->setHp(unitObject.value(QStringLiteral("hp")).toInt(unit->hp()));
        unit->setMaxHp(unitObject.value(QStringLiteral("maxHp")).toInt(unit->baseMaxHp()));
        unit->setAtk(unitObject.value(QStringLiteral("atk")).toInt(unit->baseAtk()));
        unit->setRange(unitObject.value(QStringLiteral("range")).toInt(unit->baseRange()));
        unit->setMaxMana(unitObject.value(QStringLiteral("maxMana")).toInt(unit->baseMaxMana()));
        unit->setMana(unitObject.value(QStringLiteral("mana")).toInt(unit->mana()));
        unit->setAttackInterval(unitObject.value(QStringLiteral("attackInterval")).toInt(unit->attackInterval()));
        for (const QString& equipmentName :
             jsonArrayToStringList(unitObject.value(QStringLiteral("equipmentNames")).toArray())) {
            equipmentFromName(equipmentName).applyTo(unit);
        }

        m_units.append(unit);
        m_player.addUnit(unit->id());

        const QJsonObject locationObject = unitObject.value(QStringLiteral("location")).toObject();
        const QString locationType = locationObject.value(QStringLiteral("type")).toString();
        if (locationType == QStringLiteral("board")) {
            const QPoint pos(locationObject.value(QStringLiteral("x")).toInt(-1),
                             locationObject.value(QStringLiteral("y")).toInt(-1));
            if (m_board.isValidPosition(pos)) {
                m_board.addUnit(unit, pos);
            }
        } else if (locationType == QStringLiteral("bench")) {
            const int slot = locationObject.value(QStringLiteral("slot")).toInt(-1);
            if (slot >= 0 && slot < m_benchSlots.size()) {
                m_benchSlots[slot] = unit;
                unit->setPosition(QPoint(slot, Board::ROWS));
            }
        }
    }
}

void Game::loadLegacySaveData(const QByteArray& saveData)
{
    QBuffer buffer;
    buffer.setData(saveData);
    if (!buffer.open(QIODevice::ReadOnly)) {
        throw makeRuntimeError(QStringLiteral("旧存档读取失败。"));
    }

    QTextStream in(&buffer);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.startsWith(QStringLiteral("PLAYER "))) {
            const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 6) {
                m_player.setHp(parts.at(1).toInt());
                m_player.setGold(parts.at(2).toInt());
                m_player.setLevel(parts.at(3).toInt());
                m_player.setPopulationLimit(parts.at(4).toInt());
                m_player.setCurrentRound(parts.at(5).toInt());
                if (parts.size() >= 8) {
                    m_player.setWinStreak(parts.at(6).toInt());
                    m_player.setLossStreak(parts.at(7).toInt());
                }
            }
        } else if (line.startsWith(QStringLiteral("SHOP"))) {
            const QStringList parts = line.split('|');
            m_shopSlots.fill(QString(), 5);
            for (int i = 1; i < parts.size() && i - 1 < m_shopSlots.size(); ++i) {
                m_shopSlots[i - 1] = parts.at(i);
            }
        } else if (line.startsWith(QStringLiteral("EQUIPMENT"))) {
            const QStringList parts = line.split('|');
            for (int i = 1; i < parts.size(); ++i) {
                if (!parts.at(i).isEmpty()) {
                    m_equipmentPool.append(equipmentFromName(parts.at(i)));
                }
            }
        } else if (line.startsWith(QStringLiteral("ACHIEVEMENTS"))) {
            const QStringList parts = line.split('|');
            for (int i = 1; i < parts.size(); ++i) {
                if (!parts.at(i).isEmpty() && !m_achievements.contains(parts.at(i))) {
                    m_achievements.append(parts.at(i));
                }
            }
        } else if (line.startsWith(QStringLiteral("UNIT|"))) {
            const QStringList parts = line.split('|');
            if (parts.size() < 10) {
                continue;
            }

            Unit* unit = createUnitFromTemplate(parts.at(1), UnitOwner::PlayerCtrl);
            unit->setStarLevel(parts.at(2).toInt());
            unit->setHp(parts.at(3).toInt());
            unit->setMaxHp(parts.at(4).toInt());
            unit->setAtk(parts.at(5).toInt());
            unit->setRange(parts.at(6).toInt());
            unit->setMaxMana(parts.at(7).toInt());
            unit->setMana(parts.at(8).toInt());
            m_units.append(unit);
            m_player.addUnit(unit->id());

            const QString location = parts.at(9);
            if (location.startsWith(QStringLiteral("BOARD:"))) {
                const QStringList coords = location.split(':');
                if (coords.size() == 3) {
                    m_board.addUnit(unit, QPoint(coords.at(1).toInt(), coords.at(2).toInt()));
                }
            } else if (location.startsWith(QStringLiteral("BENCH:"))) {
                const QStringList slotParts = location.split(':');
                if (slotParts.size() == 2) {
                    const int slot = slotParts.at(1).toInt();
                    if (slot >= 0 && slot < m_benchSlots.size()) {
                        m_benchSlots[slot] = unit;
                        unit->setPosition(QPoint(slot, Board::ROWS));
                    }
                }
            }
        }
    }

    if (in.status() != QTextStream::Ok) {
        throw makeRuntimeError(QStringLiteral("读取旧存档文件失败。"));
    }
}

void Game::finalizeLoadedGame(int slot)
{
    m_eventRewardRound = m_player.currentRound();
    m_currentEvent = currentEventForRound(m_player.currentRound());
    generateEnemyRound(m_player.currentRound());
    const QPoint enemyPositions[] = {QPoint(5, 0), QPoint(6, 0)};
    int deployedEnemies = 0;
    for (Unit* unit : m_units) {
        if (!unit || unit->owner() != UnitOwner::EnemyCtrl || deployedEnemies >= 2) {
            continue;
        }
        unit->resetCombatState();
        m_board.addUnit(unit, enemyPositions[deployedEnemies]);
        ++deployedEnemies;
    }

    m_lastResult = QStringLiteral("存档 %1 读取完成。").arg(slot);
    addLog(QStringLiteral("读取存档 %1。").arg(slot));
    checkAchievements();
    buildScene();
    syncFromBoard();
}

bool Game::hasSaveSlot(int slot) const
{
    return QFile::exists(saveFileName(slot)) || QFile::exists(legacySaveFileName(slot));
}

QString Game::saveSlotTimeText(int slot) const
{
    QFileInfo info(saveFileName(slot));
    if (!info.exists()) {
        info.setFile(legacySaveFileName(slot));
    }
    if (!info.exists()) {
        return QStringLiteral("未保存");
    }

    return info.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

void Game::createStarterUnitsIfNeeded()
{
    if (!m_units.isEmpty()) {
        return;
    }

    auto addPlayerUnit = [this](Unit* unit) {
        m_units.append(unit);
        m_player.addUnit(unit->id());
        if (m_selectedUnitId < 0) {
            m_selectedUnitId = unit->id();
        }
    };

    const QStringList starterUnits = unitPool().mid(0, 5);
    for (const QString& unitName : starterUnits) {
        addPlayerUnit(createUnitFromTemplate(unitName, UnitOwner::PlayerCtrl));
    }

    generateEnemyRound(m_player.currentRound());
}

Unit* Game::createUnitFromTemplate(const QString& name, UnitOwner owner) const
{
    return Unit::create(name, owner);
}

QString Game::unitInfoForName(const QString& name) const
{
    // Build a temporary Unit via the factory to read stats; never added to the game.
    std::unique_ptr<Unit> probe(Unit::create(name, UnitOwner::PlayerCtrl));
    if (!probe || probe->name() != name) {
        // Fallback: check if the name matches a subtype that changes its display name
        for (const QString& candidate : {
                 QStringLiteral("战士"), QStringLiteral("弓手"), QStringLiteral("法师"),
                 QStringLiteral("预备兵"), QStringLiteral("守卫"),
                 QStringLiteral("敌方战士"), QStringLiteral("敌方弓手")
             }) {
            probe.reset(Unit::create(candidate, UnitOwner::PlayerCtrl));
            if (probe && probe->name() == name) break;
            if (probe && probe->typeName() == name) break;
        }
    }

    if (probe) {
        QStringList parts;
        parts << QStringLiteral("生命:%1").arg(probe->baseMaxHp());
        parts << QStringLiteral("攻击:%1").arg(probe->baseAtk());
        parts << QStringLiteral("射程:%1").arg(probe->baseRange());
        parts << QStringLiteral("法力:%1").arg(probe->baseMaxMana());
        parts << QStringLiteral("技能:%1").arg(skillName(probe->skillType()));
        parts << QStringLiteral("羁绊:%1").arg(probe->traits().join(QLatin1Char('/')));
        parts << QStringLiteral("购买:%1金").arg(GameConstants::kUnitCost);
        return parts.join(QStringLiteral("  "));
    }
    return QString();
}

QStringList Game::unitPool() const
{
    return {
        QStringLiteral("战士"),
        QStringLiteral("弓手"),
        QStringLiteral("法师"),
        QStringLiteral("预备兵"),
        QStringLiteral("守卫"),
    };
}

void Game::rollShop()
{
    const QStringList pool = unitPool();
    for (int i = 0; i < GameConstants::kShopSlotCount; ++i) {
        const int index = QRandomGenerator::global()->bounded(pool.size());
        m_shopSlots[i] = pool.at(index);
    }
}

int Game::firstEmptyBenchSlot() const
{
    for (int i = 0; i < m_benchSlots.size(); ++i) {
        if (!m_benchSlots.at(i)) {
            return i;
        }
    }
    return -1;
}

bool Game::addUnitToBench(Unit* unit)
{
    if (!unit) {
        return false;
    }

    const int slot = firstEmptyBenchSlot();
    if (slot < 0) {
        return false;
    }

    m_benchSlots[slot] = unit;
    unit->setPosition(QPoint(slot, Board::ROWS));
    return true;
}

void Game::tryMergeUnits()
{
    bool merged = true;
    while (merged) {
        merged = false;
        QHash<QString, QList<Unit*>> groups;

        for (Unit* unit : m_units) {
            if (!unit || unit->owner() != UnitOwner::PlayerCtrl || unit->starLevel() >= 2) {
                continue;
            }
            groups[unit->name() + QStringLiteral("#") + QString::number(unit->starLevel())].append(unit);
        }

        for (auto it = groups.begin(); it != groups.end(); ++it) {
            QList<Unit*> candidates = it.value();
            if (candidates.size() < 3) {
                continue;
            }

            Unit* keep = candidates.at(0);
            upgradeUnitStar(keep);

            for (int i = 1; i < 3; ++i) {
                Unit* removed = candidates.at(i);
                m_board.removeUnit(removed);
                const int benchSlot = benchIndexOf(removed);
                if (benchSlot >= 0) {
                    m_benchSlots[benchSlot] = nullptr;
                }
                m_player.removeUnit(removed->id());
                m_units.removeOne(removed);
                delete removed;
            }

            m_selectedUnitId = keep->id();
            m_lastResult = QStringLiteral("%1升到%2星。").arg(keep->name()).arg(keep->starLevel());
            merged = true;
            break;
        }
    }
}

void Game::upgradeUnitStar(Unit* unit)
{
    if (!unit) {
        return;
    }

    // Attribute layering: baseMaxHp/baseAtk store the cumulative base value
    // (template + previous star-ups + equipment), separate from trait bonuses
    // that are applied as temporary m_traitMaxHpBonus / m_traitAtkBonus.
    // Star-up scales the base value to preserve the multiplicative growth curve.
    unit->setStarLevel(unit->starLevel() + 1);
    unit->setMaxHp(unit->baseMaxHp() * GameConstants::kStarUpFactorNumerator / GameConstants::kStarUpFactorDenominator);
    unit->setHp(unit->baseMaxHp());
    unit->setAtk(unit->baseAtk() * GameConstants::kStarUpFactorNumerator / GameConstants::kStarUpFactorDenominator);
}

QHash<QString, int> Game::traitCounts() const
{
    QHash<QString, int> counts;
    for (Unit* unit : m_units) {
        if (!unit || unit->owner() != UnitOwner::PlayerCtrl || !unit->isAlive()) {
            continue;
        }

        const QPoint pos = unit->position();
        if (!m_board.isValidPosition(pos) || m_board.getUnitAt(pos) != unit) {
            continue;
        }

        for (const QString& trait : unit->traits()) {
            counts[trait] += 1;
        }
    }
    return counts;
}

void Game::refreshTraitBonuses()
{
    for (Unit* unit : m_units) {
        if (unit) {
            unit->clearTraitBonuses();
        }
    }

    const QVector<ActiveTraitBonus> activeBonuses = activeTraitBonuses(traitCounts());
    int teamAtkBonus = 0;
    for (const ActiveTraitBonus& bonus : activeBonuses) {
        teamAtkBonus += bonus.threshold.teamAtkBonus;
    }

    for (Unit* unit : m_units) {
        if (!unit || unit->owner() != UnitOwner::PlayerCtrl) {
            continue;
        }

        const QPoint pos = unit->position();
        if (!m_board.isValidPosition(pos) || m_board.getUnitAt(pos) != unit) {
            continue;
        }

        int maxHpBonus = 0;
        int atkBonus = teamAtkBonus;
        int rangeBonus = 0;
        int maxManaBonus = 0;
        int manaGainBonus = 0;
        int skillAmpPercent = 0;
        int extraStrikeChance = 0;

        for (const ActiveTraitBonus& bonus : activeBonuses) {
            if (!unit->traits().contains(bonus.name)) {
                continue;
            }

            maxHpBonus += bonus.threshold.traitMaxHpBonus;
            rangeBonus += bonus.threshold.traitRangeBonus;
            maxManaBonus += bonus.threshold.traitMaxManaBonus;
            manaGainBonus += bonus.threshold.traitManaGainBonus;
            skillAmpPercent += bonus.threshold.traitSkillAmpPercent;
            extraStrikeChance = qMax(extraStrikeChance, bonus.threshold.traitExtraStrikeChance);
        }

        unit->setTraitBonuses(maxHpBonus, atkBonus, rangeBonus, maxManaBonus, manaGainBonus, skillAmpPercent,
                              extraStrikeChance);
    }
}

QString Game::activeTraitsText() const
{
    const QHash<QString, int> counts = traitCounts();
    QStringList active;

    for (const ActiveTraitBonus& bonus : activeTraitBonuses(counts)) {
        active << QStringLiteral("%1×%2：%3").arg(bonus.name).arg(bonus.count).arg(bonus.threshold.description);
    }

    return active.isEmpty() ? QStringLiteral("无") : active.join(QStringLiteral("，"));
}

Equipment Game::randomEquipment() const
{
    const int roll = QRandomGenerator::global()->bounded(4);
    return Equipment(static_cast<EquipmentType>(roll));
}

Equipment Game::equipmentFromName(const QString& name) const
{
    return Equipment::fromName(name);
}

void Game::generateEnemyRound(int round)
{
    auto findEnemyByName = [this](const QString& name) -> Unit* {
        for (Unit* unit : m_units) {
            if (unit && unit->owner() == UnitOwner::EnemyCtrl && unit->name() == name) {
                return unit;
            }
        }
        return nullptr;
    };

    const bool eliteRound = roundEventForRound(round).key == QStringLiteral("elite");
    for (const EnemyRoundTemplate& enemyTemplate : enemyRoundTemplates()) {
        Unit* enemy = findEnemyByName(enemyTemplate.name);
        if (!enemy) {
            enemy = createUnitFromTemplate(enemyTemplate.name, UnitOwner::EnemyCtrl);
            m_units.append(enemy);
        }

        const int roundOffset = qMax(0, round - 1);
        const int eliteHp = eliteRound ? enemyTemplate.eliteMaxHpBonus : 0;
        const int eliteAtk = eliteRound ? enemyTemplate.eliteAtkBonus : 0;
        enemy->setMaxHp(enemyTemplate.baseMaxHp + roundOffset * enemyTemplate.hpGrowth + eliteHp);
        enemy->setHp(enemy->maxHp());
        enemy->setAtk(enemyTemplate.baseAtk + roundOffset * enemyTemplate.atkGrowth + eliteAtk);
        enemy->setMana(0);
    }
}

Unit* Game::findUnitById(int unitId) const
{
    for (Unit* unit : m_units) {
        if (unit && unit->id() == unitId) {
            return unit;
        }
    }
    return nullptr;
}

GridItem* Game::findGridItem(const QPoint& gridPos) const
{
    for (GridItem* item : m_gridItems) {
        if (item && item->gridPos() == gridPos) {
            return item;
        }
    }
    return nullptr;
}

UnitItem* Game::findUnitItem(int unitId) const
{
    auto it = m_unitItemById.find(unitId);
    if (it == m_unitItemById.end()) {
        return nullptr;
    }
    return it->second;
}

int Game::benchIndexOf(Unit* unit) const
{
    if (!unit) {
        return -1;
    }

    for (int i = 0; i < m_benchSlots.size(); ++i) {
        if (m_benchSlots.at(i) == unit) {
            return i;
        }
    }

    return -1;
}

Unit* Game::unitAtGrid(const QPoint& gridPos) const
{
    if (isBoardPosition(gridPos)) {
        return m_board.getUnitAt(gridPos);
    }

    if (isBenchPosition(gridPos)) {
        return m_benchSlots.at(gridPos.x());
    }

    return nullptr;
}

int Game::playerBoardUnitCount() const
{
    int count = 0;
    for (Unit* unit : m_units) {
        if (!unit || unit->owner() != UnitOwner::PlayerCtrl) {
            continue;
        }

        const QPoint pos = unit->position();
        if (m_board.isValidPosition(pos) && m_board.getUnitAt(pos) == unit) {
            ++count;
        }
    }

    return count;
}

void Game::clearGridHighlights()
{
    for (GridItem* item : m_gridItems) {
        if (!item) {
            continue;
        }
        item->setHoverActive(false);
        item->setDropActive(false);
    }
}

void Game::showDropHints(int unitId, const QPoint& source, const QPoint& hoverTarget)
{
    clearGridHighlights();

    for (GridItem* item : m_gridItems) {
        if (!item) {
            continue;
        }

        if (canApplyDrop(unitId, source, item->gridPos())) {
            item->setDropActive(true);
        }
    }

    GridItem* hovered = findGridItem(hoverTarget);
    if (hovered) {
        hovered->setHoverActive(true);
    }
}

bool Game::isBoardPosition(const QPoint& gridPos) const
{
    return m_board.isValidPosition(gridPos);
}

bool Game::isBenchPosition(const QPoint& gridPos) const
{
    return gridPos.y() == Board::ROWS && gridPos.x() >= 0 && gridPos.x() < m_benchSlotCount;
}

bool Game::canApplyDrop(int unitId, const QPoint& source, const QPoint& target) const
{
    if (m_phase != GamePhase::Prepare) {
        return false;
    }

    Unit* unit = findUnitById(unitId);
    if (!unit || unit->owner() != UnitOwner::PlayerCtrl) {
        return false;
    }

    if (source == target || unitAtGrid(source) != unit) {
        return false;
    }

    if (isBoardPosition(target)) {
        if (!m_board.isPlayerHalf(target) || m_board.hasUnitAt(target)) {
            return false;
        }

        if (isBenchPosition(source) && playerBoardUnitCount() >= m_player.populationLimit()) {
            return false;
        }

        return true;
    }

    if (isBenchPosition(target)) {
        return m_benchSlots.at(target.x()) == nullptr;
    }

    return false;
}

void Game::applyDrop(int unitId, const QPoint& source, const QPoint& target)
{
    Unit* unit = findUnitById(unitId);
    if (!unit) {
        return;
    }

    if (isBoardPosition(source)) {
        m_board.removeUnit(unit);
    } else if (isBenchPosition(source)) {
        m_benchSlots[source.x()] = nullptr;
    }

    if (isBoardPosition(target)) {
        m_board.addUnit(unit, target);
    } else if (isBenchPosition(target)) {
        m_benchSlots[target.x()] = unit;
        unit->setPosition(target);
    }
}

void Game::updateCombat()
{
    if (m_phase != GamePhase::Combat) {
        return;
    }

    for (Unit* unit : m_units) {
        if (!unit) {
            continue;
        }

        unit->setAttackCooldown(qMax(0, unit->attackCooldown() - 1));
        unit->setMoveCooldown(qMax(0, unit->moveCooldown() - 1));

        if (!unit->isAlive() && unit->state() != UnitState::Dead) {
            unit->setState(UnitState::Dead);
        }
    }

    for (Unit* unit : m_units) {
        if (!unit || !unit->isAlive()) {
            continue;
        }

        const QPoint pos = unit->position();
        if (!m_board.isValidPosition(pos) || m_board.getUnitAt(pos) != unit) {
            continue;
        }

        Unit* target = nearestEnemyFor(unit);
        if (!target) {
            unit->setState(UnitState::Idle);
            continue;
        }

        if (unit->mana() >= unit->maxMana()) {
            castSkill(unit, target);
            continue;
        }

        if (gridDistance(unit, target) <= unit->range()) {
            attackTarget(unit, target);
        } else {
            moveUnitToward(unit, target);
        }
    }

    // Deferred death processing: remove dead units from the board
    for (Unit* unit : m_units) {
        if (unit && unit->state() == UnitState::Dead) {
            const QPoint pos = unit->position();
            if (m_board.isValidPosition(pos) && m_board.getUnitAt(pos) == unit) {
                m_board.removeUnit(unit);
            }
        }
    }

    syncFromBoard();

    if (sideDefeated(UnitOwner::EnemyCtrl)) {
        finishCombat(true);
    } else if (sideDefeated(UnitOwner::PlayerCtrl)) {
        finishCombat(false);
    }
}

Unit* Game::nearestEnemyFor(Unit* unit) const
{
    if (!unit || !unit->isAlive()) {
        return nullptr;
    }

    Unit* best = nullptr;
    int bestDistance = std::numeric_limits<int>::max();
    int bestHp = std::numeric_limits<int>::max();

    for (Unit* candidate : m_units) {
        if (!candidate || !candidate->isAlive() || candidate->owner() == unit->owner()) {
            continue;
        }

        const QPoint candidatePos = candidate->position();
        if (!m_board.isValidPosition(candidatePos) || m_board.getUnitAt(candidatePos) != candidate) {
            continue;
        }

        const int distance = gridDistance(unit, candidate);
        if (distance < bestDistance || (distance == bestDistance && candidate->hp() < bestHp) ||
            (distance == bestDistance && candidate->hp() == bestHp &&
             candidate->position().y() < (best ? best->position().y() : Board::ROWS)) ||
            (distance == bestDistance && candidate->hp() == bestHp && best &&
             candidate->position().y() == best->position().y() && candidate->position().x() < best->position().x())) {
            best = candidate;
            bestDistance = distance;
            bestHp = candidate->hp();
        }
    }

    return best;
}

int Game::gridDistance(Unit* a, Unit* b) const
{
    if (!a || !b) {
        return std::numeric_limits<int>::max();
    }

    return gridDistance(a->position(), b->position());
}

int Game::gridDistance(const QPoint& a, const QPoint& b) const
{
    return qAbs(a.x() - b.x()) + qAbs(a.y() - b.y());
}

QPoint Game::nextStepToward(Unit* unit, Unit* target) const
{
    if (!unit || !target) {
        return QPoint(-1, -1);
    }

    const QPoint from = unit->position();
    const QPoint targetPos = target->position();
    if (!m_board.isValidPosition(from) || !m_board.isValidPosition(targetPos)) {
        return from;
    }
    if (gridDistance(from, targetPos) <= unit->range()) {
        return from;
    }

    auto indexOf = [](const QPoint& pos) { return pos.y() * Board::COLS + pos.x(); };

    QVector<int> previous(Board::ROWS * Board::COLS, -1);
    QVector<bool> visited(Board::ROWS * Board::COLS, false);
    QVector<QPoint> queue;
    queue.reserve(Board::ROWS * Board::COLS);
    queue.append(from);
    visited[indexOf(from)] = true;

    const std::array<QPoint, 4> directions = {QPoint(1, 0), QPoint(-1, 0), QPoint(0, 1), QPoint(0, -1)};

    QPoint bestReachable = from;
    int bestDistance = gridDistance(from, targetPos);
    int foundIndex = -1;

    for (int head = 0; head < queue.size(); ++head) {
        const QPoint current = queue.at(head);
        const int currentDistance = gridDistance(current, targetPos);
        if (current != from && currentDistance < bestDistance) {
            bestReachable = current;
            bestDistance = currentDistance;
        }
        if (current != from && currentDistance <= unit->range()) {
            foundIndex = indexOf(current);
            break;
        }

        QVector<QPoint> orderedNeighbors;
        orderedNeighbors.reserve(4);
        for (const QPoint& direction : directions) {
            orderedNeighbors.append(current + direction);
        }
        std::sort(orderedNeighbors.begin(), orderedNeighbors.end(),
                  [this, targetPos](const QPoint& a, const QPoint& b) {
                      return gridDistance(a, targetPos) < gridDistance(b, targetPos);
                  });

        for (const QPoint& next : orderedNeighbors) {
            if (!m_board.isValidPosition(next)) {
                continue;
            }

            const int nextIndex = indexOf(next);
            if (visited[nextIndex] || m_board.hasUnitAt(next)) {
                continue;
            }

            visited[nextIndex] = true;
            previous[nextIndex] = indexOf(current);
            queue.append(next);
        }
    }

    int destination = foundIndex >= 0 ? foundIndex : indexOf(bestReachable);
    if (destination == indexOf(from)) {
        return from;
    }

    while (previous[destination] != indexOf(from) && previous[destination] >= 0) {
        destination = previous[destination];
    }

    return QPoint(destination % Board::COLS, destination / Board::COLS);
}

void Game::moveUnitToward(Unit* unit, Unit* target)
{
    if (!unit || !target || unit->moveCooldown() > 0) {
        return;
    }

    const QPoint from = unit->position();
    const QPoint next = nextStepToward(unit, target);
    if (next == from) {
        unit->setState(UnitState::Idle);
        unit->setMoveCooldown(GameConstants::kMoveCooldown);
        return;
    }

    m_board.removeUnit(unit);
    m_board.addUnit(unit, next);
    unit->setState(UnitState::Moving);
    unit->setMoveCooldown(GameConstants::kMoveCooldown);

    // Smooth movement animation
    {
        UnitItem* moverItem = findUnitItem(unit->id());
        if (moverItem) {
            moverItem->setGridPos(next);
            const QPointF targetWorld = gridToWorld(next.y(), next.x());
            moverItem->animateMoveTo(targetWorld, 200);
        }
    }
}

void Game::attackTarget(Unit* unit, Unit* target)
{
    if (!unit || !target || unit->attackCooldown() > 0) {
        if (unit) {
            unit->setState(UnitState::Idle);
        }
        return;
    }

    unit->setState(UnitState::Attacking);
    unit->setAttackCooldown(unit->attackInterval());
    unit->setMana(qMin(unit->maxMana(), unit->mana() + GameConstants::kManaGainPerAttack + unit->traitManaGainBonus()));

    // Visual feedback: attacker flash & target damage flash
    {
        UnitItem* attackerItem = findUnitItem(unit->id());
        if (attackerItem) attackerItem->flashAttack();
    }

    applyDamage(target, unit->atk());
    addLog(QStringLiteral("%1攻击%2，造成%3伤害。").arg(unit->name(), target->name()).arg(unit->atk()));

    if (target->isAlive() && unit->traitExtraStrikeChance() > 0 &&
        QRandomGenerator::global()->bounded(100) < unit->traitExtraStrikeChance()) {
        const int bonusDamage = qMax(1, unit->atk() / 2);
        applyDamage(target, bonusDamage);
        addLog(QStringLiteral("%1触发游侠连击，追加%2伤害。").arg(unit->name()).arg(bonusDamage));
    }
}

void Game::castSkill(Unit* unit, Unit* target)
{
    if (!unit || !target || !unit->skill()) {
        return;
    }

    unit->setState(UnitState::Casting);
    unit->setMana(0);
    unit->setAttackCooldown(unit->attackInterval());

    // Visual feedback: caster flashes bright on skill activation
    {
        UnitItem* casterItem = findUnitItem(unit->id());
        if (casterItem) {
            casterItem->flashAttack();
        }
    }

    // Flash callback for skills
    auto flashFn = [this](Unit* u) {
        UnitItem* ui = findUnitItem(u->id());
        if (ui) ui->flashAttack();
    };

    unit->skill()->cast(
        unit, target, m_units, [this](Unit* damagedUnit, int damage) { applyDamage(damagedUnit, damage); },
        [this](const QPoint& a, const QPoint& b) { return gridDistance(a, b); },
        [this](const QString& message) { addLog(message); },
        flashFn);
}

void Game::applyDamage(Unit* target, int damage)
{
    if (!target || !target->isAlive()) {
        return;
    }

    target->setHp(qMax(0, target->hp() - damage));

    // Visual damage flash on target
    {
        UnitItem* targetItem = findUnitItem(target->id());
        if (targetItem) targetItem->flashDamage();
    }

    if (target->hp() <= 0) {
        target->setState(UnitState::Dead);
        addLog(QStringLiteral("%1阵亡。").arg(target->name()));
    }
}

bool Game::sideDefeated(UnitOwner owner) const
{
    for (Unit* unit : m_units) {
        if (!unit || unit->owner() != owner || !unit->isAlive()) {
            continue;
        }

        const QPoint pos = unit->position();
        if (m_board.isValidPosition(pos) && m_board.getUnitAt(pos) == unit) {
            return false;
        }
    }

    return true;
}

int Game::interestGold() const
{
    return qMin(GameConstants::kInterestMax, m_player.gold() / GameConstants::kInterestDivisor);
}

int Game::streakBonusGold(bool playerWon) const
{
    const int streak = playerWon ? m_player.winStreak() : m_player.lossStreak();
    if (streak < GameConstants::kStreakThreshold) {
        return 0;
    }
    return qMin(playerWon ? GameConstants::kWinStreakBonusCap : GameConstants::kLossStreakBonusCap, streak - 1);
}

void Game::finishCombat(bool playerWon)
{
    m_combatTimer->stop();

    const int interest = interestGold();
    Equipment reward;

    if (playerWon) {
        m_player.setWinStreak(m_player.winStreak() + 1);
        m_player.setLossStreak(0);
        const int streakBonus = streakBonusGold(true);
        reward = randomEquipment();
        m_player.setGold(m_player.gold() + GameConstants::kVictoryGold + interest + streakBonus);
        m_equipmentPool.append(reward);
        m_lastResult = QStringLiteral("胜利！基础+%1，利息+%2，连胜+%3，掉落%4。")
                           .arg(GameConstants::kVictoryGold)
                           .arg(interest)
                           .arg(streakBonus)
                           .arg(reward.name());
        addLog(m_lastResult);
        unlockAchievement(QStringLiteral("初战告捷"));
        showResultOverlay(true);
    } else {
        m_player.setLossStreak(m_player.lossStreak() + 1);
        m_player.setWinStreak(0);
        const int streakBonus = streakBonusGold(false);
        m_player.setGold(m_player.gold() + GameConstants::kLossGold + interest + streakBonus);
        m_player.setHp(qMax(0, m_player.hp() - GameConstants::kHpLossOnDefeat));
        m_lastResult = QStringLiteral("失败，生命-10，基础+%1，利息+%2，连败补偿+%3。")
                           .arg(GameConstants::kLossGold)
                           .arg(interest)
                           .arg(streakBonus);
        addLog(m_lastResult);
        showResultOverlay(false);
    }

    checkAchievements();
    syncFromBoard();
}

void Game::showResultOverlay(bool playerWon)
{
    m_phase = GamePhase::PostCombat;

    if (!m_resultOverlay) {
        const QRectF sceneRect = m_scene->sceneRect();
        const QRectF overlayRect(sceneRect.left(), sceneRect.top(),
                                 sceneRect.width(), sceneRect.height());
        m_resultOverlay = m_scene->addRect(overlayRect,
            QPen(Qt::NoPen), QBrush(QColor(0, 0, 0, 150)));
        m_resultOverlay->setZValue(5.0);

        QFont resultFont;
        resultFont.setPointSize(36);
        resultFont.setBold(true);
        m_resultText = m_scene->addText(QString(), resultFont);
        m_resultText->setZValue(5.1);
    } else {
        m_resultOverlay->setVisible(true);
        m_resultText->setVisible(true);
    }

    // Build multi-line result display
    const int interest = interestGold();
    const int streakBonus = streakBonusGold(playerWon);
    QStringList lines;
    if (playerWon) {
        lines << QStringLiteral("★ 胜利! ★");
        lines << QStringLiteral("");
        lines << QStringLiteral("基础奖励  +%1").arg(GameConstants::kVictoryGold);
        if (interest > 0) lines << QStringLiteral("利息       +%1").arg(interest);
        if (streakBonus > 0) lines << QStringLiteral("连胜奖金  +%1").arg(streakBonus);
        lines << QStringLiteral("装备掉落  %1").arg(randomEquipment().name());
    } else {
        lines << QStringLiteral("● 失败");
        lines << QStringLiteral("");
        lines << QStringLiteral("生命 -%1").arg(GameConstants::kHpLossOnDefeat);
        lines << QStringLiteral("基础金币  +%1").arg(GameConstants::kLossGold);
        if (interest > 0) lines << QStringLiteral("利息       +%1").arg(interest);
        if (streakBonus > 0) lines << QStringLiteral("连败补偿  +%1").arg(streakBonus);
    }

    const QColor titleColor = playerWon ? QColor(255, 218, 107) : QColor(230, 92, 92);
    const QColor detailColor(220, 220, 220);

    QString html = QStringLiteral("<div style='text-align:center; font-size:36pt; font-weight:bold; color:%1;'>%2</div>")
        .arg(titleColor.name(), lines.at(0).toHtmlEscaped());
    for (int i = 2; i < lines.size(); ++i) {
        html += QStringLiteral("<div style='text-align:center; color:%1; font-size:16pt; margin-top:2px;'>%2</div>")
            .arg(detailColor.name(), lines.at(i).toHtmlEscaped());
    }

    m_resultText->setHtml(html);
    m_resultText->setTextWidth(0); // auto-width
    const QRectF sceneRect = m_scene->sceneRect();
    const QRectF textRect = m_resultText->boundingRect();
    m_resultText->setPos(sceneRect.center().x() - textRect.width() / 2,
                         sceneRect.center().y() - textRect.height() / 2);

    // Auto-dismiss after 2.5s
    if (!m_resultTimer) {
        m_resultTimer = new QTimer(this);
        m_resultTimer->setSingleShot(true);
        connect(m_resultTimer, &QTimer::timeout, this, &Game::dismissResultOverlay);
    }
    m_resultTimer->start(2500);
}

void Game::dismissResultOverlay()
{
    if (m_resultOverlay) {
        m_resultOverlay->setVisible(false);
        m_resultText->setVisible(false);
    }

    // Now actually advance the game state
    // (the finishCombat logic that was deferred)
    const bool playerWon = m_player.winStreak() > 0;

    if (playerWon) {
        m_player.setCurrentRound(m_player.currentRound() + 1);
        rollShop();
    }
    m_phase = GamePhase::Prepare;
    setupRoundBoard(true);
    syncFromBoard();
}

QString Game::currentEventForRound(int round) const
{
    return roundEventForRound(round).description;
}

void Game::updateRoundEvent()
{
    m_currentEvent = currentEventForRound(m_player.currentRound());
    if (m_currentEvent == QStringLiteral("无") || m_eventRewardRound == m_player.currentRound()) {
        return;
    }

    m_eventRewardRound = m_player.currentRound();
    addLog(QStringLiteral("触发事件：%1").arg(m_currentEvent));

    const RoundEvent event = roundEventForRound(m_player.currentRound());
    if (event.goldBonus > 0) {
        m_player.setGold(m_player.gold() + event.goldBonus);
        addLog(QStringLiteral("事件奖励：金币+%1。").arg(event.goldBonus));
        checkAchievements();
    }
}

QString Game::saveFileName(int slot) const
{
    return QStringLiteral("savegame_%1.json").arg(qBound(1, slot, 3));
}

QString Game::legacySaveFileName(int slot) const
{
    return QStringLiteral("savegame_%1.txt").arg(qBound(1, slot, 3));
}

void Game::addLog(const QString& message, LogCategory category)
{
    if (message.isEmpty()) {
        return;
    }

    const LogCategory inferredCategory = inferLogCategory(message);
    m_logs.prepend({message, category == LogCategory::System ? inferredCategory : category});
    while (m_logs.size() > GameConstants::kMaxLogCount) {
        m_logs.removeLast();
    }
    updateInfoPanel();
}

void Game::unlockAchievement(const QString& name)
{
    if (name.isEmpty() || m_achievements.contains(name)) {
        return;
    }

    m_achievements.append(name);
    addLog(QStringLiteral("解锁成就：%1。").arg(name));
}

void Game::checkAchievements()
{
    if (m_player.gold() >= GameConstants::kGoldSaveThreshold) {
        unlockAchievement(QStringLiteral("小有积蓄"));
    }
    if (m_player.level() >= GameConstants::kLevelUpThreshold) {
        unlockAchievement(QStringLiteral("扩编成军"));
    }
    if (m_player.winStreak() >= GameConstants::kStreakAchievementThreshold) {
        unlockAchievement(QStringLiteral("连胜经济"));
    }
    if (m_player.lossStreak() >= GameConstants::kStreakAchievementThreshold) {
        unlockAchievement(QStringLiteral("韧性经营"));
    }

    for (Unit* unit : m_units) {
        if (!unit || unit->owner() != UnitOwner::PlayerCtrl) {
            continue;
        }
        if (unit->starLevel() >= 2) {
            unlockAchievement(QStringLiteral("初次升星"));
        }
        if (!unit->equipmentNames().isEmpty()) {
            unlockAchievement(QStringLiteral("装备上身"));
        }
    }
}

QString Game::phaseName() const
{
    switch (m_phase) {
    case GamePhase::Prepare:
        return QStringLiteral("准备");
    case GamePhase::PreCombat:
        return QStringLiteral("准备战斗");
    case GamePhase::Combat:
        return QStringLiteral("战斗");
    case GamePhase::PostCombat:
        return QStringLiteral("战斗结束");
    case GamePhase::Resolve:
        return QStringLiteral("结算");
    }
    return QStringLiteral("未知");
}

QString Game::stateName(UnitState state) const
{
    switch (state) {
    case UnitState::Idle:
        return QStringLiteral("待机");
    case UnitState::Moving:
        return QStringLiteral("移动");
    case UnitState::Attacking:
        return QStringLiteral("攻击");
    case UnitState::Casting:
        return QStringLiteral("施法");
    case UnitState::Dead:
        return QStringLiteral("死亡");
    }
    return QStringLiteral("未知");
}

QString Game::skillName(SkillType skillType) const
{
    std::unique_ptr<Skill> skill = createSkill(skillType);
    return skill->name();
}

void Game::buildScene()
{
    m_scene->clear();
    m_leftInfoPanel = nullptr;
    m_infoPanel = nullptr;
    m_sellZoneItem = nullptr;
    m_sellZoneText = nullptr;
    m_gridItems.clear();
    m_unitItems.clear();
    m_unitItemById.clear();

    QRectF totalBounds;
    bool first = true;
    for (int row = 0; row < Board::ROWS; ++row) {
        for (int col = 0; col < Board::COLS; ++col) {
            GridItem* gridItem = new GridItem(row, col, cellRectPolygon(row, col));
            gridItem->setZValue(kZGrid);
            gridItem->setBaseColor(row < Board::ROWS / 2 ? QColor(80, 60, 60) : QColor(60, 60, 80));

            m_scene->addItem(gridItem);
            m_gridItems.push_back(gridItem);

            const QRectF bounds = gridItem->boundingRect();
            totalBounds = first ? bounds : totalBounds.united(bounds);
            first = false;
        }
    }

    for (int slot = 0; slot < m_benchSlotCount; ++slot) {
        GridItem* benchItem = new GridItem(Board::ROWS, slot, benchCellPolygon(slot));
        benchItem->setZValue(kZGrid);
        benchItem->setBaseColor(QColor(58, 78, 62));

        m_scene->addItem(benchItem);
        m_gridItems.push_back(benchItem);

        const QRectF bounds = benchItem->boundingRect();
        totalBounds = first ? bounds : totalBounds.united(bounds);
        first = false;
    }

    QFont labelFont;
    labelFont.setPointSize(10);
    labelFont.setBold(true);

    QGraphicsTextItem* enemyLabel = m_scene->addText(QStringLiteral("敌方半场"), labelFont);
    enemyLabel->setDefaultTextColor(QColor(230, 170, 170));
    enemyLabel->setZValue(kZGrid + 0.1);
    enemyLabel->setPos(0, -34);

    QGraphicsTextItem* playerLabel = m_scene->addText(QStringLiteral("玩家半场"), labelFont);
    playerLabel->setDefaultTextColor(QColor(170, 190, 255));
    playerLabel->setZValue(kZGrid + 0.1);
    playerLabel->setPos(0, gridToWorld(Board::ROWS / 2, 0).y() - 34);

    QGraphicsTextItem* benchLabel = m_scene->addText(QStringLiteral("备战区"), labelFont);
    benchLabel->setDefaultTextColor(QColor(170, 230, 180));
    benchLabel->setZValue(kZGrid + 0.1);
    benchLabel->setPos(0, benchToWorld(0).y() - 54);

    const QRectF leftPanelRect(-320.0, -40.0, 290.0, 360.0);
    QGraphicsRectItem* leftPanelBack =
        m_scene->addRect(leftPanelRect, QPen(QColor(88, 92, 100), 1), QBrush(QColor(29, 31, 36, 232)));
    leftPanelBack->setZValue(kZGrid + 0.05);

    QFont panelFont = labelFont;
    panelFont.setPointSize(9);
    panelFont.setBold(false);
    m_leftInfoPanel = m_scene->addText(QString(), panelFont);
    m_leftInfoPanel->setDefaultTextColor(QColor(235, 235, 235));
    m_leftInfoPanel->setTextWidth(leftPanelRect.width() - 24.0);
    m_leftInfoPanel->setZValue(kZGrid + 0.1);
    m_leftInfoPanel->setPos(leftPanelRect.left() + 12.0, leftPanelRect.top() + 10.0);

    const qreal panelX = gridToWorld(0, Board::COLS - 1).x() + 96.0;
    const QRectF panelRect(panelX, -40.0, 400.0, 700.0);
    QGraphicsRectItem* panelBack =
        m_scene->addRect(panelRect, QPen(QColor(88, 92, 100), 1), QBrush(QColor(29, 31, 36, 232)));
    panelBack->setZValue(kZGrid + 0.05);

    m_infoPanel = m_scene->addText(QString(), panelFont);
    m_infoPanel->setDefaultTextColor(QColor(235, 235, 235));
    m_infoPanel->setTextWidth(panelRect.width() - 24.0);
    m_infoPanel->setZValue(kZGrid + 0.1);
    m_infoPanel->setPos(panelRect.left() + 12.0, panelRect.top() + 10.0);
    updateInfoPanel();
    totalBounds = totalBounds.united(leftPanelRect).united(panelRect);

    for (Unit* unit : m_units) {
        UnitItem* unitItem = new UnitItem(unit);
        unitItem->setZValue(kZUnit);
        m_scene->addItem(unitItem);
        m_unitItems.push_back(unitItem);
        m_unitItemById[unit->id()] = unitItem;

        connect(unitItem, &UnitItem::unitSelected, this, &Game::handleUnitSelected);
        connect(unitItem, &UnitItem::dragStarted, this, &Game::handleDragStarted);
        connect(unitItem, &UnitItem::dragMoved, this, &Game::handleDragMoved);
        connect(unitItem, &UnitItem::dragDropped, this, &Game::handleDropCommand);
    }

    // Sell zone: drag-to-sell area aligned with left panel
    {
        const qreal pitch = m_cellSize + m_cellGap;
        const qreal zoneY = leftPanelRect.bottom() + 8 + 3.0 * pitch;
        m_sellZoneRect = QRectF(leftPanelRect.left(), zoneY, leftPanelRect.width(), 2.0 * pitch);
        QPen sellPen(QColor(180, 60, 60), 2);
        QBrush sellBrush(QColor(60, 25, 25, 180));
        m_sellZoneItem = m_scene->addRect(m_sellZoneRect, sellPen, sellBrush);
        m_sellZoneItem->setZValue(kZGrid + 0.05);

        QFont sellFont;
        sellFont.setPointSize(10);
        sellFont.setBold(false);
        m_sellZoneText = m_scene->addText(QStringLiteral("将角色拖动到此处来出售"), sellFont);
        m_sellZoneText->setDefaultTextColor(QColor(200, 100, 100));
        const QRectF textRect = m_sellZoneText->boundingRect();
        m_sellZoneText->setPos(m_sellZoneRect.center().x() - textRect.width() / 2,
                         m_sellZoneRect.center().y() - textRect.height() / 2);
        m_sellZoneText->setZValue(kZGrid + 0.1);
        totalBounds = totalBounds.united(m_sellZoneRect);
    }

    m_scene->setSceneRect(totalBounds.adjusted(-40, -40, 40, 40));
}

void Game::syncFromBoard()
{
    clearGridHighlights();

    for (UnitItem* item : m_unitItems) {
        if (!item || !item->unit()) {
            continue;
        }

        item->setDragEnabled(m_phase == GamePhase::Prepare && item->unit()->owner() == UnitOwner::PlayerCtrl);
        item->setSelectedActive(item->unitId() == m_selectedUnitId);

        const QPoint pos = item->unit()->position();
        if (!m_board.isValidPosition(pos) || m_board.getUnitAt(pos) != item->unit()) {
            if (item->unit()->state() == UnitState::Dead && m_board.isValidPosition(pos)) {
                if (item->deathAnimationFinished()) {
                    item->setVisible(false);
                    continue;
                }

                item->setVisible(true);
                item->setGridPos(pos);
                item->setPos(gridToWorld(pos.y(), pos.x()));
                item->setZValue(kZUnit - 0.1);
                continue;
            }

            const int benchSlot = benchIndexOf(item->unit());
            if (benchSlot < 0) {
                item->setVisible(false);
                continue;
            }

            item->setVisible(true);
            item->setGridPos(QPoint(benchSlot, Board::ROWS));
            item->setPos(benchToWorld(benchSlot));
            item->setZValue(kZUnit);
            continue;
        }

        item->setVisible(true);
        item->setGridPos(pos);
        item->setPos(gridToWorld(pos.y(), pos.x()));
        item->setZValue(kZUnit);
    }

    updateInfoPanel();
}

void Game::updateInfoPanel()
{
    if (!m_leftInfoPanel || !m_infoPanel) {
        return;
    }

    auto safe = [](const QString& value) { return value.toHtmlEscaped(); };
    auto badge = [](const QString& text, const QString& color) {
        return QStringLiteral("<span style='color:%1; font-weight:700;'>%2</span>").arg(color, text.toHtmlEscaped());
    };
    auto metric = [](const QString& label, const QString& value, const QString& color) {
        return QStringLiteral("<td width='50%' style='padding:3px 5px;'>"
                              "<span style='color:#9fa6b2;'>%1</span><br/>"
                              "<span style='color:%3; font-size:12pt; font-weight:700;'>%2</span>"
                              "</td>")
            .arg(label.toHtmlEscaped(), value.toHtmlEscaped(), color);
    };
    auto section = [](const QString& title, const QString& body, const QString& accentColor) {
        return QStringLiteral(
                   "<table width='100%' cellspacing='0' cellpadding='0' "
                   "style='margin-top:7px; border:1px solid #434852;'>"
                   "<tr><td bgcolor='#30343c' style='padding:4px 6px; color:%3; font-weight:700;'>%1</td></tr>"
                   "<tr><td bgcolor='#24272d' style='padding:6px;'>%2</td></tr>"
                   "</table>")
            .arg(title.toHtmlEscaped(), body, accentColor);
    };

    const Unit* selected = findUnitById(m_selectedUnitId);
    QString selectedHtml = QStringLiteral("<span style='color:#8f96a3;'>点击一个己方单位来选中（装备/出售）</span>");

    if (selected) {
        const QString ownerText =
            selected->owner() == UnitOwner::PlayerCtrl ? QStringLiteral("己方") : QStringLiteral("敌方");
        const QString ownerColor =
            selected->owner() == UnitOwner::PlayerCtrl ? QStringLiteral("#8fb3ff") : QStringLiteral("#ff918f");
        QStringList bonusParts;
        if (selected->traitSkillAmpPercent() > 0) {
            bonusParts << badge(QStringLiteral("技能+%1%").arg(selected->traitSkillAmpPercent()),
                                QStringLiteral("#d9b8ff"));
        }
        if (selected->traitExtraStrikeChance() > 0) {
            bonusParts << badge(QStringLiteral("连击%1%").arg(selected->traitExtraStrikeChance()),
                                QStringLiteral("#ffd36a"));
        }
        if (selected->traitManaGainBonus() > 0) {
            bonusParts << badge(QStringLiteral("回蓝+%1").arg(selected->traitManaGainBonus()),
                                QStringLiteral("#7dc7ff"));
        }

        QStringList selectedRows;
        selectedRows << QStringLiteral("<tr>%1%2</tr>")
                            .arg(metric(QStringLiteral("生命"),
                                        QStringLiteral("%1/%2").arg(selected->hp()).arg(selected->maxHp()),
                                        QStringLiteral("#80d98f")),
                                 metric(QStringLiteral("法力"),
                                        QStringLiteral("%1/%2").arg(selected->mana()).arg(selected->maxMana()),
                                        QStringLiteral("#78b9ff")));
        selectedRows << QStringLiteral("<tr>%1%2</tr>")
                            .arg(metric(QStringLiteral("攻击"), QString::number(selected->atk()),
                                        QStringLiteral("#ffd36a")),
                                 metric(QStringLiteral("射程"), QString::number(selected->range()),
                                        QStringLiteral("#d8dce4")));

        selectedHtml =
            QStringLiteral("<div style='font-size:11pt; font-weight:700;'>%1 "
                           "<span style='color:%2; font-size:9pt;'>%3</span></div>"
                           "<div style='color:#aeb5c1; margin-top:2px;'>%4星 · %5 · %6</div>"
                           "<table width='100%' cellspacing='0' cellpadding='0' style='margin-top:5px;'>%7</table>"
                           "<div style='margin-top:4px; color:#aeb5c1;'>羁绊：%8</div>"
                           "<div style='margin-top:2px; color:#aeb5c1;'>加成：%9</div>"
                           "<div style='margin-top:2px; color:#aeb5c1;'>装备：%10</div>")
                .arg(safe(selected->name()), ownerColor, safe(ownerText), QString::number(selected->starLevel()),
                     safe(stateName(selected->state())), safe(skillName(selected->skillType())),
                     selectedRows.join(QString()), safe(selected->traits().join(QStringLiteral("，"))),
                     bonusParts.isEmpty() ? QStringLiteral("<span style='color:#8f96a3;'>无</span>")
                                          : bonusParts.join(QStringLiteral("，")),
                     selected->equipmentNames().isEmpty()
                         ? QStringLiteral("<span style='color:#8f96a3;'>无</span>")
                         : safe(selected->equipmentNames().join(QStringLiteral("，"))));
    }

    int benchUsed = 0;
    for (Unit* unit : m_benchSlots) {
        if (unit) {
            ++benchUsed;
        }
    }

    QStringList shopRows;
    for (int i = 0; i < GameConstants::kShopSlotCount; ++i) {
        const QString unitName = m_shopSlots.at(i);
        const QString slotText =
            unitName.isEmpty()
                ? QStringLiteral("<span style='color:#8f96a3;'>已售出</span>")
               : QStringLiteral("<span style='color:#f2f4f8;'>%1</span> <span style='color:#ffd36a;'>%2金</span>")
                      .arg(safe(unitName))
                      .arg(GameConstants::kUnitCost);
        shopRows << QStringLiteral("<tr><td width='22' style='color:#9fa6b2;'>%1.</td>"
                                   "<td style='padding:2px 0;'>%2</td></tr>")
                        .arg(i + 1)
                        .arg(slotText);
    }

    QStringList equipmentLines;
    for (const Equipment& equipment : m_equipmentPool) {
        equipmentLines << QStringLiteral(
                              "%1 <span style='color:%2;'>[%3]</span> <span style='color:#9fa6b2;'>%4</span>")
                              .arg(safe(equipment.name()), equipment.rarityColor(), safe(equipment.rarity()),
                                   safe(equipment.description()));
    }
    QStringList visibleEquipment = equipmentLines.mid(0, 3);
    if (equipmentLines.size() > visibleEquipment.size()) {
        visibleEquipment << QStringLiteral("<span style='color:#8f96a3;'>还有%1件...</span>")
                                .arg(equipmentLines.size() - visibleEquipment.size());
    }

    QStringList visibleAchievements = m_achievements.mid(0, 4);
    for (QString& achievement : visibleAchievements) {
        achievement = safe(achievement);
    }
    if (m_achievements.size() > visibleAchievements.size()) {
        visibleAchievements << QStringLiteral("<span style='color:#8f96a3;'>还有%1项...</span>")
                                   .arg(m_achievements.size() - visibleAchievements.size());
    }
    const QString achievementsText = visibleAchievements.isEmpty()
                                         ? QStringLiteral("<span style='color:#8f96a3;'>无</span>")
                                         : visibleAchievements.join(QStringLiteral("，"));

    QStringList visibleLogs;
    const int visibleLogCount = qMin(5, m_logs.size());
    for (int i = 0; i < visibleLogCount; ++i) {
        const GameLog& log = m_logs.at(i);
        visibleLogs << QStringLiteral("<span style='color:%1; font-weight:700;'>[%2]</span> %3")
                           .arg(logCategoryColor(log.category), safe(logCategoryText(log.category)), safe(log.message));
    }
    const QString logsText = visibleLogs.isEmpty()
                                 ? QStringLiteral("<span style='color:#8f96a3;'>暂无</span>")
                                 : QStringLiteral("<div style='line-height:125%; color:#c4cad4;'>%1</div>")
                                       .arg(visibleLogs.join(QStringLiteral("<br/>")));

    const QString phaseColor =
        m_phase == GamePhase::Combat
            ? QStringLiteral("#e0a447")
            : (m_phase == GamePhase::PreCombat || m_phase == GamePhase::PostCombat
                   ? QStringLiteral("#a78bfa")
                   : (m_phase == GamePhase::Resolve ? QStringLiteral("#a78bfa") : QStringLiteral("#58c28d")));
    const QString resultColor = (m_phase == GamePhase::Combat || m_phase == GamePhase::PreCombat)
        ? QStringLiteral("#f1d18a") : QStringLiteral("#d9dde6");

    const QString globalBody =
        QStringLiteral("<table width='100%' cellspacing='0' cellpadding='0'>"
                       "<tr>%1%2</tr>"
                       "</table>"
                       "<div style='margin-top:5px; color:%3;'>%4</div>"
                       "<div style='margin-top:2px; color:#9fa6b2;'>事件：%5</div>")
            .arg(metric(QStringLiteral("阶段"), phaseName(), phaseColor),
                 metric(QStringLiteral("轮次"), QString::number(m_player.currentRound()), QStringLiteral("#d8dce4")),
                 resultColor, safe(m_lastResult), safe(m_currentEvent));

    const QString playerMetrics =
        QStringLiteral("<table width='100%' cellspacing='0' cellpadding='0'>"
                       "<tr>%1%2</tr>"
                       "<tr>%3%4</tr>"
                       "<tr>%5%6</tr>"
                       "</table>")
            .arg(metric(QStringLiteral("生命"), QString::number(m_player.hp()), QStringLiteral("#ff918f")),
                 metric(QStringLiteral("金币"), QString::number(m_player.gold()), QStringLiteral("#ffd36a")),
                 metric(QStringLiteral("等级"), QString::number(m_player.level()), QStringLiteral("#d8dce4")),
                 metric(QStringLiteral("轮次"), QString::number(m_player.currentRound()), QStringLiteral("#d8dce4")),
                 metric(QStringLiteral("上阵"),
                        QStringLiteral("%1/%2").arg(playerBoardUnitCount()).arg(m_player.populationLimit()),
                        QStringLiteral("#8fb3ff")),
                 metric(QStringLiteral("备战区"), QStringLiteral("%1/%2").arg(benchUsed).arg(m_benchSlotCount),
                        QStringLiteral("#8fd9a2")));

    const QString economyBody =
        QStringLiteral("<div style='color:#aeb5c1;'>利息 <span style='color:#ffd36a;'>+%1</span> · 连胜/连败 "
                       "<span style='color:#f2f4f8;'>%2/%3</span></div>"
                       "<table width='100%' cellspacing='0' cellpadding='0' style='margin-top:4px;'>%4</table>"
                       "<div style='margin-top:5px; color:#aeb5c1;'>羁绊（仅棋盘）：%5</div>"
                       "<div style='margin-top:2px; color:#aeb5c1;'>装备池：%6</div>"
                       "<div style='margin-top:2px; color:#aeb5c1;'>成就：%7</div>")
            .arg(interestGold())
            .arg(m_player.winStreak())
            .arg(m_player.lossStreak())
            .arg(shopRows.join(QString()))
            .arg(safe(activeTraitsText()))
            .arg(visibleEquipment.isEmpty() ? QStringLiteral("<span style='color:#8f96a3;'>无</span>")
                                            : visibleEquipment.join(QStringLiteral("，")))
            .arg(achievementsText);

    const QString leftHtml = QStringLiteral("<html><body style='font-family:\"Microsoft YaHei\",\"Segoe "
                                            "UI\",sans-serif; font-size:9pt; color:#eef1f6;'>"
                                            "%1"
                                            "%2"
                                            "</body></html>")
                                 .arg(section(QStringLiteral("全局"), globalBody, phaseColor),
                                      section(QStringLiteral("玩家状态"), playerMetrics, QStringLiteral("#8fb3ff")));

    const QString rightHtml = QStringLiteral("<html><body style='font-family:\"Microsoft YaHei\",\"Segoe "
                                             "UI\",sans-serif; font-size:9pt; color:#eef1f6;'>"
                                             "%1"
                                             "%2"
                                             "%3"
                                             "</body></html>")
                                  .arg(section(QStringLiteral("运营"), economyBody, QStringLiteral("#ffd36a")),
                                       section(QStringLiteral("最近日志"), logsText, QStringLiteral("#8fd9a2")),
                                       section(QStringLiteral("选中单位"), selectedHtml, QStringLiteral("#c8b6ff")));

    m_leftInfoPanel->setHtml(leftHtml);
    m_infoPanel->setHtml(rightHtml);
}

QPointF Game::gridToWorld(int row, int col) const
{
    const qreal pitch = m_cellSize + m_cellGap;
    const qreal x = col * pitch + m_cellSize * 0.5;
    const qreal y = row * pitch + m_cellSize * 0.5;
    return QPointF(x, y);
}

QPointF Game::benchToWorld(int slot) const
{
    const qreal pitch = m_cellSize + m_cellGap;
    const qreal x = slot * pitch + m_cellSize * 0.5;
    const qreal y = m_rows * pitch + m_benchGap + m_cellSize * 0.5;
    return QPointF(x, y);
}

QPoint Game::worldToGrid(const QPointF& world) const
{
    const qreal pitch = m_cellSize + m_cellGap;
    const int col = qFloor(world.x() / pitch);
    const int row = qFloor(world.y() / pitch);
    const QPoint gridPos(col, row);

    if (m_board.isValidPosition(gridPos)) {
        const QPointF center = gridToWorld(row, col);
        const QRectF cellRect(center.x() - m_cellSize * 0.5, center.y() - m_cellSize * 0.5, m_cellSize, m_cellSize);
        if (cellRect.contains(world)) {
            return gridPos;
        }
    }

    for (int slot = 0; slot < m_benchSlotCount; ++slot) {
        const QPointF center = benchToWorld(slot);
        const QRectF cellRect(center.x() - m_cellSize * 0.5, center.y() - m_cellSize * 0.5, m_cellSize, m_cellSize);
        if (cellRect.contains(world)) {
            return QPoint(slot, Board::ROWS);
        }
    }

    return QPoint(-1, -1);
}

QPolygonF Game::cellRectPolygon(int row, int col) const
{
    const QPointF center = gridToWorld(row, col);
    QPolygonF poly;
    poly.reserve(4);

    const qreal half = m_cellSize * 0.5;
    poly.append(QPointF(center.x() - half, center.y() - half));
    poly.append(QPointF(center.x() + half, center.y() - half));
    poly.append(QPointF(center.x() + half, center.y() + half));
    poly.append(QPointF(center.x() - half, center.y() + half));

    return poly;
}

QPolygonF Game::benchCellPolygon(int slot) const
{
    const QPointF center = benchToWorld(slot);
    QPolygonF poly;
    poly.reserve(4);

    const qreal half = m_cellSize * 0.5;
    poly.append(QPointF(center.x() - half, center.y() - half));
    poly.append(QPointF(center.x() + half, center.y() - half));
    poly.append(QPointF(center.x() + half, center.y() + half));
    poly.append(QPointF(center.x() - half, center.y() + half));

    return poly;
}
