#include "gamewindow.h"
#include "core/game.h"
#include <QCoreApplication>
#include <QDialog>
#include <QFileInfo>
#include <QGraphicsView>
#include <QHash>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QSize>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QString spriteRelativePathForShopUnit(const QString& name)
{
    if (name == QStringLiteral("战士") || name == QStringLiteral("Warrior")) {
        return QStringLiteral("assets/craftpix-reaper-man-chibi-2d-game-sprites/Reaper_Man_1/PNG/PNG "
                              "Sequences/Idle/0_Reaper_Man_Idle_000.png");
    }
    if (name == QStringLiteral("弓手") || name == QStringLiteral("Archer")) {
        return QStringLiteral(
            "assets/craftpix-satyr-tiny-style-2d-sprites/PNG/Satyr_01/PNG Sequences/Idle/Satyr_01_Idle_000.png");
    }
    if (name == QStringLiteral("法师") || name == QStringLiteral("Mage")) {
        return QStringLiteral("assets/craftpix-reaper-man-chibi-2d-game-sprites/Reaper_Man_2/PNG/PNG "
                              "Sequences/Idle/0_Reaper_Man_Idle_000.png");
    }
    if (name == QStringLiteral("预备兵") || name == QStringLiteral("Reserve")) {
        return QStringLiteral(
            "assets/craftpix-satyr-tiny-style-2d-sprites/PNG/Satyr_02/PNG Sequences/Idle/Satyr_02_Idle_000.png");
    }
    if (name == QStringLiteral("守卫") || name == QStringLiteral("Guard")) {
        return QStringLiteral(
            "assets/craftpix-satyr-tiny-style-2d-sprites/PNG/Satyr_03/PNG Sequences/Idle/Satyr_03_Idle_000.png");
    }
    return QString();
}

QPixmap loadShopPixmap(const QString& unitName)
{
    static QHash<QString, QPixmap> pixmapCache;
    if (pixmapCache.contains(unitName)) {
        return pixmapCache.value(unitName);
    }

    const QString relativePath = spriteRelativePathForShopUnit(unitName);
    if (relativePath.isEmpty()) {
        return QPixmap();
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString roots[] = {QFileInfo(appDir + "/..").canonicalFilePath(),
                             QFileInfo(appDir + "/../..").canonicalFilePath()};

    QPixmap pixmap;
    for (const QString& root : roots) {
        if (root.isEmpty()) {
            continue;
        }
        pixmap.load(root + "/" + relativePath);
        if (!pixmap.isNull()) {
            break;
        }
    }

    pixmapCache.insert(unitName, pixmap);
    return pixmap;
}

} // namespace

GameWindow::GameWindow(QWidget* parent)
    : QMainWindow(parent), m_centralWidget(new QWidget(this)), m_mainLayout(new QVBoxLayout()),
      m_view(new QGraphicsView(this)), m_resetButton(new QPushButton(QStringLiteral("重置"), this)),
      m_startCombatButton(new QPushButton(QStringLiteral("开始战斗"), this)),
      m_shopButton(new QPushButton(QStringLiteral("商店"), this)),
      m_levelUpButton(new QPushButton(QStringLiteral("升级人口"), this)),
      m_equipButton(new QPushButton(QStringLiteral("装备首件"), this)),
      m_saveButton(new QPushButton(QStringLiteral("保存"), this)),
      m_loadButton(new QPushButton(QStringLiteral("读取"), this)), m_game(new Game(this))
{
    setupUI();
    m_game->initialize();
    QTimer::singleShot(0, this, &GameWindow::fitSceneInView);
}

GameWindow::~GameWindow() = default;

void GameWindow::onResetButtonClicked()
{
    if (m_game) {
        m_game->reset();
        updateLevelUpButtonText();
        fitSceneInView();
    }
}

