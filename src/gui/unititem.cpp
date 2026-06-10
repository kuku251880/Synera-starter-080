#include "gui/unititem.h"
#include "entity/unit.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QTimerEvent>

UnitItem::UnitItem(Unit* unit, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_unit(unit)
    , m_gridPos(-1, -1)
    , m_dragging(false)
    , m_dragEnabled(true)
    , m_selectedActive(false)
    , m_deathAnimationFinished(false)
    , m_animationFrameIndex(0)
    , m_animationTimerId(0)
    , m_dragOffset(0.0, 0.0)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    m_animationTimerId = startTimer(90);
}

QRectF UnitItem::boundingRect() const
{
    return QRectF(-46, -54, 92, 108);
}

void UnitItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing);

    ensureAnimationLoaded();

    if (m_selectedActive) {
        painter->setPen(QPen(QColor(255, 218, 107), 2.5));
        painter->setBrush(QColor(255, 218, 107, 28));
        painter->drawRoundedRect(QRectF(-43, -53, 86, 103), 6, 6);
    }

    if (!m_animationFrames.isEmpty()) {
        const QRectF targetRect(-40, -40, 80, 80);
        const int frameIndex = m_animationFrameIndex % m_animationFrames.size();
        const QPixmap& frame = m_animationFrames.at(frameIndex);
        painter->drawPixmap(targetRect, frame, frame.rect());
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

    const QColor hpColor = m_unit->owner() == UnitOwner::PlayerCtrl
        ? QColor(70, 205, 92)
        : QColor(230, 92, 92);

    const qreal barWidth = 56.0;
    const qreal barHeight = 4.0;
    const QRectF hpBack(-28, -35, barWidth, barHeight);
    const QRectF manaBack(-28, -29, barWidth, barHeight);
    const qreal hpRatio = qBound(0.0, static_cast<qreal>(m_unit->hp()) / qMax(1, m_unit->maxHp()), 1.0);
    const qreal manaRatio = qBound(0.0, static_cast<qreal>(m_unit->mana()) / qMax(1, m_unit->maxMana()), 1.0);

    painter->setPen(QPen(QColor(15, 15, 15), 1));
    painter->setBrush(QColor(25, 25, 25, 210));
    painter->drawRect(hpBack);
    painter->drawRect(manaBack);

    painter->setPen(Qt::NoPen);
    painter->setBrush(hpColor);
    painter->drawRect(QRectF(hpBack.left(), hpBack.top(), hpBack.width() * hpRatio, hpBack.height()));
    painter->setBrush(QColor(80, 150, 255));
    painter->drawRect(QRectF(manaBack.left(), manaBack.top(), manaBack.width() * manaRatio, manaBack.height()));

    QFont font = painter->font();
    font.setPointSize(7);
    font.setBold(true);
    painter->setFont(font);
    painter->setPen(Qt::white);
    painter->drawText(QRectF(-38, 29, 76, 10), Qt::AlignCenter,
                      QStringLiteral("%1星 血%2 攻%3")
                          .arg(m_unit->starLevel())
                          .arg(m_unit->hp())
                          .arg(m_unit->atk()));
}

