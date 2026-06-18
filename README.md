# Synera: Synergy Auto-Arena（协同自走棋）

高级程序设计 PA 项目。基于 Qt6 + C++17 实现的单机 PVE 自走棋原型，包含准备布阵、自动战斗、商店经济、羁绊增益、升星合成、装备穿戴与随机掉落、JSON 存档读档、成就系统、利息与连胜/连败经济扩展。

学号：251880080　 姓名：贺双喜

## 1. 基本信息

- 项目阶段：阶段一至阶段三主体框架已实现；阶段四实现了利息与连胜/连败奖励扩展
- 图形库：Qt6 Widgets / `QGraphicsScene` + `QGraphicsView`
- 构建系统：CMake 3.16+，C++17
- 外部依赖：仅 Qt6（Core, Widgets, Gui），无第三方库
- 当前开发平台：Windows 11 + Qt 6.11.1 MinGW 64-bit

## 2. 功能完成度

### 阶段一：棋盘、备战区与基础单位

- **8×8 棋盘**：上半区（y = 0~3）为敌方半场，下半区（y = 4~7）为玩家半场。
- **8 格备战区**：位于棋盘下方，用于暂存新购买的单位；支持单位在棋盘与备战区之间拖拽挪位。
- **单位系统**：`Unit` 基类 + 7 个派生子类（战士、弓手、法师、预备兵、守卫、敌方战士、敌方弓手），通过工厂方法 `Unit::create()` 按名称创建。每个单位包含 HP、ATK、Range、Max Mana、Mana、技能类型（`SkillType`）、星级、装备列表和羁绊标签。
- **拖拽布阵**：准备阶段可拖拽己方单位在棋盘与备战区之间移动，非法落点（敌方半场、已占用格、超人口上限）自动回弹；拖拽时高亮所有合法落点。
- GUI 展示：棋盘格子、备战区格子、单位动画精灵、商店货架、玩家信息面板（HP/金币/等级/人口/利息/连胜连败）、战斗日志、羁绊激活状态。

### 阶段二：PVE 自动战斗

- **四阶段状态机**：`Prepare`（准备布阵）→ `PreCombat`（3-2-1 倒计时）→ `Combat`（全自动战斗）→ `PostCombat`（结算弹窗 2.5s 后自动进入下一轮）。
- **战斗循环**：`QTimer` 每 300ms 触发一次 `Game::updateCombat()`，分三趟遍历全部单位：
  1. 更新冷却与死亡状态；
  2. 存活单位依次索敌 → 施法（满蓝时）→ 攻击（在射程内时）→ 移动（BFS 寻路接近目标）；
  3. 清除死亡单位。
- **敌人系统**：每轮按 `data/enemy_waves.json` 中的模板生成敌方单位，属性随轮次线性成长；精英轮（通过 `data/events.json` 判定）额外获得 HP/ATK 加成。
- **锁敌算法**：曼哈顿距离优先，距离相同时选血量更低者，再按坐标稳定排序。
- **BFS 寻路**：`Game::nextStepToward()` 从当前位置出发，优先朝目标方向搜索可达空格，避开已占用格，搜索到进入攻击范围的格子后回溯出第一步作为本次移动目标。
- **3 种技能**：
  - 强力一击（PowerStrike）：对单体造成 `ATK × 2 + 20` 伤害。
  - 自我治疗（SelfHeal）：回复自身 45 HP，不超过最大生命值。
  - 奥术爆裂（ArcaneBurst）：对目标及其周围 1 格内所有敌方单位造成 `ATK × 1 + 20` 范围伤害。
- 一方全灭后触发 `finishCombat()`：胜利则推进轮次获得金币，失败则扣除 10 HP。

### 阶段三：经济、羁绊、升星、装备、存档

