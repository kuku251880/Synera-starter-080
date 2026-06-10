#ifndef GUI_ITEMS_UNITITEM_H
#define GUI_ITEMS_UNITITEM_H

#include <QGraphicsObject>
#include <QPoint>
#include <QPointF>
#include <QPixmap>
#include <QString>
#include <QVector>

class Unit;

class UnitItem : public QGraphicsObject
{
    Q_OBJECT

public:
    explicit UnitItem(Unit* unit, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    Unit* unit() const { return m_unit; }
    int unitId() const;

    void setGridPos(const QPoint& gridPos);
    QPoint gridPos() const { return m_gridPos; }
    void setDragEnabled(bool enabled) { m_dragEnabled = enabled; }
    void setSelectedActive(bool active);
    bool deathAnimationFinished() const { return m_deathAnimationFinished; }

signals:
    void unitSelected(int unitId);
    void dragStarted(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);
    void dragMoved(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);
    void dragDropped(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);

protected:
    void timerEvent(QTimerEvent* event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
    void ensureAnimationLoaded();
    QString animationRelativeDirForUnit() const;
    QString idleFrameRelativePathForUnit() const;
    void paintFallbackToken(QPainter* painter) const;
    void paintStatusOverlay(QPainter* painter) const;

    Unit* m_unit;
    QPoint m_gridPos;
    bool m_dragging;
    bool m_dragEnabled;
    bool m_selectedActive;
    bool m_deathAnimationFinished;
    int m_animationFrameIndex;
    int m_animationTimerId;
    QPointF m_dragOffset;
    mutable QVector<QPixmap> m_animationFrames;
    mutable QString m_loadedAnimationKey;
};

#endif // GUI_ITEMS_UNITITEM_H
