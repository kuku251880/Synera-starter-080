#ifndef CORE_GAME_H
#define CORE_GAME_H

// 引入 Qt 核心工具箱与自定义底层类
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
#include "entity/skill.h"

// 前置声明：告诉编译器这些图形和核心类待会儿会用到
class Unit;
class QGraphicsScene;
class QGraphicsTextItem;
class QGraphicsRectItem;
class QTimer;
class GridItem;
class UnitItem;

// ─── 核心游戏数值常量（方便统一调整平衡性） ─────────────────────────
namespace GameConstants {

// 经济系统配置
inline constexpr int kUnitCost = 3;             // 棋子买入价：3金币
inline constexpr int kRerollCost = 2;           // 刷新商店价：2金币
inline constexpr int kMaxLevel = 8;             // 满级：8级
inline constexpr int kInitialPopulation = 4;    // 开局基础人口上限：4人
inline constexpr int kMaxPopulation = 8;        // 最高上阵人数：8人
inline constexpr int kInitialPlayerHp = 100;    // 玩家初始血量：100
inline constexpr int kInitialPlayerGold = 10;   // 玩家初始金币：10
inline constexpr int kVictoryGold = 5;          // 赢了给 5 金币
inline constexpr int kLossGold = 2;             // 输了给 2 金币
inline constexpr int kHpLossOnDefeat = 10;      // 输了扣 10 血
inline constexpr int kInterestDivisor = 10;     // 利息发利息的存款基数（每10金币给1金币利息）
inline constexpr int kInterestMax = 3;          // 利息封顶：最多拿3金币
inline constexpr int kWinStreakBonusCap = 3;    // 连胜奖金封顶：3金币
inline constexpr int kLossStreakBonusCap = 2;   // 连败奖金封顶：2金币
inline constexpr int kStreakThreshold = 2;      // 触发连胜连败的最小门槛：2场

// 自动战斗时钟配置
inline constexpr int kCombatTickIntervalMs = 300; // 战斗时钟频率：每 300毫秒 判定一次行为（1个Tick）
inline constexpr int kMoveCooldown = 4;           // 移动冷却：走一步要等4个Tick
inline constexpr int kDefaultAttackInterval = 8;  // 基础攻速：打一下要等8个Tick
inline constexpr int kManaGainPerAttack = 30;     // 普攻回蓝：每次打人回30蓝
inline constexpr int kMaxEquipmentPerUnit = 3;    // 装备上限：每个棋子最多带3件装备

// 界面网格与视口布局
inline constexpr int kBenchSlotCount = 8;        // 备战区格子数：8个
inline constexpr int kEnemyDeployCount = 2;       // 敌方每轮上场人数：2人
inline constexpr qreal kCellSize = 64.0;         // 每个正方形格子的大小：64像素
inline constexpr qreal kCellGap = 4.0;           // 棋盘格子的间隙：4像素
inline constexpr qreal kBenchGap = 52.0;         // 备战区和棋盘之间的空隙：52像素

// 商店配置
inline constexpr int kShopSlotCount = 5;          // 商店货架位置：5个

// 升星倍率（1星变2星：数值乘以 1.7）
inline constexpr int kStarUpFactorNumerator = 17;
inline constexpr int kStarUpFactorDenominator = 10;

// 成就解锁条件阈值
inline constexpr int kGoldSaveThreshold = 20;     // 攒够20金币解锁成就
inline constexpr int kLevelUpThreshold = 3;       // 升到3级解锁成就
inline constexpr int kStreakAchievementThreshold = 3; // 连胜连败3场解锁成就

// 局内养成星级限制
inline constexpr int kMaxStarLevel = 2;           // 最高支持合到 2 星（3个1星合成1个2星）

// 计算升级人口所需的金币（逐级递增公式：4 + 当前等级 * 2）
inline constexpr int levelUpCost(int currentLevel) {
    if (currentLevel >= kMaxLevel) return 999;
    return 4 + currentLevel * 2;
}

// 战斗日志行数上限
inline constexpr int kMaxLogCount = 8;

} // namespace GameConstants

// 游戏阶段状态机
enum class GamePhase
{
    Prepare,     // 准备阶段：买兵、调站位、穿装备
    PreCombat,   // 战前阶段：3-2-1 倒计时
    Combat,      // 战斗阶段：全自动对弈打架中
    PostCombat,  // 战后阶段：屏幕弹窗显示胜利/失败结果
    Resolve      // 结算阶段：发利息、加回合数
};

// 战斗日志分类标签
enum class LogCategory
{
    System,      // 系统提示
    Combat,      // 普通攻击/阵亡
    Skill,       // 释放大招
    Economy,     // 买卖、刷新、升级
    SaveLoad,    // 存读档
    Trait        // 羁绊/成就
};

