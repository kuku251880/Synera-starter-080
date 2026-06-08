#include "griditem.h"
#include <QGraphicsSceneHoverEvent>
#include <QPainter>

GridItem::GridItem(int row, int col, const QPolygonF& polygon, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_row(row)
    , m_col(col)
    , m_polygon(polygon)
    , m_bounds(polygon.boundingRect())
    , m_baseColor(QColor(60, 60, 80))
    , m_hoverActive(false)
    , m_dropActive(false)
    , m_pointerHover(false)
{
    setAcceptHoverEvents(true);
}

QRectF GridItem::boundingRect() const
{
    return m_bounds.adjusted(-2.0, -2.0, 2.0, 2.0);
}

void GridItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    QColor fill = m_baseColor;
    QColor border = QColor(40, 40, 40);

    if (m_dropActive) {
        fill = QColor(110, 170, 110);
        border = QColor(100, 255, 100);
    } else if (m_hoverActive || m_pointerHover) {
        fill = m_baseColor.lighter(120);
    }

    painter->setPen(QPen(border, 2));
    painter->setBrush(fill);
    painter->drawPolygon(m_polygon);

    if (m_hoverActive || m_pointerHover) {
        painter->setPen(QPen(QColor(220, 220, 220), 2));
        painter->setBrush(Qt::NoBrush);
        painter->drawPolygon(m_polygon);
    }
}

QPoint GridItem::gridPos() const
{
    return QPoint(m_col, m_row);
}

void GridItem::setBaseColor(const QColor& color)
{
    m_baseColor = color;
    update();
}

void GridItem::setHoverActive(bool active)
{
    if (m_hoverActive == active) {
        return;
    }
    m_hoverActive = active;
    update();
}

void GridItem::setDropActive(bool active)
{
    if (m_dropActive == active) {
        return;
    }
    m_dropActive = active;
    update();
}

void GridItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    Q_UNUSED(event);
    if (!m_pointerHover) {
        m_pointerHover = true;
        update();
    }
}

void GridItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    Q_UNUSED(event);
    if (m_pointerHover) {
        m_pointerHover = false;
        update();
    }
}
