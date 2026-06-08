#include "game.h"
#include "entity/unit.h"
#include "gui/griditem.h"
#include "gui/unititem.h"
#include <QFile>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QRandomGenerator>
#include <QTextStream>
#include <QTimer>
#include <QtMath>
#include <limits>

namespace {
constexpr qreal kZGrid = 0.0;
constexpr qreal kZUnit = 1.0;
constexpr qreal kZDraggingUnit = 2.0;
}

Game::Game(QObject* parent)
    : QObject(parent)
    , m_benchSlots(8, nullptr)
    , m_shopSlots(5)
    , m_scene(new QGraphicsScene(this))
    , m_infoPanel(nullptr)
    , m_combatTimer(new QTimer(this))
    , m_dragActive(false)
    , m_activeUnitId(-1)
    , m_selectedUnitId(-1)
    , m_sourceGrid(-1, -1)
    , m_phase(GamePhase::Prepare)
    , m_lastResult(QStringLiteral("请布置你的阵容。"))
    , m_currentEvent(QStringLiteral("无"))
    , m_eventRewardRound(0)
    , m_rows(Board::ROWS)
    , m_cols(Board::COLS)
    , m_benchSlotCount(8)
    , m_cellSize(64.0)
    , m_cellGap(4.0)
    , m_benchGap(52.0)
{
    m_combatTimer->setInterval(250);
    connect(m_combatTimer, &QTimer::timeout,
            this, &Game::updateCombat);
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
    m_player.setHp(100);
    m_player.setGold(10);
    m_player.setLevel(1);
    m_player.setPopulationLimit(4);
    m_player.setCurrentRound(1);
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

    m_phase = GamePhase::Combat;
    m_lastResult = QStringLiteral("战斗进行中。");
    addLog(QStringLiteral("第%1轮战斗开始。").arg(m_player.currentRound()));

    for (Unit* unit : m_units) {
        if (!unit) {
            continue;
        }

        if (unit->isAlive() && m_board.isValidPosition(unit->position()) && m_board.getUnitAt(unit->position()) == unit) {
            unit->setState(UnitState::Idle);
            unit->setAttackCooldown(0);
            unit->setMoveCooldown(0);
        }
    }

    syncFromBoard();
    m_combatTimer->start();
}