- **商店**：5 个招募位，购买单价 3 金币；支持花 2 金币刷新（reroll）；支持花金币升级人口（公式：`4 + 当前等级 × 2`，满级 8 级，最高人口 8）。
- **升星合成**：三个同名同星级单位自动合成为更高星单位（1 星 → 2 星），属性按 1.7 倍等比放大。`tryMergeUnits()` 在每次购买后自动检测并触发，支持连锁合成。
- **装备系统**：胜利后随机掉落装备进入公共装备池；选中己方单位后可穿戴装备池中第一件；每个单位最多 3 件装备。已实现 4 种装备（训练剑 +15 ATK、活力甲 +150 HP、迅捷符 -2 攻击间隔、法力护符 +30 Max Mana）。
- **羁绊系统**（4 种，战斗开始前注入临时加成）：
  - 人类 2/4：全队攻击 +10/+25。
  - 前排 2/3：前排单位生命 +120/+240。
  - 游侠 2：游侠普攻有 35% 概率追加一次半额伤害（连击）。
  - 奥术 2：奥术单位最大法力降至 20、普攻回蓝 +20、技能伤害 +25%。
- **JSON 存档**：保存到 `savegame_{slot}.json`，包含玩家状态、商店、装备池、成就、己方单位完整属性与位置。向后兼容旧版文本格式 `savegame_{slot}.txt`。支持版本迁移（migration）。

### 阶段四：扩展功能

- **利息经济**：每 10 金币产生 1 金币利息，最多 +3。
- **连胜/连败奖励**：连胜 ≥2 场额外 +1~3 金币；连败 ≥2 场额外 +1~2 金币。
- **成就系统**：初战告捷、小有积蓄（≥20 金币）、扩编成军（≥3 级）、初次升星、装备上身、连胜经济、韧性经营等。
- **战斗日志**：侧边栏最多显示 8 条带分类标签（System/Combat/Skill/Economy/SaveLoad/Trait）的彩色日志。
- **棋子动画**：根据单位状态（Idle/Walking/Slashing/Attacking/Throwing/Taunt/Dying）播放对应的序列帧精灵动画，素材在 `assets/` 下分 Reaper Man 和 Satyr 两套角色。
- **出售区域**：屏幕左侧红色方框，将单位拖入即可出售（按星级返还金币：1 星 3 金币，2 星 6 金币）。

## 3. 文件结构

```text
Synera_starter(2)/
├── CMakeLists.txt                  # CMake 构建配置 (Qt6, C++17)
├── README.md                       # 本文件
├── data/                           # JSON 数据配置
│   ├── units.json                  # 单位模板数据（预留，当前构造函数硬编码）
│   ├── skills.json                 # 技能参数配置
│   ├── traits.json                 # 羁绊阈值与加成配置
│   ├── events.json                 # 回合事件配置（普通轮/精英轮）
│   ├── enemy_waves.json            # 敌方波次成长模板
│   └── equipment.json              # 装备属性配置
├── assets/                         # 精灵动画序列帧素材
│   ├── Reaper_Man_1/               # 战士动画（Idle/Walking/Slashing/Throwing/Dying）
│   ├── Reaper_Man_2/               # 法师动画
│   ├── Reaper_Man_3/               # 敌方战士动画
│   ├── Satyr_01/                   # 弓手动画
│   ├── Satyr_02/                   # 预备兵动画
│   └── Satyr_03/                   # 守卫/敌方弓手动画
├── tools/
│   └── verify_project.ps1          # PowerShell 项目验证脚本
└── src/
    ├── main.cpp                    # 入口：创建 QApplication + GameWindow
    ├── core/
    │   ├── board.h / board.cpp     # 8×8 棋盘数据结构（占位/移除/合法性查询）
    │   └── game.h / game.cpp       # 游戏主控制器（~2900 行）：全部核心逻辑
    ├── entity/
    │   ├── unit.h / unit.cpp       # Unit 类层次 + 7 个派生子类 + 工厂方法
    │   ├── player.h / player.cpp   # 玩家状态（HP/金币/等级/人口/连胜连败）
    │   ├── skill.h / skill.cpp     # 技能接口 + 3 种技能实现 + JSON 配置加载
    │   └── equipment.h / equipment.cpp  # 装备系统 + JSON 配置加载
    └── gui/
        ├── gamewindow.h / gamewindow.cpp  # 主窗口 UI（按钮/面板/对话框）
        ├── griditem.h / griditem.cpp      # 棋盘/备战区格子的 QGraphicsItem
        └── unititem.h / unititem.cpp      # 单位的 QGraphicsObject（动画/拖拽/状态条）
```