void GameWindow::onStartCombatButtonClicked()
{
    if (m_game) {
        m_game->startCombat();
        updateLevelUpButtonText();
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

    m_startCombatButton->setObjectName(QStringLiteral("primaryButton"));
    m_resetButton->setObjectName(QStringLiteral("resetButton"));
    m_startCombatButton->setToolTip(QStringLiteral("进入战斗阶段，单位会自动移动、攻击和施法"));
    m_resetButton->setToolTip(QStringLiteral("重置当前游戏"));
    m_shopButton->setToolTip(QStringLiteral("打开商店，查看并购买当前5个角色"));
    m_levelUpButton->setToolTip(QStringLiteral("花费金币提升等级和人口上限"));
    m_equipButton->setToolTip(QStringLiteral("把装备池中的第一件装备给当前选中的己方单位"));
    m_saveButton->setToolTip(QStringLiteral("打开存档页面，选择一个槽位保存"));
    m_loadButton->setToolTip(QStringLiteral("打开读档页面，选择一个槽位读取"));

    setStyleSheet(R"(
        QMainWindow {
            background-color: #2b2b2b;
        }
        QWidget {
            background-color: #2b2b2b;
            color: #f2f2f2;
        }
        QGraphicsView {
            background-color: #202124;
            border: 1px solid #474a50;
            border-radius: 6px;
        }
        QPushButton {
            background-color: #2f2f2f;
            color: #f2f2f2;
            border: 1px solid #565656;
            border-radius: 4px;
            padding: 6px 10px;
            font-size: 13px;
            min-height: 28px;
        }
        QPushButton#primaryButton {
            background-color: #28614c;
            border-color: #4aa77d;
            font-weight: bold;
        }
        QPushButton#resetButton {
            background-color: #3a3030;
            border-color: #735050;
        }
        QPushButton:hover {
            background-color: #3a3a3a;
        }
        QPushButton:pressed {
            background-color: #242424;
        }
        QLabel#sectionLabel {
            color: #d6d6d6;
            font-weight: bold;
            padding: 0 6px;
        }
        QWidget#controlBar {
            border-top: 1px solid #44474d;
            padding-top: 4px;
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
    controlBar->setObjectName(QStringLiteral("controlBar"));
    QVBoxLayout* controlStack = new QVBoxLayout(controlBar);
    controlStack->setContentsMargins(0, 0, 0, 0);
    controlStack->setSpacing(4);

    QHBoxLayout* actionLayout = new QHBoxLayout();
    actionLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* battleLabel = new QLabel(QStringLiteral("战斗"), controlBar);
    battleLabel->setObjectName(QStringLiteral("sectionLabel"));
    actionLayout->addWidget(battleLabel);
    actionLayout->addWidget(m_startCombatButton);
    actionLayout->addWidget(m_resetButton);
    actionLayout->addSpacing(16);
    QLabel* economyLabel = new QLabel(QStringLiteral("运营"), controlBar);
    economyLabel->setObjectName(QStringLiteral("sectionLabel"));
    actionLayout->addWidget(economyLabel);
    actionLayout->addWidget(m_shopButton);
    actionLayout->addWidget(m_levelUpButton);
    actionLayout->addWidget(m_equipButton);
   actionLayout->addStretch();
    actionLayout->addWidget(m_saveButton);
    actionLayout->addWidget(m_loadButton);
    controlStack->addLayout(actionLayout);

    m_mainLayout->addWidget(controlBar);

    connect(m_resetButton, &QPushButton::clicked, this, &GameWindow::onResetButtonClicked);
    connect(m_startCombatButton, &QPushButton::clicked, this, &GameWindow::onStartCombatButtonClicked);
    connect(m_shopButton, &QPushButton::clicked, this, [this]() { showShopDialog(); });
    connect(m_levelUpButton, &QPushButton::clicked, this, [this]() {
        if (m_game) {
            m_game->levelUp();
            updateLevelUpButtonText();
        }
    });
    connect(m_equipButton, &QPushButton::clicked, this, [this]() {
        if (m_game) {
            m_game->equipSelectedUnit();
        }
    });
    connect(m_saveButton, &QPushButton::clicked, this, [this]() { showArchiveDialog(false); });
    connect(m_loadButton, &QPushButton::clicked, this, [this]() { showArchiveDialog(true); });

    updateLevelUpButtonText();
    m_view->setScene(m_game->scene());
}

void GameWindow::updateLevelUpButtonText()
{
    if (!m_game) return;
    const int cost = m_game->levelUpCost();
    m_levelUpButton->setText(m_game->maxLevelReached()
        ? QStringLiteral("已满级")
        : QStringLiteral("升级人口(%1金)").arg(cost));
}

