#include "gui/unititem.h"
#include "entity/unit.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

UnitItem::UnitItem(Unit* unit, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_unit(unit)
    , m_gridPos(-1, -1)
    , m_dragging(false)
    , m_spriteTried(false)
{
    setAcceptedMouseButtons(Qt::LeftButton);
}

QRectF UnitItem::boundingRect() const
{
    return QRectF(-46, -54, 92, 108);
}

void UnitItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing);

    ensureSpriteLoaded();

    if (!m_sprite.isNull()) {
        const QRectF targetRect(-40, -40, 80, 80);
        painter->drawPixmap(targetRect, m_sprite, m_sprite.rect());
        paintStatusOverlay(painter);
        return;
    }

    paintFallbackToken(painter);
    paintStatusOverlay(painter);
}

void UnitItem::paintFallbackToken(QPainter* painter) const
{
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(20, 20, 20, 110));
    painter->drawEllipse(QRectF(-14, 8, 28, 10));

    QPolygonF badge;
    badge << QPointF(0, -15)
          << QPointF(13, -7)
          << QPointF(13, 7)
          << QPointF(0, 15)
          << QPointF(-13, 7)
          << QPointF(-13, -7);

    painter->setPen(QPen(QColor(18, 18, 18), 1.5));
    painter->setBrush(QColor(100, 150, 200));
    painter->drawPolygon(badge);

    if (m_unit) {
        painter->setPen(Qt::white);
        QFont font = painter->font();
        font.setPointSize(12);
        font.setBold(true);
        painter->setFont(font);
        painter->drawText(QRectF(-13, -13, 26, 26), Qt::AlignCenter, m_unit->name().left(1));
    }
}

void UnitItem::paintStatusOverlay(QPainter* painter) const
{
    if (!m_unit) {
        return;
    }

    const QColor ownerColor = m_unit->owner() == UnitOwner::PlayerCtrl
        ? QColor(90, 150, 255)
        : QColor(230, 92, 92);

    painter->setPen(QPen(ownerColor, 2));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(QRectF(-42, -42, 84, 84), 4, 4);

    const qreal barWidth = 74.0;
    const qreal barHeight = 6.0;
    const QRectF hpBack(-37, -51, barWidth, barHeight);
    const QRectF manaBack(-37, -43, barWidth, barHeight);
    const qreal hpRatio = qBound(0.0, static_cast<qreal>(m_unit->hp()) / qMax(1, m_unit->maxHp()), 1.0);
    const qreal manaRatio = qBound(0.0, static_cast<qreal>(m_unit->mana()) / qMax(1, m_unit->maxMana()), 1.0);

    painter->setPen(QPen(QColor(15, 15, 15), 1));
    painter->setBrush(QColor(25, 25, 25, 210));
    painter->drawRect(hpBack);
    painter->drawRect(manaBack);

    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(70, 205, 92));
    painter->drawRect(QRectF(hpBack.left(), hpBack.top(), hpBack.width() * hpRatio, hpBack.height()));
    painter->setBrush(QColor(80, 150, 255));
    painter->drawRect(QRectF(manaBack.left(), manaBack.top(), manaBack.width() * manaRatio, manaBack.height()));

    QFont font = painter->font();
    font.setPointSize(7);
    font.setBold(true);
    painter->setFont(font);
    painter->setPen(Qt::white);
    painter->drawText(QRectF(-42, 38, 84, 12), Qt::AlignCenter,
                      QStringLiteral("HP %1  ATK %2  R%3")
                          .arg(m_unit->hp())
                          .arg(m_unit->atk())
                          .arg(m_unit->range()));
}

void UnitItem::ensureSpriteLoaded() const
{
    if (m_spriteTried) {
        return;
    }

    m_spriteTried = true;
    const QString relativePath = spriteRelativePathForUnit();
    if (relativePath.isEmpty()) {
        return;
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString roots[] = {
        QFileInfo(appDir + "/..").canonicalFilePath(),
        QFileInfo(appDir + "/../..").canonicalFilePath()
    };

    QPixmap pix;
    for (const QString& root : roots) {
        if (root.isEmpty()) {
            continue;
        }
        pix.load(root + "/" + relativePath);
        if (!pix.isNull()) {
            break;
        }
    }

    if (pix.isNull()) {
        return;
    }

    m_sprite = pix.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QString UnitItem::spriteRelativePathForUnit() const
{
    if (!m_unit) {
        return QString();
    }

    const QString name = m_unit->name();
    if (name == QStringLiteral("Warrior")) {
        return QStringLiteral("assets/craftpix-reaper-man-chibi-2d-game-sprites/Reaper_Man_1/PNG/PNG Sequences/Idle/0_Reaper_Man_Idle_000.png");
    }
    if (name == QStringLiteral("Archer")) {
        return QStringLiteral("assets/craftpix-satyr-tiny-style-2d-sprites/PNG/Satyr_01/PNG Sequences/Idle/Satyr_01_Idle_000.png");
    }
    if (name == QStringLiteral("Mage")) {
        return QStringLiteral("assets/craftpix-reaper-man-chibi-2d-game-sprites/Reaper_Man_2/PNG/PNG Sequences/Idle/0_Reaper_Man_Idle_000.png");
    }
    if (name == QStringLiteral("Reserve")) {
        return QStringLiteral("assets/craftpix-satyr-tiny-style-2d-sprites/PNG/Satyr_02/PNG Sequences/Idle/Satyr_02_Idle_000.png");
    }
    if (name == QStringLiteral("Guard")) {
        return QStringLiteral("assets/craftpix-satyr-tiny-style-2d-sprites/PNG/Satyr_03/PNG Sequences/Idle/Satyr_03_Idle_000.png");
    }
    if (name == QStringLiteral("Enemy Warrior")) {
        return QStringLiteral("assets/craftpix-reaper-man-chibi-2d-game-sprites/Reaper_Man_3/PNG/PNG Sequences/Idle/0_Reaper_Man_Idle_000.png");
    }
    if (name == QStringLiteral("Enemy Archer")) {
        return QStringLiteral("assets/craftpix-satyr-tiny-style-2d-sprites/PNG/Satyr_03/PNG Sequences/Idle/Satyr_03_Idle_000.png");
    }

    return QString();
}

int UnitItem::unitId() const
{
    return m_unit ? m_unit->id() : -1;
}

void UnitItem::setGridPos(const QPoint& gridPos)
{
    m_gridPos = gridPos;
}

void UnitItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QGraphicsObject::mousePressEvent(event);
        return;
    }

    m_dragging = true;
    emit dragStarted(unitId(), m_gridPos, event->scenePos());
    event->accept();
}

void UnitItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (!m_dragging) {
        QGraphicsObject::mouseMoveEvent(event);
        return;
    }

    emit dragMoved(unitId(), m_gridPos, event->scenePos());
    event->accept();
}

void UnitItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (!m_dragging || event->button() != Qt::LeftButton) {
        QGraphicsObject::mouseReleaseEvent(event);
        return;
    }

    m_dragging = false;
    emit dragDropped(unitId(), m_gridPos, event->scenePos());
    event->accept();
}