// 日志单条数据结构
struct GameLog
{
    QString message;
    LogCategory category;
};

// ─── 全游戏中央大脑类 ──────────────────────────────────────────────
class Game : public QObject
{
    Q_OBJECT

public:
    explicit Game(QObject* parent = nullptr);
    ~Game();

           // ─── 供界面层（GUI）调用的核心公开接口 ───────────────────────────
    void initialize();                           // 游戏总初始化：发开局棋子、刷商店
    void reset();                                // 一键重置：开新局、资产清空
    void startCombat();                          // 玩家点击“开始战斗”的入口
    void buyShopUnit(int slot);                  // 买兵：点击货架格子扣钱买兵
    void rerollShop();                           // 刷商店：花2金币重刷5个商品
    void levelUp();                              // 买经验：花钱升级人口上限
    void equipSelectedUnit();                    // 给选中的棋子穿上装备池里第一件装备
    void saveGame();                             // 保存到默认槽位
    void saveGame(int slot);                     // 保存游戏到指定数字槽位
    void loadGame();                             // 读取默认槽位
    void loadGame(int slot);                     // 从指定数字槽位读取游戏
    int levelUpCost() const { return GameConstants::levelUpCost(m_player.level()); } // 查现在升级要多少钱
    bool maxLevelReached() const { return m_player.level() >= GameConstants::kMaxLevel; } // 问满级了没有
    void sellSelectedUnit(int unitId = -1);      // 卖兵：把棋子销毁并返还金币
    QString unitInfoForName(const QString& name) const; // 查商店里某个兵种的详细身材属性（用于悬停提示）
    bool hasSaveSlot(int slot) const;            // 问某个槽位有没有对应的存盘文件
    QString saveSlotTimeText(int slot) const;    // 拿某个存档文件的最后修改时间

    QGraphicsScene* scene() const { return m_scene; } // 拿渲染游戏画面的核心画布指针
    QVector<QString> shopSlots() const { return m_shopSlots; } // 拿当前商店里的5个商品名字
    int playerGold() const { return m_player.gold(); } // 拿玩家身上的现金

           // ─── 鼠标拖拽棋子排兵布阵的同步接口 ──────────────────────────────
    void handleDragStarted(int unitId, const QPoint& sourceGrid, const QPointF& scenePos); // 鼠标刚抓起棋子
    void handleDragMoved(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);   // 鼠标拖着棋子移动中
    void handleDropCommand(int unitId, const QPoint& sourceGrid, const QPointF& scenePos); // 鼠标松开、放下棋子
    void handleUnitSelected(int unitId);                                                 // 鼠标点击，选中了某个棋子

private:
    // ─── 局内运营与养成私有流 ────────────────────────────────────────
    void setupRoundBoard(bool preservePlayerLayout = false); // 每轮重新布置棋盘（生成新敌人、回收我方位置）
    void createStarterUnitsIfNeeded();           // 开局保底：如果手里没兵，送几个基础兵
    Unit* createUnitFromTemplate(const QString& name, UnitOwner owner) const; // 调用工厂真正去 new 一个棋子
    QStringList unitPool() const;                // 本作支持的白名单兵种池
    void rollShop();                             // 摇随机数，往商店货架上塞5个随机兵种
    int firstEmptyBenchSlot() const;             // 找备战区最左边空着的格子下标
    bool addUnitToBench(Unit* unit);             // 把刚造好的棋子塞进备战区空位
    void tryMergeUnits();                        // 自动合成流：扫描并触发“三合一”星级提升
    void upgradeUnitStar(Unit* unit);            // 提升单体星级，并等比放大数值
    QHash<QString, int> traitCounts() const;     // 统计当前棋盘上存活的我方职业标签数量（如人类:3, 游侠:2）
    void refreshTraitBonuses();                  // 重新计算并给符合条件的上阵棋子注入羁绊buff
    QString activeTraitsText() const;            // 把当前激活的羁绊拼成文字（用于界面显示）
    Equipment randomEquipment() const;            // 随机摇一件装备
    Equipment equipmentFromName(const QString& name) const; // 通过名字实例一件装备
    void generateEnemyRound(int round);          // 根据当前关卡数，动态生成/变强本轮的敌方棋子

    // ─── 精准索引与快速查找私有流 ─────────────────────────────────────
    Unit* findUnitById(int unitId) const;        // 通过唯一身份证号找到棋子数据指针
    GridItem* findGridItem(const QPoint& gridPos) const; // 通过行和列坐标找到界面上的格子格子对象
    UnitItem* findUnitItem(int unitId) const;    // 通过身份证号快速找到它对应的屏幕上会动的图形棋子
    int benchIndexOf(Unit* unit) const;          // 查某个棋子在备战区的第几个位置
    Unit* unitAtGrid(const QPoint& gridPos) const; // 融合查：不管棋盘还是备战区，给我坐标，查出是谁
    int playerBoardUnitCount() const;            // 数数玩家当前在**棋盘上（不含备战区）**丢了多少个兵