## 4. 核心类与数据结构

### 类架构

| 类 | 职责 | 关键成员/方法 |
|---|---|---|
| `Game` | 全局游戏控制器，管理状态机、战斗 AI、商店、合成、羁绊、存档 | `startCombat()`, `buyShopUnit()`, `updateCombat()`, `tryMergeUnits()`, `saveGame()`, `loadGame()` |
| `Board` | 8×8 棋盘：`QVector<Unit*>(64)` + `QHash<Unit*, QPoint>` 双向索引 | `addUnit()`, `removeUnit()`, `getUnitAt()`, `isValidPosition()`, `clear()` |
| `Unit` | 战斗单位：基础属性 + 临时羁绊加成 + 状态冷却 | `setTraitBonuses()`, `clearTraitBonuses()`, `resetCombatState()` |
| `Player` | 玩家状态 | HP/Gold/Level/PopulationLimit/CurrentRound/WinStreak/LossStreak/ownedUnitIds |
| `Equipment` | 装备类型（TrainingSword/VitalityArmor/SwiftCharm/ManaTalisman） | `applyTo(Unit*)`, `removeFrom(Unit*)` |
| `Skill` | 技能抽象基类 → 3 个子类 | `cast(caster, target, units, applyDamage, gridDistance, addLog, flashSkill)` |
| `UnitItem` | 单位的 QGraphicsObject，负责精灵动画、血条/蓝条、拖拽、攻击/受击/治疗闪烁 | `paint()`, `ensureAnimationLoaded()`, `flashAttack()`, `flashDamage()` |
| `GridItem` | 棋盘/备战区格子的 QGraphicsItem，支持悬停高亮、可放置提示 | `setHoverActive()`, `setDropActive()` |
| `GameWindow` | QWidget 主窗口，布局所有操作按钮与信息面板 | `startCombat()`, `refreshShop()`, `levelUp()`, `saveGame()`, `loadGame()` |

### 关键常量（`GameConstants` 命名空间）

| 常量 | 值 | 说明 |
|---|---|---|
| `kCombatTickIntervalMs` | 300 | 战斗时钟周期（ms） |
| `kMoveCooldown` | 4 | 移动冷却（tick 数） |
| `kDefaultAttackInterval` | 8 | 基础攻击间隔（tick 数） |
| `kManaGainPerAttack` | 30 | 普攻回蓝量 |
| `kStarUpFactorNumerator / Denominator` | 17 / 10 | 升星属性倍率（×1.7） |
| `kMaxStarLevel` | 2 | 最高星级 |
| `kMaxEquipmentPerUnit` | 3 | 单单位装备上限 |
| `kInitialPlayerHp` | 100 | 初始血量 |
| `kInitialPlayerGold` | 10 | 初始金币 |
| `kInitialPopulation` | 4 | 初始人口上限 |

### 属性分层机制

单位属性分为"基础永久层"和"羁绊临时层"：

- **基础层**（`setMaxHp` / `setAtk` / `setRange` / `setMaxMana`）：存储初始模板 + 升星奖励 + 装备属性，通过 `baseMaxHp()` / `baseAtk()` 读取。
- **羁绊临时层**（`m_traitMaxHpBonus` 等 7 个字段）：战斗前由 `refreshTraitBonuses()` 写入，战斗后清除。

关键设计：`maxHp()` 返回 `m_maxHp + m_traitMaxHpBonus`，`atk()` 返回 `m_atk + m_traitAtkBonus`。升星时只放大基础层（`baseMaxHp() * 1.7`），羁绊加成始终保持精准独立。

## 5. 关键算法

### 战斗循环（`Game::updateCombat()`）