void UnitItem::ensureAnimationLoaded()
{
    if (!m_unit) {
        return;
    }

    const QString relativeDir = animationRelativeDirForUnit();
    const QString animationKey = m_unit->name()
        + QStringLiteral("|")
        + QString::number(static_cast<int>(m_unit->state()))
        + QStringLiteral("|")
        + relativeDir;
    if (m_loadedAnimationKey == animationKey) {
        return;
    }

    m_loadedAnimationKey = animationKey;
    m_animationFrames.clear();
    m_animationFrameIndex = 0;
    m_deathAnimationFinished = false;

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString roots[] = {
        QFileInfo(appDir + "/..").canonicalFilePath(),
        QFileInfo(appDir + "/../..").canonicalFilePath()
    };

    auto loadSequence = [&](const QString& dirRelativePath) {
        if (dirRelativePath.isEmpty()) {
            return;
        }

        for (const QString& root : roots) {
            if (root.isEmpty()) {
                continue;
            }

            const QDir dir(root + "/" + dirRelativePath);
            if (!dir.exists()) {
                continue;
            }

            const QStringList files = dir.entryList(QStringList() << QStringLiteral("*.png"),
                                                    QDir::Files,
                                                    QDir::Name);
            for (const QString& fileName : files) {
                QPixmap pix;
                pix.load(dir.filePath(fileName));
                if (!pix.isNull()) {
                    m_animationFrames.append(pix.scaled(80,
                                                        80,
                                                        Qt::KeepAspectRatio,
                                                        Qt::SmoothTransformation));
                }
            }

            if (!m_animationFrames.isEmpty()) {
                return;
            }
        }
    };

    loadSequence(relativeDir);
    if (!m_animationFrames.isEmpty()) {
        return;
    }

    const QString idleFramePath = idleFrameRelativePathForUnit();
    if (idleFramePath.isEmpty()) {
        return;
    }

    for (const QString& root : roots) {
        if (root.isEmpty()) {
            continue;
        }

        QPixmap pix;
        pix.load(root + "/" + idleFramePath);
        if (!pix.isNull()) {
            m_animationFrames.append(pix.scaled(80,
                                                80,
                                                Qt::KeepAspectRatio,
                                                Qt::SmoothTransformation));
            return;
        }
    }
}

QString UnitItem::animationRelativeDirForUnit() const
{
    if (!m_unit) {
        return QString();
    }

    const QString name = m_unit->name();
    const bool reaper = name == QStringLiteral("战士")
        || name == QStringLiteral("Warrior")
        || name == QStringLiteral("法师")
        || name == QStringLiteral("Mage")
        || name == QStringLiteral("敌方战士")
        || name == QStringLiteral("Enemy Warrior");
    const bool satyr = name == QStringLiteral("弓手")
        || name == QStringLiteral("Archer")
        || name == QStringLiteral("预备兵")
        || name == QStringLiteral("Reserve")
        || name == QStringLiteral("守卫")
        || name == QStringLiteral("Guard")
        || name == QStringLiteral("敌方弓手")
        || name == QStringLiteral("Enemy Archer");

    QString variant;
    if (name == QStringLiteral("战士") || name == QStringLiteral("Warrior")) {
        variant = QStringLiteral("Reaper_Man_1");
    } else if (name == QStringLiteral("法师") || name == QStringLiteral("Mage")) {
        variant = QStringLiteral("Reaper_Man_2");
    } else if (name == QStringLiteral("敌方战士") || name == QStringLiteral("Enemy Warrior")) {
        variant = QStringLiteral("Reaper_Man_3");
    } else if (name == QStringLiteral("弓手") || name == QStringLiteral("Archer")) {
        variant = QStringLiteral("Satyr_01");
    } else if (name == QStringLiteral("预备兵") || name == QStringLiteral("Reserve")) {
        variant = QStringLiteral("Satyr_02");
    } else if (name == QStringLiteral("守卫")
               || name == QStringLiteral("Guard")
               || name == QStringLiteral("敌方弓手")
               || name == QStringLiteral("Enemy Archer")) {
        variant = QStringLiteral("Satyr_03");
    }

    QString action;
    switch (m_unit->state()) {
    case UnitState::Idle:
        action = QStringLiteral("Idle");
        break;
    case UnitState::Moving:
        action = QStringLiteral("Walking");
        break;
    case UnitState::Attacking:
        action = reaper ? QStringLiteral("Slashing") : QStringLiteral("Attacking");
        break;
    case UnitState::Casting:
        action = reaper ? QStringLiteral("Throwing") : QStringLiteral("Taunt");
        break;
    case UnitState::Dead:
        action = QStringLiteral("Dying");
        break;
    }

    if (reaper && !variant.isEmpty()) {
        return QStringLiteral("assets/craftpix-reaper-man-chibi-2d-game-sprites/%1/PNG/PNG Sequences/%2")
            .arg(variant, action);
    }
    if (satyr && !variant.isEmpty()) {
        return QStringLiteral("assets/craftpix-satyr-tiny-style-2d-sprites/PNG/%1/PNG Sequences/%2")
            .arg(variant, action);
    }

    return QString();
}

