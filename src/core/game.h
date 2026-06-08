#ifndef CORE_GAME_H
#define CORE_GAME_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QPoint>
#include <QPointF>
#include <QPolygonF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <unordered_map>
#include <vector>
#include "board.h"
#include "entity/equipment.h"
#include "entity/player.h"

class Unit;
class QGraphicsScene;
class QGraphicsTextItem;
class QTimer;
class GridItem;
class UnitItem;

enum class GamePhase
{
    Prepare,
    Combat,
    Resolve
};

class Game : public QObject
{
    Q_OBJECT

public:
    explicit Game(QObject* parent = nullptr);
    ~Game();

    void initialize();
    void reset();
    void startCombat();
    void buyShopUnit(int slot);
    void rerollShop();
    void levelUp();
    void equipSelectedUnit();
    void saveGame();
    void loadGame();

    QGraphicsScene* scene() const { return m_scene; }

    void handleDragStarted(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);
    void handleDragMoved(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);
    void handleDropCommand(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);
    void handleUnitSelected(int unitId);

private:
    void setupRoundBoard();
    void createStarterUnitsIfNeeded();
    Unit* createUnitFromTemplate(const QString& name, UnitOwner owner) const;
    QStringList unitPool() const;
    void rollShop();
    int firstEmptyBenchSlot() const;
    bool addUnitToBench(Unit* unit);
    void tryMergeUnits();
    void upgradeUnitStar(Unit* unit);
    QHash<QString, int> traitCounts() const;
    QString activeTraitsText() const;
    Equipment randomEquipment() const;
    Equipment equipmentFromName(const QString& name) const;
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
    void updateCombat();
    Unit* nearestEnemyFor(Unit* unit) const;
    int gridDistance(Unit* a, Unit* b) const;
    int gridDistance(const QPoint& a, const QPoint& b) const;
    QPoint nextStepToward(const QPoint& from, const QPoint& to) const;
    void moveUnitToward(Unit* unit, Unit* target);
    void attackTarget(Unit* unit, Unit* target);
    void castSkill(Unit* unit, Unit* target);
    void applyDamage(Unit* target, int damage);
    bool sideDefeated(UnitOwner owner) const;
    void finishCombat(bool playerWon);
    QString currentEventForRound(int round) const;
    void updateRoundEvent();
    void addLog(const QString& message);
    void unlockAchievement(const QString& name);
    void checkAchievements();
    QString phaseName() const;
    QString stateName(UnitState state) const;
    QString skillName(SkillType skillType) const;
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
    QVector<QString> m_shopSlots;
    QVector<Equipment> m_equipmentPool;

    QGraphicsScene* m_scene;
    QGraphicsTextItem* m_infoPanel;
    QTimer* m_combatTimer;
    std::vector<GridItem*> m_gridItems;
    std::vector<UnitItem*> m_unitItems;

    bool m_dragActive;
    int m_activeUnitId;
    int m_selectedUnitId;
    QPoint m_sourceGrid;
    GamePhase m_phase;
    QString m_lastResult;
    QString m_currentEvent;
    QStringList m_achievements;
    QStringList m_logs;
    int m_eventRewardRound;
    std::unordered_map<int, UnitItem*> m_unitItemById;

    int m_rows;
    int m_cols;
    int m_benchSlotCount;
    qreal m_cellSize;
    qreal m_cellGap;
    qreal m_benchGap;
};

#endif // CORE_GAME_H