void GameWindow::showShopDialog()
{
    if (!m_game) {
        return;
    }

    QDialog dialog(this);
    dialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dialog.setModal(true);
    dialog.resize(860, 430);
    dialog.setStyleSheet(R"(
        QDialog {
            background-color: #24262b;
            color: #f2f2f2;
            border: 1px solid #515762;
        }
        QLabel#shopTitle {
            color: #f4f6fb;
            font-size: 18px;
            font-weight: bold;
        }
        QLabel#goldLabel {
            color: #ffd36a;
            font-weight: bold;
            padding: 4px 0;
        }
        QPushButton {
            background-color: #30343b;
            color: #f2f2f2;
            border: 1px solid #5b606b;
            border-radius: 5px;
            padding: 7px 10px;
            min-height: 30px;
        }
        QPushButton:hover {
            background-color: #3a4049;
        }
        QPushButton:disabled {
            color: #868b95;
            background-color: #292c31;
            border-color: #3f434a;
        }
        QToolButton {
            background-color: #30343b;
            color: #f2f2f2;
            border: 1px solid #606672;
            border-radius: 6px;
            padding: 10px 8px;
            font-size: 14px;
            font-weight: bold;
        }
        QToolButton:hover {
            background-color: #3a4049;
            border-color: #7b8492;
        }
        QToolButton:disabled {
            color: #858b96;
            background-color: #292c31;
            border-color: #3f434a;
        }
        QPushButton#cancelButton {
            background-color: #3a3030;
            border-color: #735050;
            min-width: 64px;
        }
        QPushButton#refreshButton {
            background-color: #2d4f42;
            border-color: #4aa77d;
            min-width: 108px;
        }
    )");

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(18, 16, 18, 18);
    mainLayout->setSpacing(12);

    QHBoxLayout* headerLayout = new QHBoxLayout();
    QLabel* titleLabel = new QLabel(QStringLiteral("商店"), &dialog);
    titleLabel->setObjectName(QStringLiteral("shopTitle"));
    QPushButton* cancelButton = new QPushButton(QStringLiteral("取消"), &dialog);
    cancelButton->setObjectName(QStringLiteral("cancelButton"));
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(cancelButton);
    mainLayout->addLayout(headerLayout);

    QLabel* goldLabel = new QLabel(&dialog);
    goldLabel->setObjectName(QStringLiteral("goldLabel"));
    mainLayout->addWidget(goldLabel);

    QLabel* unitDetailLabel = new QLabel(QStringLiteral("\u79fb\u5230\u5546\u54c1\u4e0a\u67e5\u770b\u8be6\u60c5"), &dialog);
    unitDetailLabel->setObjectName(QStringLiteral("unitDetailLabel"));
    unitDetailLabel->setWordWrap(true);
    unitDetailLabel->setStyleSheet(QStringLiteral("color:#aeb5c1; background-color:#1e2025; border:1px solid #3f434a; border-radius:4px; padding:10px; font-size:12px; min-height:48px;"));
    mainLayout->addWidget(unitDetailLabel);

    QHBoxLayout* shopLayout = new QHBoxLayout();
    shopLayout->setContentsMargins(0, 6, 0, 0);
    shopLayout->setSpacing(12);
    QVector<QToolButton*> buyButtons;
    buyButtons.reserve(GameConstants::kShopSlotCount);
    for (int i = 0; i < GameConstants::kShopSlotCount; ++i) {
        QToolButton* buyButton = new QToolButton(&dialog);
        buyButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        buyButton->setIconSize(QSize(112, 112));
        buyButton->setMinimumSize(145, 205);
        buyButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        shopLayout->addWidget(buyButton, 1);
        buyButtons.append(buyButton);
    }
    mainLayout->addLayout(shopLayout, 1);

    QHBoxLayout* footerLayout = new QHBoxLayout();
    QPushButton* refreshButton = new QPushButton(QStringLiteral("刷新(2金)"), &dialog);
    refreshButton->setObjectName(QStringLiteral("refreshButton"));
    footerLayout->addStretch();
    footerLayout->addWidget(refreshButton);
    mainLayout->addLayout(footerLayout);

    auto refreshView = [&]() {
        goldLabel->setText(QStringLiteral("当前金币：%1").arg(m_game->playerGold()));
        const QVector<QString> shopItems = m_game->shopSlots();
        for (int i = 0; i < buyButtons.size(); ++i) {
            const QString unitName = i < shopItems.size() ? shopItems.at(i) : QString();
            QToolButton* button = buyButtons.at(i);
            if (unitName.isEmpty()) {
                button->setIcon(QIcon());
                button->setText(QStringLiteral("已售出"));
                button->setToolTip(QStringLiteral("这个位置已经售出"));
                button->setEnabled(false);
                continue;
            }

            const bool canAfford = m_game->playerGold() >= GameConstants::kUnitCost;
            const QPixmap pixmap = loadShopPixmap(unitName);
            button->setIcon(pixmap.isNull()
                                ? QIcon()
                                : QIcon(pixmap.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            button->setText(canAfford ? QStringLiteral("%1\n价格：3金币").arg(unitName)
                                      : QStringLiteral("%1\n价格：3金币\n金币不足").arg(unitName));
           button->setToolTip(canAfford ? QStringLiteral("购买%1").arg(unitName)
                                        : QStringLiteral("金币不足，无法购买%1").arg(unitName));
            const QString unitInfo = m_game->unitInfoForName(unitName);
            if (!unitInfo.isEmpty()) {
                button->setToolTip(unitInfo);
            }
            button->setEnabled(canAfford);
        }
        const bool canRefresh = m_game->playerGold() >= GameConstants::kRerollCost;
        refreshButton->setText(canRefresh ? QStringLiteral("刷新(2金)") : QStringLiteral("刷新(2金不足)"));
        refreshButton->setToolTip(canRefresh ? QStringLiteral("花费2金币刷新商店")
                                             : QStringLiteral("金币不足，无法刷新商店"));
        refreshButton->setEnabled(canRefresh);
    };

    auto updateDetail = [&](int idx) {
        const QVector<QString> items = m_game->shopSlots();
        if (idx >= 0 && idx < items.size()) {
            const QString info = m_game->unitInfoForName(items.at(idx));
            if (!info.isEmpty()) {
                unitDetailLabel->setText(info);
                unitDetailLabel->setStyleSheet(QStringLiteral("color:#eef1f6; background-color:#1e2025; border:1px solid #5b606b; border-radius:4px; padding:10px; font-size:12px; min-height:48px;"));
            }
        }
    };
    for (int i = 0; i < buyButtons.size(); ++i) {
        const int btnIdx = i;
        connect(buyButtons.at(i), &QPushButton::pressed, &dialog, [btnIdx, &updateDetail]() {
            updateDetail(btnIdx);
        });
        connect(buyButtons.at(i), &QPushButton::clicked, &dialog, [this, i, &refreshView]() {
            if (!m_game) {
                return;
            }
            m_game->buyShopUnit(i);
            fitSceneInView();
            refreshView();
        });
    }

    connect(refreshButton, &QPushButton::clicked, &dialog, [this, &refreshView]() {
        if (!m_game) {
            return;
        }
        m_game->rerollShop();
        refreshView();
    });
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);

    refreshView();
    dialog.exec();
}

