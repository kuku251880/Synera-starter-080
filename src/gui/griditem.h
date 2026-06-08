#ifndef GUI_ITEMS_GRIDITEM_H
#define GUI_ITEMS_GRIDITEM_H

#include <QGraphicsObject>
#include <QPolygonF>

class QGraphicsSceneHoverEvent;

class GridItem : public QGraphicsObject
{
    Q_OBJECT

public:
    GridItem(int row, int col, const QPolygonF& polygon, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    int row() const { return m_row; }
    int col() const { return m_col; }
    QPoint gridPos() const;

    void setBaseColor(const QColor& color);
    void setHoverActive(bool active);
    void setDropActive(bool active);

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    int m_row;
    int m_col;
    QPolygonF m_polygon;
    QRectF m_bounds;
    QColor m_baseColor;
    bool m_hoverActive;
    bool m_dropActive;
    bool m_pointerHover;
};

#endif // GUI_ITEMS_GRIDITEM_H