QString UnitItem::idleFrameRelativePathForUnit() const
{
    if (!m_unit) {
        return QString();
    }

    const QString name = m_unit->name();
    if (name == QStringLiteral("战士") || name == QStringLiteral("Warrior")) {
        return QStringLiteral("assets/craftpix-reaper-man-chibi-2d-game-sprites/Reaper_Man_1/PNG/PNG Sequences/Idle/0_Reaper_Man_Idle_000.png");
    }
    if (name == QStringLiteral("弓手") || name == QStringLiteral("Archer")) {
        return QStringLiteral("assets/craftpix-satyr-tiny-style-2d-sprites/PNG/Satyr_01/PNG Sequences/Idle/Satyr_01_Idle_000.png");
    }
    if (name == QStringLiteral("法师") || name == QStringLiteral("Mage")) {
        return QStringLiteral("assets/craftpix-reaper-man-chibi-2d-game-sprites/Reaper_Man_2/PNG/PNG Sequences/Idle/0_Reaper_Man_Idle_000.png");
    }
    if (name == QStringLiteral("预备兵") || name == QStringLiteral("Reserve")) {
        return QStringLiteral("assets/craftpix-satyr-tiny-style-2d-sprites/PNG/Satyr_02/PNG Sequences/Idle/Satyr_02_Idle_000.png");
    }
    if (name == QStringLiteral("守卫") || name == QStringLiteral("Guard")) {
        return QStringLiteral("assets/craftpix-satyr-tiny-style-2d-sprites/PNG/Satyr_03/PNG Sequences/Idle/Satyr_03_Idle_000.png");
    }
    if (name == QStringLiteral("敌方战士") || name == QStringLiteral("Enemy Warrior")) {
        return QStringLiteral("assets/craftpix-reaper-man-chibi-2d-game-sprites/Reaper_Man_3/PNG/PNG Sequences/Idle/0_Reaper_Man_Idle_000.png");
    }
    if (name == QStringLiteral("敌方弓手") || name == QStringLiteral("Enemy Archer")) {
        return QStringLiteral("assets/craftpix-satyr-tiny-style-2d-sprites/PNG/Satyr_03/PNG Sequences/Idle/Satyr_03_Idle_000.png");
    }

    return QString();
}

int UnitItem::unitId() const
{
    return m_unit ? m_unit->id() : -1;
}

void UnitItem::setSelectedActive(bool active)
{
    if (m_selectedActive == active) {
        return;
    }
    m_selectedActive = active;
    update();
}

void UnitItem::setGridPos(const QPoint& gridPos)
{
    m_gridPos = gridPos;
}

void UnitItem::timerEvent(QTimerEvent* event)
{
    if (event->timerId() != m_animationTimerId) {
        QGraphicsObject::timerEvent(event);
        return;
    }

    if (m_animationFrames.size() > 1 && m_unit && m_unit->state() == UnitState::Dead) {
        if (m_animationFrameIndex < m_animationFrames.size() - 1) {
            ++m_animationFrameIndex;
            update();
            return;
        }

        if (!m_deathAnimationFinished) {
            m_deathAnimationFinished = true;
            setVisible(false);
        }
        return;
    }

    if (m_animationFrames.size() > 1) {
        m_animationFrameIndex = (m_animationFrameIndex + 1) % m_animationFrames.size();
        update();
    }
}

void UnitItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QGraphicsObject::mousePressEvent(event);
        return;
    }

    emit unitSelected(unitId());

    if (!m_dragEnabled || !m_unit || m_unit->owner() != UnitOwner::PlayerCtrl) {
        event->accept();
        return;
    }

    m_dragging = true;
    m_dragOffset = pos() - event->scenePos();
    setOpacity(0.82);
    emit dragStarted(unitId(), m_gridPos, event->scenePos());
    event->accept();
}

void UnitItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (!m_dragging) {
        QGraphicsObject::mouseMoveEvent(event);
        return;
    }

    setPos(event->scenePos() + m_dragOffset);
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
    setOpacity(1.0);
    emit dragDropped(unitId(), m_gridPos, event->scenePos());
    event->accept();
}