```
每 300ms:
  遍历 1：更新所有单位冷却 → 标记死亡
  遍历 2：存活单位 →
    ├─ 位置不合法？跳过
    ├─ 法力满？→ castSkill() → 跳过
    ├─ 目标在射程内？→ attackTarget()
    └─ 否则 → moveUnitToward()（BFS 寻路）
  遍历 3：清除死亡单位
  判定胜负 → 触发 finishCombat()
```

### 锁敌算法（`nearestEnemyFor()`）

遍历所有敌方存活且位置合法的单位，按优先级排序：
1. 曼哈顿距离最近
2. 距离相同时 HP 更低
3. HP 也相同时 Y 坐标更小（靠近上方）
4. 全部相同时 X 坐标更小

### BFS 寻路（`nextStepToward()`）

从单位当前位置开始 BFS，按朝向目标方向的优先级顺序探索相邻格。搜索到进入攻击范围（`gridDistance ≤ range`）的空格后停止，回溯找到第一步移动方向。只允许移动到空格（空闲的棋盘格），避免穿人。

### 羁绊计算（`traitCounts() + refreshTraitBonuses()`）

1. 统计棋盘上存活己方单位的标签计数（如 `{"前排": 3, "人类": 2, "游侠": 2}`）
2. 将计数与 `data/traits.json` 中的阈值匹配，选出生效的羁绊等级
3. 为场上己方单位注入对应的临时加成（`setTraitBonuses()`）

### 升星合成（`tryMergeUnits()`）

```cpp
while (存在可合成组):
  按 "名字#星级" 分组
  组内 ≥3 个 →
    保留第 0 个 → upgradeUnitStar()（星级+1，HP/ATK ×1.7）
    删除第 1、2 个（从棋盘、备战区、Player、m_units 中清除）
    重建场景
```

### JSON 存档格式（版本 2）

```json
{
  "format": "SyneraSave",
  "version": 2,
  "savedAt": "2026-06-18T12:00:00",
  "player": { "hp": 100, "gold": 10, "level": 1, "populationLimit": 4, "currentRound": 1, "winStreak": 0, "lossStreak": 0 },
  "shop": ["战士", "弓手", "", "法师", "预备兵"],
  "equipment": ["训练剑", "活力甲"],
  "achievements": ["初战告捷"],
  "units": [
    {
      "name": "战士", "starLevel": 1, "hp": 120, "maxHp": 120, "atk": 14, "range": 1,
      "maxMana": 80, "mana": 0, "attackInterval": 8, "skillType": "PowerStrike",
      "traits": ["前排", "人类"], "equipmentNames": [],
      "location": { "type": "board", "x": 0, "y": 7 }
    }
  ]
}
```

加载时优先读取 JSON 格式；若不存在则尝试读取旧版文本格式 `savegame_{slot}.txt`。

## 6. 构建与运行

### 前置要求

- Qt 6.x（推荐 6.11.1）+ MinGW 64-bit（Windows）或 Clang/GCC（Linux/macOS）
- CMake 3.16+
- C++17 编译器

### Qt Creator（推荐）

1. 打开 Qt Creator → File → Open File or Project
2. 选择 `CMakeLists.txt`
3. 选择 Qt6 Kit（如 Desktop Qt 6.11.1 MinGW 64-bit）
4. 点击 Build → Run

### 命令行

```powershell
# 配置（Windows + Qt 6.11.1 MinGW）
cmake -S . -B build -G "Ninja" -DCMAKE_PREFIX_PATH=C:\Qt\6.11.1\mingw_64

# 编译
cmake --build build

# 运行
.\build\Synera_Starter.exe
```

当前开发环境构建目录：`build/Desktop_Qt_6_11_1_MinGW_64_bit-Debug/`

### 验证脚本

```powershell
powershell -ExecutionPolicy Bypass -File tools/verify_project.ps1
```

## 7. 操作说明