void Game::setupRoundBoard()
{
    m_board.clear();
    m_benchSlots.fill(nullptr, m_benchSlotCount);
    updateRoundEvent();
    generateEnemyRound(m_player.currentRound());

    const QPoint playerPositions[] = {
        QPoint(0, 7),
        QPoint(1, 7),
        QPoint(2, 7)
    };
    const QPoint enemyPositions[] = {
        QPoint(5, 0),
        QPoint(6, 0)
    };

    int deployedPlayers = 0;
    int benchSlot = 0;
    int deployedEnemies = 0;

    for (Unit* unit : m_units) {
        if (!unit) {
            continue;
        }

        unit->resetCombatState();

        if (unit->owner() == UnitOwner::EnemyCtrl) {
            if (deployedEnemies < 2) {
                m_board.addUnit(unit, enemyPositions[deployedEnemies]);
                ++deployedEnemies;
            }
            continue;
        }

        if (deployedPlayers < 3) {
            m_board.addUnit(unit, playerPositions[deployedPlayers]);
            ++deployedPlayers;
            continue;
        }

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
}

void Game::handleDragMoved(int unitId, const QPoint&, const QPointF& scenePos)
{
    if (!m_dragActive) {
        return;
    }

    clearGridHighlights();

    const QPoint target = worldToGrid(scenePos);
    GridItem* targetItem = findGridItem(target);
    if (!targetItem) {
        return;
    }

    targetItem->setHoverActive(true);

    if (canApplyDrop(unitId, m_sourceGrid, target)) {
        targetItem->setDropActive(true);
    }
}

void Game::handleDropCommand(int unitId, const QPoint&, const QPointF& scenePos)
{
    if (!m_dragActive) {
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

    constexpr int kUnitCost = 3;
    if (m_player.gold() < kUnitCost) {
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

    m_player.setGold(m_player.gold() - kUnitCost);
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

    constexpr int kRerollCost = 2;
    if (m_player.gold() < kRerollCost) {
        m_lastResult = QStringLiteral("金币不足，无法刷新商店。");
        updateInfoPanel();
        return;
    }

    m_player.setGold(m_player.gold() - kRerollCost);
    rollShop();
    m_lastResult = QStringLiteral("商店已刷新。");
    addLog(QStringLiteral("刷新商店，花费%1金币。").arg(kRerollCost));
    checkAchievements();
    updateInfoPanel();
}

void Game::levelUp()
{
    if (m_phase != GamePhase::Prepare) {
        return;
    }

    constexpr int kLevelCost = 6;
    if (m_player.level() >= 8) {
        m_lastResult = QStringLiteral("已经达到最高等级。");
        updateInfoPanel();
        return;
    }
    if (m_player.gold() < kLevelCost) {
        m_lastResult = QStringLiteral("金币不足，无法升级。");
        updateInfoPanel();
        return;
    }

    m_player.setGold(m_player.gold() - kLevelCost);
    m_player.setLevel(m_player.level() + 1);
    m_player.setPopulationLimit(qMin(8, m_player.populationLimit() + 1));
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

    if (unit->equipmentNames().size() >= 3) {
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

void Game::saveGame()
{
    QFile file(QStringLiteral("savegame.txt"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_lastResult = QStringLiteral("保存失败。");
        updateInfoPanel();
        return;
    }

    QTextStream out(&file);
    out << "PLAYER "
        << m_player.hp() << ' '
        << m_player.gold() << ' '
        << m_player.level() << ' '
        << m_player.populationLimit() << ' '
        << m_player.currentRound() << '\n';

    out << "SHOP";
    for (const QString& slot : m_shopSlots) {
        out << '|' << slot;
    }
    out << '\n';

    out << "EQUIPMENT";
    for (const Equipment& equipment : m_equipmentPool) {
        out << '|' << equipment.name();
    }
    out << '\n';

    out << "ACHIEVEMENTS";
    for (const QString& achievement : m_achievements) {
        out << '|' << achievement;
    }
    out << '\n';

    for (Unit* unit : m_units) {
        if (!unit || unit->owner() == UnitOwner::EnemyCtrl) {
            continue;
        }

        QString location = QStringLiteral("HIDDEN");
        QPoint pos = unit->position();
        if (m_board.isValidPosition(pos) && m_board.getUnitAt(pos) == unit) {
            location = QStringLiteral("BOARD:%1:%2").arg(pos.x()).arg(pos.y());
        } else {
            const int benchSlot = benchIndexOf(unit);
            if (benchSlot >= 0) {
                location = QStringLiteral("BENCH:%1").arg(benchSlot);
            }
        }

        out << "UNIT|"
            << unit->name() << '|'
            << unit->starLevel() << '|'
            << unit->hp() << '|'
            << unit->maxHp() << '|'
            << unit->atk() << '|'
            << unit->range() << '|'
            << unit->maxMana() << '|'
            << unit->mana() << '|'
            << location << '\n';
    }

    m_lastResult = QStringLiteral("已保存到 savegame.txt。");
    addLog(QStringLiteral("保存游戏。"));
    updateInfoPanel();
}

void Game::loadGame()
{
    QFile file(QStringLiteral("savegame.txt"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastResult = QStringLiteral("没有找到 savegame.txt。");
        updateInfoPanel();
        return;
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

    QTextStream in(&file);
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

    m_eventRewardRound = m_player.currentRound();
    m_currentEvent = currentEventForRound(m_player.currentRound());
    generateEnemyRound(m_player.currentRound());
    const QPoint enemyPositions[] = {
        QPoint(5, 0),
        QPoint(6, 0)
    };
    int deployedEnemies = 0;
    for (Unit* unit : m_units) {
        if (!unit || unit->owner() != UnitOwner::EnemyCtrl || deployedEnemies >= 2) {
            continue;
        }
        unit->resetCombatState();
        m_board.addUnit(unit, enemyPositions[deployedEnemies]);
        ++deployedEnemies;
    }

    m_lastResult = QStringLiteral("存档读取完成。");
    addLog(QStringLiteral("读取存档。"));
    checkAchievements();
    buildScene();
    syncFromBoard();
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

    addPlayerUnit(createUnitFromTemplate(QStringLiteral("战士"), UnitOwner::PlayerCtrl));
    addPlayerUnit(createUnitFromTemplate(QStringLiteral("弓手"), UnitOwner::PlayerCtrl));
    addPlayerUnit(createUnitFromTemplate(QStringLiteral("法师"), UnitOwner::PlayerCtrl));
    addPlayerUnit(createUnitFromTemplate(QStringLiteral("预备兵"), UnitOwner::PlayerCtrl));
    addPlayerUnit(createUnitFromTemplate(QStringLiteral("守卫"), UnitOwner::PlayerCtrl));

    generateEnemyRound(m_player.currentRound());
}

Unit* Game::createUnitFromTemplate(const QString& name, UnitOwner owner) const
{
    if (name == QStringLiteral("战士") || name == QStringLiteral("Warrior")) {
        return new Unit(QStringLiteral("战士"), 120, 14, 1, 80, owner, {QStringLiteral("前排"), QStringLiteral("人类")}, SkillType::PowerStrike);
    }
    if (name == QStringLiteral("弓手") || name == QStringLiteral("Archer")) {
        return new Unit(QStringLiteral("弓手"), 80, 18, 3, 60, owner, {QStringLiteral("游侠"), QStringLiteral("人类")}, SkillType::PowerStrike);
    }
    if (name == QStringLiteral("法师") || name == QStringLiteral("Mage")) {
        return new Unit(QStringLiteral("法师"), 70, 22, 3, 100, owner, {QStringLiteral("奥术"), QStringLiteral("人类")}, SkillType::ArcaneBurst);
    }
    if (name == QStringLiteral("预备兵") || name == QStringLiteral("Reserve")) {
        return new Unit(QStringLiteral("预备兵"), 95, 12, 1, 70, owner, {QStringLiteral("前排")}, SkillType::SelfHeal);
    }
    if (name == QStringLiteral("守卫") || name == QStringLiteral("Guard")) {
        return new Unit(QStringLiteral("守卫"), 140, 10, 1, 90, owner, {QStringLiteral("前排")}, SkillType::SelfHeal);
    }
    if (name == QStringLiteral("敌方战士") || name == QStringLiteral("Enemy Warrior")) {
        return new Unit(QStringLiteral("敌方战士"), 120, 15, 1, 80, owner, {QStringLiteral("前排"), QStringLiteral("敌人")}, SkillType::PowerStrike);
    }
    if (name == QStringLiteral("敌方弓手") || name == QStringLiteral("Enemy Archer")) {
        return new Unit(QStringLiteral("敌方弓手"), 85, 18, 3, 60, owner, {QStringLiteral("游侠"), QStringLiteral("敌人")}, SkillType::ArcaneBurst);
    }

    return new Unit(name, 100, 10, 1, 100, owner, {QStringLiteral("普通")}, SkillType::PowerStrike);
}

QStringList Game::unitPool() const
{
    return {
        QStringLiteral("战士"),
        QStringLiteral("弓手"),
        QStringLiteral("法师"),
        QStringLiteral("预备兵"),
        QStringLiteral("守卫")
    };
}

void Game::rollShop()
{
    const QStringList pool = unitPool();
    for (int i = 0; i < m_shopSlots.size(); ++i) {
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

    unit->setStarLevel(unit->starLevel() + 1);
    unit->setMaxHp(unit->maxHp() * 17 / 10);
    unit->setHp(unit->maxHp());
    unit->setAtk(unit->atk() * 17 / 10);
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

QString Game::activeTraitsText() const
{
    const QHash<QString, int> counts = traitCounts();
    QStringList active;

    for (auto it = counts.begin(); it != counts.end(); ++it) {
        if (it.value() >= 2) {
            active << QStringLiteral("%1×%2").arg(it.key()).arg(it.value());
        }
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
    if (name == QStringLiteral("活力甲") || name == QStringLiteral("Vitality Armor")) {
        return Equipment(EquipmentType::VitalityArmor);
    }
    if (name == QStringLiteral("迅捷符") || name == QStringLiteral("Swift Charm")) {
        return Equipment(EquipmentType::SwiftCharm);
    }
    if (name == QStringLiteral("法力护符") || name == QStringLiteral("Mana Amulet")) {
        return Equipment(EquipmentType::ManaAmulet);
    }
    return Equipment(EquipmentType::TrainingSword);
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

    Unit* enemyWarrior = findEnemyByName(QStringLiteral("敌方战士"));
    if (!enemyWarrior) {
        enemyWarrior = createUnitFromTemplate(QStringLiteral("敌方战士"), UnitOwner::EnemyCtrl);
        m_units.append(enemyWarrior);
    }

    Unit* enemyArcher = findEnemyByName(QStringLiteral("敌方弓手"));
    if (!enemyArcher) {
        enemyArcher = createUnitFromTemplate(QStringLiteral("敌方弓手"), UnitOwner::EnemyCtrl);
        m_units.append(enemyArcher);
    }

    const int growth = qMax(0, round - 1) * 20;
    enemyWarrior->setMaxHp(120 + growth);
    enemyWarrior->setHp(enemyWarrior->maxHp());
    enemyWarrior->setAtk(15 + qMax(0, round - 1) * 2);
    enemyWarrior->setMana(0);

    enemyArcher->setMaxHp(85 + growth);
    enemyArcher->setHp(enemyArcher->maxHp());
    enemyArcher->setAtk(18 + qMax(0, round - 1) * 2);
    enemyArcher->setMana(0);

    if (round > 0 && round % 5 == 0) {
        enemyWarrior->setMaxHp(enemyWarrior->maxHp() + 80);
        enemyWarrior->setHp(enemyWarrior->maxHp());
        enemyWarrior->setAtk(enemyWarrior->atk() + 5);

        enemyArcher->setMaxHp(enemyArcher->maxHp() + 60);
        enemyArcher->setHp(enemyArcher->maxHp());
        enemyArcher->setAtk(enemyArcher->atk() + 5);
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

bool Game::isBoardPosition(const QPoint& gridPos) const
{
    return m_board.isValidPosition(gridPos);
}

bool Game::isBenchPosition(const QPoint& gridPos) const
{
    return gridPos.y() == Board::ROWS
        && gridPos.x() >= 0
        && gridPos.x() < m_benchSlotCount;
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
            m_board.removeUnit(unit);
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

    for (Unit* candidate : m_units) {
        if (!candidate
            || !candidate->isAlive()
            || candidate->owner() == unit->owner()) {
            continue;
        }

        const QPoint candidatePos = candidate->position();
        if (!m_board.isValidPosition(candidatePos) || m_board.getUnitAt(candidatePos) != candidate) {
            continue;
        }

        const int distance = gridDistance(unit, candidate);
        if (distance < bestDistance) {
            best = candidate;
            bestDistance = distance;
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

QPoint Game::nextStepToward(const QPoint& from, const QPoint& to) const
{
    QVector<QPoint> candidates;
    const int dx = to.x() - from.x();
    const int dy = to.y() - from.y();

    const QPoint horizontal(from.x() + (dx == 0 ? 0 : (dx > 0 ? 1 : -1)), from.y());
    const QPoint vertical(from.x(), from.y() + (dy == 0 ? 0 : (dy > 0 ? 1 : -1)));

    if (qAbs(dx) >= qAbs(dy)) {
        candidates << horizontal << vertical;
    } else {
        candidates << vertical << horizontal;
    }

    for (const QPoint& candidate : candidates) {
        if (candidate != from
            && m_board.isValidPosition(candidate)
            && !m_board.hasUnitAt(candidate)) {
            return candidate;
        }
    }

    return from;
}

void Game::moveUnitToward(Unit* unit, Unit* target)
{
    if (!unit || !target || unit->moveCooldown() > 0) {
        return;
    }

    const QPoint from = unit->position();
    const QPoint next = nextStepToward(from, target->position());
    if (next == from) {
        unit->setState(UnitState::Idle);
        unit->setMoveCooldown(3);
        return;
    }

    m_board.removeUnit(unit);
    m_board.addUnit(unit, next);
    unit->setState(UnitState::Moving);
    unit->setMoveCooldown(3);
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
    unit->setMana(qMin(unit->maxMana(), unit->mana() + 30));
    applyDamage(target, unit->atk());
    addLog(QStringLiteral("%1攻击%2，造成%3伤害。").arg(unit->name(), target->name()).arg(unit->atk()));
}

void Game::castSkill(Unit* unit, Unit* target)
{
    if (!unit || !target) {
        return;
    }

    unit->setState(UnitState::Casting);
    unit->setMana(0);
    unit->setAttackCooldown(unit->attackInterval());

    switch (unit->skillType()) {
    case SkillType::PowerStrike:
        applyDamage(target, unit->atk() * 2 + 20);
        addLog(QStringLiteral("%1释放强力一击。").arg(unit->name()));
        break;
    case SkillType::SelfHeal:
        unit->setHp(qMin(unit->maxHp(), unit->hp() + 45));
        addLog(QStringLiteral("%1释放自我治疗。").arg(unit->name()));
        break;
    case SkillType::ArcaneBurst: {
        const QPoint center = target->position();
        for (Unit* candidate : m_units) {
            if (!candidate
                || !candidate->isAlive()
                || candidate->owner() == unit->owner()) {
                continue;
            }

            if (gridDistance(center, candidate->position()) <= 1) {
                applyDamage(candidate, unit->atk() + 20);
            }
        }
        addLog(QStringLiteral("%1释放奥术爆裂。").arg(unit->name()));
        break;
    }
    }
}

void Game::applyDamage(Unit* target, int damage)
{
    if (!target || !target->isAlive()) {
        return;
    }

    target->setHp(qMax(0, target->hp() - damage));
    if (target->hp() <= 0) {
        target->setState(UnitState::Dead);
        m_board.removeUnit(target);
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

void Game::finishCombat(bool playerWon)
{
    m_combatTimer->stop();
    m_phase = GamePhase::Resolve;

    if (playerWon) {
        constexpr int kVictoryGold = 5;
        Equipment reward = randomEquipment();
        m_player.setGold(m_player.gold() + kVictoryGold);
        m_equipmentPool.append(reward);
        m_lastResult = QStringLiteral("胜利！获得%1金币，掉落%2。").arg(kVictoryGold).arg(reward.name());
        addLog(m_lastResult);
        unlockAchievement(QStringLiteral("初战告捷"));
        m_player.setCurrentRound(m_player.currentRound() + 1);
        rollShop();
        m_phase = GamePhase::Prepare;
        setupRoundBoard();
    } else {
        m_player.setHp(qMax(0, m_player.hp() - 10));
        m_lastResult = QStringLiteral("失败，扣除生命并重置本轮。");
        addLog(m_lastResult);
        m_phase = GamePhase::Prepare;
        setupRoundBoard();
    }

    checkAchievements();
    syncFromBoard();
}

QString Game::currentEventForRound(int round) const
{
    if (round > 0 && round % 5 == 0) {
        return QStringLiteral("精英来袭：本轮敌人属性提升。");
    }
    if (round > 0 && round % 3 == 0) {
        return QStringLiteral("丰收回合：准备阶段额外获得3金币。");
    }
    return QStringLiteral("无");
}

void Game::updateRoundEvent()
{
    m_currentEvent = currentEventForRound(m_player.currentRound());
    if (m_currentEvent == QStringLiteral("无") || m_eventRewardRound == m_player.currentRound()) {
        return;
    }

    m_eventRewardRound = m_player.currentRound();
    addLog(QStringLiteral("触发事件：%1").arg(m_currentEvent));

    if (m_player.currentRound() % 3 == 0 && m_player.currentRound() % 5 != 0) {
        m_player.setGold(m_player.gold() + 3);
        addLog(QStringLiteral("丰收回合奖励：金币+3。"));
        checkAchievements();
    }
}

void Game::addLog(const QString& message)
{
    if (message.isEmpty()) {
        return;
    }

    m_logs.prepend(message);
    while (m_logs.size() > 8) {
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
    if (m_player.gold() >= 20) {
        unlockAchievement(QStringLiteral("小有积蓄"));
    }
    if (m_player.level() >= 3) {
        unlockAchievement(QStringLiteral("扩编成军"));
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
    case GamePhase::Combat:
        return QStringLiteral("战斗");
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
    switch (skillType) {
    case SkillType::PowerStrike:
        return QStringLiteral("强力一击");
    case SkillType::SelfHeal:
        return QStringLiteral("自我治疗");
    case SkillType::ArcaneBurst:
        return QStringLiteral("奥术爆裂");
    }
    return QStringLiteral("未知");
}

void Game::buildScene()
{
    m_scene->clear();
    m_infoPanel = nullptr;
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

    const qreal panelX = gridToWorld(0, Board::COLS - 1).x() + 96.0;
    const QRectF panelRect(panelX, -40.0, 360.0, 640.0);
    QGraphicsRectItem* panelBack = m_scene->addRect(
        panelRect,
        QPen(QColor(90, 90, 95), 1),
        QBrush(QColor(32, 32, 36, 210)));
    panelBack->setZValue(kZGrid + 0.05);

    m_infoPanel = m_scene->addText(QString(), labelFont);
    m_infoPanel->setDefaultTextColor(QColor(235, 235, 235));
    m_infoPanel->setTextWidth(panelRect.width() - 20.0);
    m_infoPanel->setZValue(kZGrid + 0.1);
    m_infoPanel->setPos(panelRect.left() + 10.0, panelRect.top() + 10.0);
    updateInfoPanel();
    totalBounds = totalBounds.united(panelRect);

    for (Unit* unit : m_units) {
        UnitItem* unitItem = new UnitItem(unit);
        unitItem->setZValue(kZUnit);
        m_scene->addItem(unitItem);
        m_unitItems.push_back(unitItem);
        m_unitItemById[unit->id()] = unitItem;

        connect(unitItem, &UnitItem::unitSelected,
                this, &Game::handleUnitSelected);
        connect(unitItem, &UnitItem::dragStarted,
                this, &Game::handleDragStarted);
        connect(unitItem, &UnitItem::dragMoved,
                this, &Game::handleDragMoved);
        connect(unitItem, &UnitItem::dragDropped,
                this, &Game::handleDropCommand);
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

        item->setDragEnabled(m_phase == GamePhase::Prepare
                             && item->unit()->owner() == UnitOwner::PlayerCtrl);

        const QPoint pos = item->unit()->position();
        if (!m_board.isValidPosition(pos) || m_board.getUnitAt(pos) != item->unit()) {
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
    if (!m_infoPanel) {
        return;
    }

    const Unit* selected = findUnitById(m_selectedUnitId);
    QString ownerText;
    QString unitText = QStringLiteral("选中单位\n无");

    if (selected) {
        ownerText = selected->owner() == UnitOwner::PlayerCtrl
            ? QStringLiteral("己方")
            : QStringLiteral("敌方");

        unitText.clear();
        QTextStream unitStream(&unitText);
        unitStream << "选中单位\n"
                   << selected->name() << '\n'
                   << "星级: " << selected->starLevel() << '\n'
                   << "归属: " << ownerText << '\n'
                   << "状态: " << stateName(selected->state()) << '\n'
                   << "技能: " << skillName(selected->skillType()) << '\n'
                   << "生命: " << selected->hp() << '/' << selected->maxHp() << '\n'
                   << "攻击: " << selected->atk() << "   射程: " << selected->range() << '\n'
                   << "法力: " << selected->mana() << '/' << selected->maxMana() << '\n'
                   << "羁绊: " << selected->traits().join(QStringLiteral("，")) << '\n'
                   << "装备: "
                   << (selected->equipmentNames().isEmpty()
                           ? QStringLiteral("无")
                           : selected->equipmentNames().join(QStringLiteral("，")));
    }

    int benchUsed = 0;
    for (Unit* unit : m_benchSlots) {
        if (unit) {
            ++benchUsed;
        }
    }

    QStringList shopLines;
    for (int i = 0; i < m_shopSlots.size(); ++i) {
        shopLines << QStringLiteral("%1:%2").arg(i + 1).arg(m_shopSlots.at(i).isEmpty() ? QStringLiteral("-") : m_shopSlots.at(i));
    }

    QStringList equipmentLines;
    for (const Equipment& equipment : m_equipmentPool) {
        equipmentLines << equipment.name();
    }

    const QString achievementsText = m_achievements.isEmpty()
        ? QStringLiteral("无")
        : m_achievements.join(QStringLiteral("，"));
    const QString logsText = m_logs.isEmpty()
        ? QStringLiteral("暂无")
        : m_logs.join(QStringLiteral("\n"));

    QString text;
    QTextStream stream(&text);
    stream << "玩家\n"
           << "阶段: " << phaseName() << '\n'
           << "结果: " << m_lastResult << '\n'
           << "事件: " << m_currentEvent << '\n'
           << "生命: " << m_player.hp() << '\n'
           << "金币: " << m_player.gold() << '\n'
           << "等级: " << m_player.level() << '\n'
           << "轮次: " << m_player.currentRound() << '\n'
           << "上阵: " << playerBoardUnitCount() << '/' << m_player.populationLimit() << '\n'
           << "备战区: " << benchUsed << '/' << m_benchSlotCount << "\n\n"
           << "商店: " << shopLines.join(QStringLiteral(" | ")) << '\n'
           << "激活羁绊: " << activeTraitsText() << '\n'
           << "装备池: "
           << (equipmentLines.isEmpty() ? QStringLiteral("无") : equipmentLines.join(QStringLiteral("，")))
           << '\n'
           << "成就: " << achievementsText
           << "\n\n"
           << unitText << "\n\n"
           << "最近日志:\n" << logsText << "\n\n"
           << "准备阶段: 拖拽布阵。\n"
           << "战斗阶段: 单位自动行动。\n"
           << "非法放置会回弹。";

    m_infoPanel->setPlainText(text);
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
        const QRectF cellRect(center.x() - m_cellSize * 0.5,
                              center.y() - m_cellSize * 0.5,
                              m_cellSize,
                              m_cellSize);
        if (cellRect.contains(world)) {
            return gridPos;
        }
    }

    for (int slot = 0; slot < m_benchSlotCount; ++slot) {
        const QPointF center = benchToWorld(slot);
        const QRectF cellRect(center.x() - m_cellSize * 0.5,
                              center.y() - m_cellSize * 0.5,
                              m_cellSize,
                              m_cellSize);
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