    // ─── 界面高亮与拖拽提示流 ────────────────────────────────────────
    void clearGridHighlights();                  // 一键关闭所有格子的鼠标悬停、可放置等高亮颜色
    void showDropHints(int unitId, const QPoint& source, const QPoint& hoverTarget); // 计算并高亮可以落子的绿色区域
    bool isBoardPosition(const QPoint& gridPos) const; // 判断坐标是不是落在 8x8 棋盘内
    bool isBenchPosition(const QPoint& gridPos) const; // 判断坐标是不是落在 1x8 备战区内
    bool canApplyDrop(int unitId, const QPoint& source, const QPoint& target) const; // 校验拖拽落子动作合不合法（是否超人口等）
    void applyDrop(int unitId, const QPoint& source, const QPoint& target); // 逻辑上正式改变棋子在棋盘/备战区的内部映射

    // ─── 核心全自动战斗 AI 逻辑流（updateCombat 定时驱动） ──────────────
    void updateCombat();                         // ⚔️ 核心战斗心跳：每 300ms 指挥全场做一次行为动作决策
    Unit* nearestEnemyFor(Unit* unit) const;     // 锁敌算法：寻找离自己最近、血量最低的敌人
    int gridDistance(Unit* a, Unit* b) const;     // 算两个棋子之间的格子曼哈顿距离
    int gridDistance(const QPoint& a, const QPoint& b) const; // 算两个平面格子坐标之间的纯数学距离
    QPoint nextStepToward(Unit* unit, Unit* target) const; // 🤖 寻路算法核心：基于 BFS 算棋子下一步该往哪挪
    void moveUnitToward(Unit* unit, Unit* target); // 执行移动：让棋子往目标走一格，并播屏幕动画
    void attackTarget(Unit* unit, Unit* target);  // 普通攻击：挥刀打人、扣敌血、自己回蓝、判定游侠连击
    void castSkill(Unit* unit, Unit* target);     // 满蓝开大：改变状态机、蓝量清零、调用大招功能
    void applyDamage(Unit* target, int damage);   // 扣血流：扣减目标HP、触发受击红闪、判定阵亡死亡状态
    bool sideDefeated(UnitOwner owner) const;    // 判定全歼：看玩家或敌人是不是死光了

    // ─── 局内经济结算与机制流 ────────────────────────────────────────
    void finishCombat(bool playerWon);           // 战斗打完：停时钟、发基础钱、发利息和连胜/连败连击钱、扣血
    int interestGold() const;                    // 算利息钱数（存款/10，最多3金币）
    int streakBonusGold(bool playerWon) const;    // 算玩家连胜或连败能额外拿多少奖金
    QString currentEventForRound(int round) const; // 获取本回合随机事件的文字描述
    void updateRoundEvent();                     // 触发回合事件逻辑（如丰收回合额外送钱）

    // ─── 序列化存储工具流 ───────────────────────────────────────────
    QString saveFileName(int slot) const;        // 新版 JSON 存档的文件名拼接
    QString legacySaveFileName(int slot) const;  // 旧版 TXT 存档的文件名拼接
    void loadJsonSaveData(const QByteArray& saveData); // 解析 JSON 字符串数据恢复游戏
    void loadLegacySaveData(const QByteArray& saveData); // 兼容解析旧版 TXT 文本恢复游戏
    void finalizeLoadedGame(int slot);           // 读档最后收尾：重新生成敌人、刷新界面场景

    // ─── 局内系统反馈流 ─────────────────────────────────────────────
    void addLog(const QString& message, LogCategory category = LogCategory::System); // 往侧边栏塞一条彩色的战斗历史日志
    void unlockAchievement(const QString& name); // 解锁成就系统
    void checkAchievements();                    // 定期扫描并解锁成就（如满现金、初次升星等）
    QString phaseName() const;                   // 把游戏状态（Prepare等）转换成大白话中文
    QString stateName(UnitState state) const;    // 把棋子状态（Idle等）转换成大白话中文
    QString skillName(SkillType skillType) const; // 拿大招技能的中文官方名字

    // ─── 动态弹窗与视觉倒计时流 ──────────────────────────────────────
    void startCountdown();                       // 启动战前倒计时定时器
    void tickCountdown();                        // 倒计时每走一秒刷新一下屏幕中间的超大数字
    void showResultOverlay(bool playerWon);      // 战斗刚完时，在画布正中弹出一张巨大的华丽结算卡片
    void dismissResultOverlay();                 // 2.5秒后自动关闭大结算卡片，正式切回准备阶段

