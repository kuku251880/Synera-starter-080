# Synera Starter

一个基于 Qt6 与 C++17 的极简自走棋原型（Starter Code），用于课程PA快速起步。
作为起步demo，主要通过"棋盘渲染" + "单位拖拽"来帮助大家快速理解/上手QT。

## 1. 项目说明
- 本demo只用于帮助大家快速上手QT，并不代表你必须使用这部分内容，部分设计实现与PA说明文档中的要求可能并不一致；
- 你可以选择自己从0开始另起框架，按照你自己的思路设计，甚至可以使用其他图形库完成PA；
- 使用此demo进行后续实现时，请务必按照PA说明文档的要求对不一致的部分做出修改(这也意味着你应该至少check一次此demo的代码)；
- 本README.md只对本demo的部分核心内容做简要说明，详情请阅读源码；
- 如有QT/C++语法使用问题，请查阅相关文档/求助AI；
- 完成PA要求时，**一切以PA说明文档为主**!!!

## 2. 当前架构

### 逻辑层（core / entity）

- `Board`：8x8 棋盘数据与占位规则
- `Unit`：最小单位数据（`id/name/position/...`，**与PA文档并不一致，需重新设计类并实现多态**）
- `Game`：协调逻辑、拖拽规则、场景构建与同步

### 渲染层（gui）

- `QGraphicsScene`：承载所有图元
- `GridItem`：单个六边形格子（含 hover/drop 高亮）
- `UnitItem`：单位图元（优先渲染 PNG，失败时回退占位图）
- `GameWindow`：主窗口（`QGraphicsView + Reset`）

## 3. 目录结构

```text
Synera_Starter/
|- CMakeLists.txt
|- README.md
|- assets/                  # 部分人物的美术资源（PNG/序列帧等,来自craftpix.net）
`- src/
   |- main.cpp
   |- core/
   |  |- board.h
   |  |- board.cpp
   |  |- game.h
   |  `- game.cpp
   |- entity/
   |  |- unit.h
   |  `- unit.cpp
   `- gui/
      |- gamewindow.h
      |- gamewindow.cpp
      |- griditem.h
      |- griditem.cpp
      |- unititem.h
      `- unititem.cpp