### 准备阶段
- **购买单位**：点击商店中 5 个招募位之一（3 金币），单位进入备战区
- **刷新商店**：点击"刷新商店"按钮（2 金币）
- **升级人口**：点击"升级人口"按钮（费用显示在按钮上）
- **布阵**：从备战区或棋盘拖拽单位到目标位置——绿色高亮 = 可放置，无高亮 = 不可放置（会自动回弹）；拖入左侧红色"出售"区域即售出
- **穿戴装备**：选中己方单位后点击"穿戴装备"按钮，装备池中第一件装备会穿到该单位身上
- **开始战斗**：点击"开始战斗"按钮（至少需要 1 个单位在棋盘上）

### 战斗阶段
- 全自动进行，无需操作
- 屏幕上方显示敌方半场，下方显示己方半场
- HP 条和 MP 条实时更新

### 战后阶段
- 战斗结束后弹出结算卡片（2.5 秒自动关闭）
- 胜利：进入下一轮，商店刷新（不免费），装备掉落
- 失败：扣除 10 HP，商店刷新（不免费）
- 新的敌人按轮次生成并成长

### 存档 / 读档
- 点击"保存"按钮保存到默认槽位
- 点击"读档"按钮从默认槽位读取
- 支持 3 个槽位（slot 0/1/2）

## 8. 资源说明

`assets/` 中的角色精灵素材来自 CraftPix.net。素材采用序列帧 PNG 格式，按角色和动作分目录存放。若素材文件缺失或加载失败，`UnitItem` 会回退到 Qt 绘制的几何体占位符（六边形 + 首字符），不影响游戏逻辑正常运行。

## 9. AI 使用说明

本项目在开发过程中使用了 AI 辅助工具（Claude Code）进行代码审查、错误检测、代码重构和文档完善。

### AI 辅助的工作范围

1. **代码审查与 Bug 修复**：AI 对整个代码库进行了静态分析，发现并修复了以下问题：
   - `buildScene()` 中 `m_scene->clear()` 后未将 `m_countdownOverlay`、`m_countdownText`、`m_resultOverlay`、`m_resultText` 置空，导致升星后开始战斗时对悬垂指针调用 `setVisible()` 而崩溃
   - `reset()` 中未清理旧的 `m_units`、`m_benchSlots`、`Player` 单位 ID，且未重新调用 `createStarterUnitsIfNeeded()`，导致售出初始角色并重置后不再出现
   - `reset()` 中 `buildScene()` 后缺少 `syncFromBoard()` 调用，导致初始角色全部显示在左上角
2. **代码重构**：AI 协助将单位系统从数据驱动重构为 OOP 类继承体系，并优化了 `game.cpp` 中的辅助函数和内部类型组织。
3. **文档完善**：AI 协助补充了完整的 README（包含类架构表、属性分层机制说明、操作指南）和存档格式文档。

### 核心模块运行原理（自行编写部分）

以下模块为本人在课程期间独立设计与实现：

- **战斗循环**：`QTimer` 以 300ms 为周期驱动 `Game::updateCombat`。每 Tick 先更新所有单位的攻击/移动冷却，标记死亡单位；然后依次遍历存活单位进行索敌、施法（法力满时）、攻击（在攻击范围内时）或移动（BFS 寻路接近目标）；最后检测是否有一方全灭以决定胜负结算。
- **BFS 寻路**：`Game::nextStepToward` 从单位当前位置出发，按曼哈顿距离向目标方向优先搜索可到达的空格，避免穿过已占用格子。搜索到进入攻击范围的最短路径后，回溯到第一步作为本次移动目标，每次移动一格。
- **羁绊加成**：战斗开始前，`traitCounts` 统计棋盘上存活己方单位的标签计数；`activeTraitBonuses` 根据阈值选出生效的羁绊效果；`refreshTraitBonuses` 为每个单位累加团队攻击加成和自身标签匹配的 HP/法力/技能等加成。

### 代码理解与检查

本人已逐行审阅了 AI 修改与重构的代码，确认所有修改均正确且不影响原有功能逻辑。AI 生成的 README 内容已根据项目实际实现核对，确保描述准确。本人完全掌握项目的整体架构及各模块的数据流。
