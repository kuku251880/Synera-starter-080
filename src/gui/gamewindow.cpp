#include "gamewindow.h"
#include "core/game.h"
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>

GameWindow::GameWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_centralWidget(new QWidget(this))
    , m_mainLayout(new QVBoxLayout())
    , m_view(new QGraphicsView(this))
    , m_resetButton(new QPushButton(QStringLiteral("重置"), this))
    , m_startCombatButton(new QPushButton(QStringLiteral("开始战斗"), this))
    , m_rerollButton(new QPushButton(QStringLiteral("刷新商店"), this))
    , m_levelUpButton(new QPushButton(QStringLiteral("升级人口"), this))
    , m_equipButton(new QPushButton(QStringLiteral("装备"), this))
    , m_saveButton(new QPushButton(QStringLiteral("保存"), this))
    , m_loadButton(new QPushButton(QStringLiteral("读取"), this))
    , m_game(new Game(this))
{
    for (int i = 0; i < 5; ++i) {
        m_buyButtons[i] = new QPushButton(QStringLiteral("购买%1").arg(i + 1), this);
    }

    setupUI();
    m_game->initialize();
    QTimer::singleShot(0, this, &GameWindow::fitSceneInView);
}

GameWindow::~GameWindow() = default;

void GameWindow::onResetButtonClicked()
{
    if (m_game) {
        m_game->reset();
        fitSceneInView();
    }
}

void GameWindow::onStartCombatButtonClicked()
{
    if (m_game) {
        m_game->startCombat();
        fitSceneInView();
    }
}

void GameWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    fitSceneInView();
}

void GameWindow::setupUI()
{
    setCentralWidget(m_centralWidget);
    m_centralWidget->setLayout(m_mainLayout);

    setStyleSheet(R"(
        QMainWindow {
            background-color: #2b2b2b;
        }
        QWidget {
            background-color: #2b2b2b;
            color: #f2f2f2;
        }
        QPushButton {
            background-color: #2f2f2f;
            color: #f2f2f2;
            border: 1px solid #565656;
            border-radius: 4px;
            padding: 6px 14px;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: #3a3a3a;
        }
        QPushButton:pressed {
            background-color: #242424;
        }
    )");

    m_view->setRenderHint(QPainter::Antialiasing, true);
    m_view->setDragMode(QGraphicsView::NoDrag);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    m_view->setMouseTracking(true);
    m_view->viewport()->setMouseTracking(true);

    m_mainLayout->addWidget(m_view, 1);

    QWidget* controlBar = new QWidget(this);
    QHBoxLayout* controlLayout = new QHBoxLayout(controlBar);
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->addWidget(m_startCombatButton);
    for (QPushButton* buyButton : m_buyButtons) {
        controlLayout->addWidget(buyButton);
    }
    controlLayout->addWidget(m_rerollButton);
    controlLayout->addWidget(m_levelUpButton);
    controlLayout->addWidget(m_equipButton);
    controlLayout->addWidget(m_saveButton);
    controlLayout->addWidget(m_loadButton);
    controlLayout->addWidget(m_resetButton);
    controlLayout->addStretch();
    m_mainLayout->addWidget(controlBar);

    connect(m_resetButton, &QPushButton::clicked,
            this, &GameWindow::onResetButtonClicked);
    connect(m_startCombatButton, &QPushButton::clicked,
            this, &GameWindow::onStartCombatButtonClicked);
    for (int i = 0; i < 5; ++i) {
        connect(m_buyButtons[i], &QPushButton::clicked,
                this, [this, i]() {
                    if (m_game) {
                        m_game->buyShopUnit(i);
                        fitSceneInView();
                    }
                });
    }
    connect(m_rerollButton, &QPushButton::clicked,
            this, [this]() {
                if (m_game) {
                    m_game->rerollShop();
                }
            });
    connect(m_levelUpButton, &QPushButton::clicked,
            this, [this]() {
                if (m_game) {
                    m_game->levelUp();
                }
            });
    connect(m_equipButton, &QPushButton::clicked,
            this, [this]() {
                if (m_game) {
                    m_game->equipSelectedUnit();
                }
            });
    connect(m_saveButton, &QPushButton::clicked,
            this, [this]() {
                if (m_game) {
                    m_game->saveGame();
                }
            });
    connect(m_loadButton, &QPushButton::clicked,
            this, [this]() {
                if (m_game) {
                    m_game->loadGame();
                    fitSceneInView();
                }
            });

    m_view->setScene(m_game->scene());
}

void GameWindow::fitSceneInView()
{
    if (!m_view || !m_game || !m_game->scene()) {
        return;
    }

    const QRectF sceneRect = m_game->scene()->sceneRect();
    if (!sceneRect.isEmpty()) {
        m_view->fitInView(sceneRect, Qt::KeepAspectRatio);
    }
}