```

## 4. Qt 基本用法（在本项目中对应的部分）

### 4.0 Qt Workflow（从启动到交互）

本项目的 QT 工作流可以简化为：

1. `main.cpp` 创建 `QApplication`
2. 创建并显示 `GameWindow`（继承 `QMainWindow`）
3. `GameWindow` 内部创建 `QGraphicsView` 并挂载 `Game::scene()`
4. `Game` 构建 `QGraphicsScene`，向场景添加 `GridItem/UnitItem`（均继承 `QGraphicsObject`）
5. `UnitItem` 通过鼠标事件发出拖拽信号，`Game` 接收后做规则校验并更新 `Board`
6. `syncFromBoard()` 将最新数据同步回图元位置

这条链路本质上是：事件输入 -> 信号分发 -> 逻辑更新 -> 渲染同步。

### 4.1 应用入口

- `main.cpp`
  - 创建 `QApplication`
  - 创建并显示 `GameWindow`
  - 进入事件循环 `app.exec()`

### 4.2 主窗口与视图

- `GameWindow`
  - 使用 `QGraphicsView` 显示 `Game::scene()`
  - 设置基础深色样式
  - 提供 `Reset` 按钮并连接到 `Game::reset()`

### 4.3 自定义图元（QGraphicsObject）

- `GridItem` 和 `UnitItem` 继承 `QGraphicsObject`
- 核心重载：
  - `boundingRect()`：声明图元占用区域（用于重绘、裁剪、拾取）
  - `paint()`：自定义绘制
- 交互方式：
  - `UnitItem` 在鼠标按下/移动/释放时发出拖拽信号
  - `Game` 接收并执行规则校验与数据更新

### 4.4 信号槽

- `UnitItem::dragStarted/dragMoved/dragDropped`
  -> 直接连接到 `Game::handleDragStarted/handleDragMoved/handleDropCommand`

### 4.5 connect / signal-slot 机制

- `connect(sender, signal, receiver, slot)` 用于把事件发送方与处理方解耦。
- signal 只负责“通知发生了什么”，slot 负责“接下来怎么处理”。
- 本项目中，`UnitItem` 不直接改棋盘数据，而是发信号给 `Game`，由 `Game` 统一处理。
- 这样做的好处：
  - 输入层（鼠标事件）和业务层（落子规则）分离
  - 便于后续替换输入方式（键盘、网络命令等）
  - 便于调试和扩展（逻辑入口集中在 `Game`）

## 5. UI 与交互规则

### 5.1 棋盘

- 六边形网格，8x8
- 偶数行水平偏移（错列排布）

### 5.2 单位显示

- 优先加载 PNG sprite（按单位名映射资源路径）
- 资源不存在时，回退到占位绘制，保证 demo 可运行

### 5.3 拖拽规则

只有满足以下条件才允许移动：

1. 源格与目标格都在棋盘内
2. 源格与目标格都在玩家半场（`y >= ROWS / 2`）
3. 目标格为空
4. 不是原地移动
5. 源格确实是该单位

### 5.4 UI 拖拽实现

- `UnitItem::mousePressEvent` -> 发 `dragStarted`
- `UnitItem::mouseMoveEvent` -> 发 `dragMoved`
- `UnitItem::mouseReleaseEvent` -> 发 `dragDropped`
- `Game` 在 `handleDragMoved(...)` 中计算候选目标格并更新 hover/drop 高亮
- `Game` 在 `handleDropCommand(...)` 中执行合法性检查，成功后调用 `applyDrop(...)`
- 最后通过 `syncFromBoard()` 统一同步 UI

### 5.5 Z 值作用原理

- 在 `QGraphicsScene` 中，图元按 `zValue` 决定遮挡关系，值越大显示越靠上。
- 本 starter code 当前采用固定三层（在后续拖拽时你也可以选择做 2.5D 行深度设计）：
  - `GridItem`：`z = 0`
  - 普通 `UnitItem`：`z = 1`
  - 拖拽中的 `UnitItem`：`z = 2`
- 作用：
  - 保证单位始终在格子上层
  - 拖拽过程中当前单位不会被其它格子/单位遮挡

## 6. 核心函数说明

### `core/game.cpp`

- `initialize()`：初始化单位，构建场景，执行首次 reset
- `reset()`：清空棋盘并放回初始单位位置
- `buildScene()`：创建所有 `GridItem/UnitItem` 并建立信号连接
- `syncFromBoard()`：根据 `Board` 同步图元位置、可见性与 z 值
- `handleDragStarted(...)`：记录拖拽上下文并提升当前单位 z 值
- `handleDragMoved(...)`：计算目标格并更新高亮反馈
- `handleDropCommand(...)`：执行规则校验，成功后落子并同步渲染
- `canApplyDrop(...)`：拖拽合法性检查
- `applyDrop(...)`：真正更新 `Board` 数据
- `gridToWorld(...) / worldToGrid(...) / cellHexPolygon(...)`：六边形坐标变换与几何生成

### `core/board.cpp`

- `addUnit(...) / removeUnit(...)`：更新占位与单位位置
- `getUnitAt(...) / hasUnitAt(...)`：查询格子占用
- `isValidPosition(...) / isPlayerHalf(...)`：规则辅助检查
- `clear()`：清空棋盘

### `gui/unititem.cpp`

- `paint(...)`：绘制 sprite 或回退占位图
- `ensureSpriteLoaded()`：懒加载资源，避免重复读取
- `mousePressEvent/mouseMoveEvent/mouseReleaseEvent`：处理拖拽输入并发信号

### `gui/griditem.cpp`

- `paint(...)`：绘制六边形格子和交互高亮
- `setHoverActive(...) / setDropActive(...)`：高亮状态切换

## 7. 构建与运行

本项目支持两种常见方式：

1. 使用 Qt Creator IDE 打开并构建
2. 使用 CMake 在命令行自行配置与构建

### 7.1 使用 Qt Creator

1. 安装 Qt6 与 Qt Creator（官方下载安装：`https://www.qt.io/`）
2. 打开 Qt Creator，选择 `File -> Open File or Project`
3. 点击 Configure 后执行 Build / Run

### 7.2 使用 CMake 命令行

#### Linux / macOS

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
./Synera_Starter
```

#### Windows（PowerShell）

```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
.\Release\Synera_Starter.exe
```

## 8. 资源（assets）说明

- **在本PA中UI并不重要，你甚至只需要用QT painter画一些占位符即可**
- 当前demo中优先使用 PNG 静态首帧，在`assets/`下主要提供一些`2D character sprites`供大家参考
