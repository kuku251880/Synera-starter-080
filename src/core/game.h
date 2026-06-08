#ifndef CORE_GAME_H
#define CORE_GAME_H

#include <QObject>
#include <QList>
#include <QPoint>
#include <QPointF>
#include <QPolygonF>
#include <QVector>
#include <unordered_map>
#include <vector>
#include "board.h"
#include "entity/player.h"

class Unit;
class QGraphicsScene;
class QGraphicsTextItem;
class GridItem;
class UnitItem;

class Game : public QObject
{
    Q_OBJECT

public:
    explicit Game(QObject* parent = nullptr);
    ~Game();

    void initialize();
    void reset();

    QGraphicsScene* scene() const { return m_scene; }

    void handleDragStarted(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);
    void handleDragMoved(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);
    void handleDropCommand(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);
    void handleUnitSelected(int unitId);

private:
    void createStarterUnitsIfNeeded();
    void generateEnemyRound(int round);
    Unit* findUnitById(int unitId) const;
    GridItem* findGridItem(const QPoint& gridPos) const;
    UnitItem* findUnitItem(int unitId) const;
    int benchIndexOf(Unit* unit) const;
    Unit* unitAtGrid(const QPoint& gridPos) const;
    int playerBoardUnitCount() const;
    void clearGridHighlights();
    bool isBoardPosition(const QPoint& gridPos) const;
    bool isBenchPosition(const QPoint& gridPos) const;
    bool canApplyDrop(int unitId, const QPoint& source, const QPoint& target) const;
    void applyDrop(int unitId, const QPoint& source, const QPoint& target);
    void buildScene();
    void syncFromBoard();
    void updateInfoPanel();

    QPointF gridToWorld(int row, int col) const;
    QPointF benchToWorld(int slot) const;
    QPoint worldToGrid(const QPointF& world) const;
    QPolygonF cellRectPolygon(int row, int col) const;
    QPolygonF benchCellPolygon(int slot) const;

    Board m_board;
    Player m_player;
    QList<Unit*> m_units;
    QVector<Unit*> m_benchSlots;

    QGraphicsScene* m_scene;
    QGraphicsTextItem* m_infoPanel;
    std::vector<GridItem*> m_gridItems;
    std::vector<UnitItem*> m_unitItems;

    bool m_dragActive;
    int m_activeUnitId;
    int m_selectedUnitId;
    QPoint m_sourceGrid;
    std::unordered_map<int, UnitItem*> m_unitItemById;

    int m_rows;
    int m_cols;
    int m_benchSlotCount;
    qreal m_cellSize;
    qreal m_cellGap;
    qreal m_benchGap;
};

#endif // CORE_GAME_H