    // ─── 屏幕像素坐标（世界）与网格坐标（格子）的转换算法 ───────────────────
    void buildScene();                           // 核心渲染：在画布上画出 8x8 棋盘、备战格子、信息文本面板
    void syncFromBoard();                        // 双向渲染同步：读取底层棋子坐标，让屏幕上的动画对象飞过去
    void updateInfoPanel();                      // 刷新两侧的大文本说明板（展示选中的棋子三围、激活的羁绊列表）

    QPointF gridToWorld(int row, int col) const; // 数学公式：输入行和列，算出它在屏幕上的像素点中心位置坐标
    QPointF benchToWorld(int slot) const;        // 数学公式：输入备战区几号位，算出屏幕对应的像素位置坐标
    QPoint worldToGrid(const QPointF& world) const; // 数学公式：鼠标点击屏幕像素 $(X, Y)$，精准反算出玩家点的是哪个格子
    QPolygonF cellRectPolygon(int row, int col) const; // 捏出一个格子的四边形多边形碰撞框（用于选区渲染）
    QPolygonF benchCellPolygon(int slot) const;  // 捏出一个备战格子的四边形框

           // ─── 【底层数据口袋（私有成员变量）】 ──────────────────────────────
    Board m_board;                               // 刚刚搞懂的 8x8 空间账本
    Player m_player;                             // 玩家的血量现金资产账本
    QList<Unit*> m_units;                        // 全局棋子实体容器（装了场上所有活着的敌我棋子）
    QVector<Unit*> m_benchSlots;                 // 长度为 8 的一维数组，对应备战区的坑位
    QVector<QString> m_shopSlots;                // 长度为 5 的字符串列表，存货架上兵的名字
    QVector<Equipment> m_equipmentPool;          // 玩家目前拥有的没穿戴的公共装备池

           // ─── Qt 图形底层对象 ───────────────────────────────────────────
    QGraphicsScene* m_scene;                     // 画布老大，所有的格子、血条、棋子全丢进这里渲染
    QGraphicsTextItem* m_leftInfoPanel;          // 左侧系统信息说明板
    QGraphicsTextItem* m_infoPanel;              // 右侧兵种信息说明板
    QTimer* m_combatTimer;                       // 战斗 300ms 循环大时钟
    QTimer* m_countdownTimer;                    // 3-2-1 倒计时的独立时钟
    QTimer* m_resultTimer;                       // 大卡片弹窗停留 2.5秒 的定时器
    QGraphicsRectItem* m_resultOverlay;          // 遮罩层：大卡片背后的半透明黑色幕布
    QGraphicsTextItem* m_resultText;             // 卡片里的多行彩色富文本
    QGraphicsRectItem* m_countdownOverlay;       // 倒计时半透明幕布
    QGraphicsTextItem* m_countdownText;          // 倒计时中间的大数字图形
    int m_countdownValue;                        // 当前倒计时数值（3，2，1）
    std::vector<GridItem*> m_gridItems;          // 场上 64 块地皮图形的集合
    std::vector<UnitItem*> m_unitItems;          // 场上所有会动的棋子动画图形集合

           // ─── 局内状态标记 ──────────────────────────────────────────────
    bool m_dragActive;                           // 标记玩家现在是不是正用鼠标按着某个棋子在空中拖拽
    int m_activeUnitId;                          // 当前正在被拖拽的那个棋子的身份证号
    int m_selectedUnitId;                        // 当前被玩家鼠标点中、框选起来的那个棋子的身份证号
    QPoint m_sourceGrid;                         // 正在拖拽的棋子是从哪个老家格子（起跑点）抓起来的
    GamePhase m_phase;                           // 全局目前处于准备、打架还是结算阶段
    QString m_lastResult;                        // 界面全局提示语文本
    QString m_currentEvent;                      // 存放本轮触发的随机事件描述
    QStringList m_achievements;                  // 当前玩家已经解锁的所有成就花名册
    QVector<GameLog> m_logs;                     // 缓存最近 8 条日志的历史垃圾桶
    int m_eventRewardRound;                      // 记录上一次发事件奖金是第几轮（防止一轮重复发钱）
    QRectF m_sellZoneRect;                       // 屏幕左侧红色“出售区域”的像素范围框
    QGraphicsRectItem* m_sellZoneItem;           // 出售区域的红色半透明方块图形
    QGraphicsTextItem* m_sellZoneText;           // 出售区域里的“拖动到此处出售”文本
    std::unordered_map<int, UnitItem*> m_unitItemById; // 身份证号到图形对象的超高速映射雷达

           // ─── 空间布局缓存数据 ───────────────────────────────────────────
    int m_rows;                                  // 行数
    int m_benchSlotCount;                        // 备战坑位数量
    qreal m_cellSize;                            // 格子像素大小
    qreal m_cellGap;                             // 格子间隙像素
    qreal m_benchGap;                            // 备战区高度偏移
};

#endif // CORE_GAME_H