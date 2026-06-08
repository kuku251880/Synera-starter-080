#include "game.h"
#include "entity/unit.h"
#include "gui/griditem.h"
#include "gui/unititem.h"
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QtMath>

namespace {
constexpr qreal kZGrid = 0.0;
constexpr qreal kZUnit = 1.0;
constexpr qreal kZDraggingUnit = 2.0;
}

Game::Game(QObject* parent)
    : QObject(parent)
    , m_benchSlots(8, nullptr)
    , m_scene(new QGraphicsScene(this))
    , m_dragActive(false)
    , m_activeUnitId(-1)
    , m_sourceGrid(-1, -1)
    , m_rows(Board::ROWS)
    , m_cols(Board::COLS)
    , m_benchSlotCount(8)
    , m_cellSize(64.0)
    , m_cellGap(4.0)
    , m_benchGap(52.0)
{}

Game::~Game()
{
    qDeleteAll(m_units);
    m_units.clear();
}

void Game::initialize()
{
    createStarterUnitsIfNeeded();
    buildScene();
    reset();
}

void Game::reset()
{
    m_board.clear();
    m_benchSlots.fill(nullptr, m_benchSlotCount);

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

void Game::createStarterUnitsIfNeeded()
{
    if (!m_units.isEmpty()) {
        return;
    }

    m_units.append(new Unit("Warrior", 120, 14, 1, 80, UnitOwner::PlayerCtrl, {"Frontline", "Human"}));
    m_units.append(new Unit("Archer", 80, 18, 3, 60, UnitOwner::PlayerCtrl, {"Ranger", "Human"}));
    m_units.append(new Unit("Mage", 70, 22, 3, 100, UnitOwner::PlayerCtrl, {"Arcane", "Human"}));
    m_units.append(new Unit("Reserve", 95, 12, 1, 70, UnitOwner::PlayerCtrl, {"Frontline"}));
    m_units.append(new Unit("Guard", 140, 10, 1, 90, UnitOwner::PlayerCtrl, {"Frontline"}));
    m_units.append(new Unit("Enemy Warrior", 120, 15, 1, 80, UnitOwner::EnemyCtrl, {"Frontline", "Enemy"}));
    m_units.append(new Unit("Enemy Archer", 85, 18, 3, 60, UnitOwner::EnemyCtrl, {"Ranger", "Enemy"}));
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
    Unit* unit = findUnitById(unitId);
    if (!unit || unit->owner() != UnitOwner::PlayerCtrl) {
        return false;
    }

    if (source == target || unitAtGrid(source) != unit) {
        return false;
    }

    if (isBoardPosition(target)) {
        return m_board.isPlayerHalf(target) && !m_board.hasUnitAt(target);
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

void Game::buildScene()
{
    m_scene->clear();
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

    QGraphicsTextItem* enemyLabel = m_scene->addText(QStringLiteral("Enemy Half"), labelFont);
    enemyLabel->setDefaultTextColor(QColor(230, 170, 170));
    enemyLabel->setZValue(kZGrid + 0.1);
    enemyLabel->setPos(0, -34);

    QGraphicsTextItem* playerLabel = m_scene->addText(QStringLiteral("Player Half"), labelFont);
    playerLabel->setDefaultTextColor(QColor(170, 190, 255));
    playerLabel->setZValue(kZGrid + 0.1);
    playerLabel->setPos(0, gridToWorld(Board::ROWS / 2, 0).y() - 34);

    QGraphicsTextItem* benchLabel = m_scene->addText(QStringLiteral("Bench"), labelFont);
    benchLabel->setDefaultTextColor(QColor(170, 230, 180));
    benchLabel->setZValue(kZGrid + 0.1);
    benchLabel->setPos(0, benchToWorld(0).y() - 54);

    for (Unit* unit : m_units) {
        UnitItem* unitItem = new UnitItem(unit);
        unitItem->setZValue(kZUnit);
        m_scene->addItem(unitItem);
        m_unitItems.push_back(unitItem);
        m_unitItemById[unit->id()] = unitItem;

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