void GameWindow::showArchiveDialog(bool loadMode)
{
    if (!m_game) {
        return;
    }

    QDialog dialog(this);
    dialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dialog.setModal(true);
    dialog.resize(430, 340);
    dialog.setStyleSheet(R"(
        QDialog {
            background-color: #24262b;
            color: #f2f2f2;
            border: 1px solid #515762;
        }
        QLabel#archiveTitle {
            color: #f4f6fb;
            font-size: 18px;
            font-weight: bold;
        }
        QPushButton {
            background-color: #30343b;
            color: #f2f2f2;
            border: 1px solid #5b606b;
            border-radius: 5px;
            padding: 9px 12px;
            min-height: 34px;
            font-size: 14px;
        }
        QPushButton:hover {
            background-color: #3a4049;
        }
        QPushButton:disabled {
            color: #868b95;
            background-color: #292c31;
            border-color: #3f434a;
        }
        QPushButton#cancelButton {
            background-color: #3a3030;
            border-color: #735050;
            min-width: 64px;
        }
        QPushButton#slotButton {
            text-align: left;
            min-height: 68px;
            font-weight: bold;
        }
    )");

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(18, 16, 18, 18);
    mainLayout->setSpacing(12);

    QHBoxLayout* headerLayout = new QHBoxLayout();
    QLabel* titleLabel = new QLabel(loadMode ? QStringLiteral("读取存档") : QStringLiteral("保存存档"), &dialog);
    titleLabel->setObjectName(QStringLiteral("archiveTitle"));
    QPushButton* cancelButton = new QPushButton(QStringLiteral("取消"), &dialog);
    cancelButton->setObjectName(QStringLiteral("cancelButton"));
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(cancelButton);
    mainLayout->addLayout(headerLayout);

    QVBoxLayout* slotLayout = new QVBoxLayout();
    slotLayout->setContentsMargins(0, 4, 0, 0);
    slotLayout->setSpacing(10);
    for (int slot = 1; slot <= 3; ++slot) {
        QPushButton* slotButton = new QPushButton(&dialog);
        slotButton->setObjectName(QStringLiteral("slotButton"));
        const bool exists = m_game->hasSaveSlot(slot);
        const QString timeText = m_game->saveSlotTimeText(slot);
        if (loadMode) {
            slotButton->setText(exists ? QStringLiteral("存档 %1    读取\n时间：%2").arg(slot).arg(timeText)
                                       : QStringLiteral("存档 %1    空槽位\n时间：未保存").arg(slot));
            slotButton->setEnabled(exists);
        } else {
            slotButton->setText(exists ? QStringLiteral("存档 %1    覆盖保存\n时间：%2").arg(slot).arg(timeText)
                                       : QStringLiteral("存档 %1    新建保存\n时间：未保存").arg(slot));
        }

        connect(slotButton, &QPushButton::clicked, &dialog, [this, &dialog, loadMode, slot]() {
            if (!m_game) {
                return;
            }
            if (loadMode) {
                m_game->loadGame(slot);
                fitSceneInView();
            } else {
                m_game->saveGame(slot);
            }
            dialog.accept();
        });
        slotLayout->addWidget(slotButton);
    }

    mainLayout->addLayout(slotLayout, 1);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);

    dialog.exec();
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
