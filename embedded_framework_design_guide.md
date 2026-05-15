# Embedded Framework Design Guide

本文档用于指导嵌入式项目的长期架构演进。它不是目录命名规范，也不是要求一次性重构的蓝图，而是一组判断边界、依赖方向、数据流和资源归属的准则。

当前仓库是基于 STM32H723、STM32Cube HAL、FreeRTOS、双 MCU（Chassis/Gimbal）的机器人控制程序。本文档保留通用原则，同时补充 Infantry 项目当前暴露出的维护性问题和可执行的演进路线。

## 0. 如何阅读这份文档

优先理解这些问题，而不是先改目录：

- 这个模块说的是硬件语言、能力语言还是业务语言？
- 这个模块主动拥有线程/中断/队列/ DMA buffer 吗？
- 这个模块的输入是命令、事件、连续数据还是状态快照？
- 这个模块未来最可能因为硬件、协议、业务规则、调参还是调试方式变化？
- 一个改动是否需要同时理解 HAL、FreeRTOS、CAN、FSM、PID 和业务规则？如果需要，边界已经不清晰。

本文档中的建议分为两类：

- 架构目标：长期应靠近的方向。
- 迁移步骤：当前项目可以逐步完成的低风险改动。

不要把 `ports/`、`domain/`、`services/` 机械套成目录。先把职责划清，再决定是否移动文件。

## 1. 核心设计目标

框架目标不是增加目录层级，而是建立清晰边界：

- 明确定义每一层的职责、依赖方向和允许暴露的信息。
- 让硬件驱动、业务逻辑、平台服务、调试接口相互解耦。
- 让资源申请、释放、所有权、并发访问方式可追踪。
- 让全局数据流动从隐式共享变量转为显式命令、事件、连续数据和状态快照。
- 让调试时可以观察结构化状态，而不破坏模块封装。
- 让同一套思想可以落地在 Zephyr、STM32Cube HAL + FreeRTOS 或裸机框架上。

核心原则：

```text
下层说硬件语言，上层说业务语言，中间层只负责翻译和编排。
```

如果一个文件里同时大量出现寄存器、DMA、GPIO、中断细节、FreeRTOS 调度、CAN frame、PID、FSM 和业务概念，通常说明层级边界已经混乱。

## 2. 关键术语解释

### 2.1 Domain

`domain` 是纯业务模型和纯算法规则。它不应该依赖 HAL、FreeRTOS、具体 UART/CAN/SPI，也不应该知道板子引脚。

在机器人项目中，适合放入 domain 的内容包括：

- `ChassisCmd`、`GimbalCmd`、`ShootCmd` 这类命令模型。
- 运动学、热量规则、弹速补偿、功率限制等纯算法。
- FSM 的状态枚举、状态转移条件、业务常量。
- 协议 payload 的纯结构定义，但不包括 UART/CAN 收发细节。

不适合放入 domain 的内容：

- `HAL_UARTEx_ReceiveToIdle_DMA`。
- `xTaskGetTickCount`。
- `FDCAN_TxHeaderTypeDef`。
- `GPIO_Pin`。
- DMA buffer、ISR callback。

### 2.2 Service / Application

`service` 或 `application` 是业务编排层。它可以拥有线程、状态机和运行时状态，负责把输入命令转化为输出目标。

当前项目里的 `FireCtrlApp`、`MovtionCtrlApp`、`CommanderSrvc` 都是 service/application。它们的问题不是名字叫 App 还是 Service，而是有些文件同时承担了过多职责：输入解释、状态机、黑板读写、PID、硬件对象访问、调试变量更新混在一起。

### 2.3 Port

`port` 是上层需要的能力接口，用于隔离真实变化。它不是所有设备都必须有的抽象。

适合定义 port 的情况：

- 上层不应该知道具体设备型号。
- 存在多个实现，例如 DR16、VideoLink、Gamepad 都是 remote input。
- 需要单元测试 mock。
- 设备或平台未来可能替换。
- 上层依赖的是能力，而不是某个具体硬件。

不适合定义 port 的情况：

- 当前只有一个实现，并且没有测试替身需求。
- 抽象后只剩空泛的 `init/read/write`。
- 需要频繁暴露具体硬件能力，抽象反而隐藏关键约束。

### 2.4 Driver

`driver` 面向具体设备或外设机制。它负责硬件可靠工作，不负责业务策略。

驱动应负责：

- 初始化、寄存器/总线事务、时序约束。
- 中断下半部通知或内部 worker。
- DMA/cache/buffer 对齐和并发保护。
- 错误检测、错误码转换和基础恢复。
- 设备健康状态和在线状态。

驱动不应负责：

- UI 策略。
- 比赛业务状态机。
- 用户命令解释。
- 跨模块全局状态管理。
- 黑板字段的大规模编排。

### 2.5 Platform

`platform` 封装 RTOS/HAL 能力：线程、队列、时间、锁、CAN/UART/SPI HAL 适配、中断分发。

如果业务文件直接到处使用 `xTaskGetTickCount`、`pdMS_TO_TICKS`、`HAL_FDCAN_AddMessageToTxFifoQ`，说明 platform 边界还不够清楚。不是所有调用都必须立刻封装，但高层业务规则不应被平台 API 绑死。

### 2.6 Board

`board` 描述具体板级事实：引脚、外设实例、CAN 拓扑、电机 ID、串口分配、机械装配偏移。

当前项目的 `Config/Chassis/hw-config.h` 和 `Config/Gimbal/hw-config.h` 已经承担了一部分 board 角色，但还混有部分算法常量和业务假设。

### 2.7 Composition Root

`composition root` 是装配根，负责创建对象、绑定依赖、注入接口、启动线程。

当前项目通过全局 singleton 和 `forceInit` 静态对象让任务自动注册。这种方式短期方便，但长期会隐藏依赖关系和初始化顺序。更可维护的方式是让装配根显式说明：有哪些任务、启动顺序、依赖谁、是否启用。

### 2.8 Command / Event / Data / Snapshot

复杂项目不要把所有共享内容都叫“数据”。至少分四类：

- Command：持续有效的目标或模式，例如 `GimbalCmd`、`ChassisCmd`。
- Event：一次性边沿事件，例如 `FRIC_TOGGLE`、`SINGLE_FIRE`。
- Data：连续高速采样或反馈，例如 IMU、motor feedback、视觉帧流。
- Snapshot：给 UI、日志、调试器读取的只读状态快照。

当前 `Blackboard` 同时承载这四类内容，但没有在类型和命名上强制区分，所以后期容易出现“谁写、谁读、是否会丢事件、是否允许旧值”的理解成本。

## 3. 推荐依赖方向

通用依赖方向：

```text
app/composition
  -> services
  -> domain
  -> ports

services -> ports <- drivers -> platform -> board -> vendor_hal / rtos / third_party
```

允许的依赖规则：

- `domain` 不依赖硬件、不依赖 RTOS、不依赖 HAL。
- `services` 可以依赖 `domain` 和必要的 `ports`。
- `ports` 只定义能力接口和小型数据类型。
- `drivers` 实现具体设备能力，可依赖 `platform` 和 `board`。
- `platform` 封装 HAL/RTOS，不包含业务规则。
- `board` 只描述硬件事实和入口路由。
- `third_party` 保存外部库，不在其中直接写业务代码。

实践判断：

```text
如果 domain 需要 include FreeRTOS/HAL，说明它不是 domain。
如果 driver 需要知道比赛模式，说明业务泄漏到底层。
如果 service 直接操作 GPIO/DMA，说明硬件细节泄漏到上层。
```

## 4. 按变化原因组织代码

修改原因不同的代码不应强行放进同一文件。

常见变化原因：

- 换芯片或 CubeMX 配置：`board/`、`platform/stm32_hal/`。
- 换 RTOS：`platform/freertos/`、`platform/zephyr/`。
- 换 IMU、电机、遥控器型号：`drivers/`。
- 换遥控映射、裁判协议、视觉协议：`middleware/protocol/` 或 `domain/protocol/`。
- 换控制策略、状态机、PID 结构：`services/`、`domain/`、`algorithm/`。
- 换 UI 样式：`ui/`。
- 换调试方式：`diagnostics/`。

当前项目中比较明显的混合点：

- `System/DataHub/data-def.h` 同时包含命令、反馈、输出、遥测、板间通信 bitfield。
- `System/Service/motor-actuator.cpp` 同时做 CAN 驱动对象创建、电机反馈采集、黑板读写和下发控制。
- `System/Service/commander.cpp` 同时做遥控设备更新、Action 驱动、仲裁逻辑、业务命令填充、板间通信打包。
- `Application/movtion-ctrl-app.cpp` 同时包含底盘控制和云台控制两个编译分支。

这些文件不一定要马上拆，但需要在文档和命名上标清责任，否则长期维护会越来越困难。

## 5. 通用文件结构

推荐目标结构如下：

```text
project/
  app/
    main.cpp
    composition_root.cpp

  board/
    chassis/
      pinmap.hpp
      motor_topology.hpp
      irq_routes.cpp
    gimbal/
      pinmap.hpp
      motor_topology.hpp
      irq_routes.cpp

  platform/
    stm32_hal/
      can_hal_port.cpp
      uart_hal_port.cpp
      spi_hal_port.cpp
      gpio_interrupt.cpp
      timer_hal_port.cpp
    freertos/
      thread.cpp
      queue.cpp
      event_group.cpp
      time.cpp

  domain/
    motion/
    fire/
    input/
    power/
    referee/
    protocol/

  ports/
    motor_port.hpp
    imu_port.hpp
    remote_input_port.hpp
    serial_port.hpp
    can_bus_port.hpp
    clock_port.hpp

  drivers/
    motor/
    imu/
    remote/
    usb/
    supercap/

  services/
    commander/
    chassis_motion/
    gimbal_motion/
    fire_control/
    motor_io/
    imu_estimator/
    board_link/
    vision_link/
    referee_link/
    ui_presenter/

  middleware/
    protocol/
    filters/
    math/
    fsm/

  diagnostics/
    snapshots/
    metrics/
    debug_exports/

  generated/
  third_party/
  tests/
  docs/
```

这不是要求当前项目一次性迁移。它更像一个“目标坐标系”，用于判断新代码应该放在哪里，旧代码应该朝哪个方向拆。

## 6. STM32Cube HAL + FreeRTOS 落地规则

在 STM32Cube HAL + FreeRTOS 项目中，建议把 HAL 和 FreeRTOS 限制在以下区域：

- CubeMX 生成代码：`Infantry-Chassis/Core`、`Infantry-Gimbal/Core` 或未来 `generated/`。
- 板级事实：`board/` 或当前 `Config/*/hw-config.h`。
- 平台适配：`platform/stm32_hal`、`platform/freertos`。
- 具体驱动：`drivers/`。

业务层尽量避免直接使用：

- `HAL_*` 函数。
- `FDCAN_TxHeaderTypeDef` / `UART_HandleTypeDef`。
- `GPIO_TypeDef` / `GPIO_Pin`。
- FreeRTOS 原始同步对象。

确实需要使用时，先限制在少数边界文件中，不要扩散到每个 application/service。

CubeMX 生成区域只做最薄转发：

```text
HAL callback / USER CODE
  -> board/platform dispatcher
  -> driver ISR-safe API
  -> service task
```

## 7. Zephyr 落地说明

如果未来迁移 Zephyr，应把 Zephyr 看成操作系统和硬件抽象底座，不要并行重造一套大 HAL。

Zephyr 中的建议归属：

- Devicetree 描述硬件事实：总线、引脚、时钟、中断、设备实例。
- Kconfig 描述软件能力：模块启用、缓冲区大小、日志等级、可选特性。
- Zephyr driver 使用 C 接入 device model。
- 应用业务层可使用 C++，但不要让 Zephyr 头文件污染 domain。
- 输入设备优先接入 Zephyr input/GPIO/QDEC。
- CAN、UART、USB 优先通过 Zephyr 设备模型适配到 port。

这部分对当前项目不是立即任务。当前更重要的是先把业务、驱动、平台边界整理清楚。

## 8. 中断与回调归属

HAL callback 或 IRQ handler 不应承载业务逻辑。

推荐链路：

```text
HAL IRQ / callback
  -> board/platform 中断分发
  -> driver 的 ISR-safe notify/write
  -> FreeRTOS task / service 处理
```

示例：

```cpp
extern "C" void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    BoardInterruptRouter::on_exti(pin);
}
```

```cpp
void BoardInterruptRouter::on_exti(uint16_t pin)
{
    if (pin == kImuDataReadyPin) {
        ImuDriver::instance().on_data_ready_isr();
    }
}
```

```cpp
void ImuDriver::on_data_ready_isr()
{
    BaseType_t woke = pdFALSE;
    xTaskNotifyFromISR(task_handle_, 0, eNoAction, &woke);
    portYIELD_FROM_ISR(woke);
}
```

真正读取 IMU、解析协议、写黑板应放在线程上下文。

当前项目的 `System/Service/interrupt-callback.cpp` 已经集中路由了 callback，这是合理起点。但需要继续避免把业务分支写进这个文件，并明确每个回调最终归属哪个 driver/service。

## 9. 线程与主动对象

线程应按职责归属，而不是机械按外设归属。

判断规则：

- 线程处理业务状态机：service/application。
- 线程只维护设备收发：driver 或 IO service。
- 线程只封装 RTOS 能力：platform。

每个主动模块建议拥有：

- 明确输入：queue、notification、mailbox 或周期 tick。
- 明确输出：command、state、snapshot 或硬件动作。
- 明确状态机：状态枚举、入口动作、转移条件、退出动作。
- 明确错误策略：离线、超时、重试、降级、急停。
- 明确调试快照：结构化状态，而不是散落的全局变量。

当前 `StaticAppBase + PeriodicApp/NotifyApp/QueueApp` 是一个有价值的任务框架。问题在于任务创建依赖全局 singleton 隐式注册，依赖关系不透明。短期可以继续用，长期建议增加显式任务清单或装配根。

## 10. 全局数据流动

### 10.1 控制流

控制流表示命令、事件和状态切换。

```text
remote / vision / referee / host
  -> command/event
  -> service FSM
  -> output command
```

适合使用：

- FreeRTOS queue。
- event flags。
- command bus。
- active object mailbox。
- 少量 seqlock command slot，但必须说明事件是否可能丢失。

一次性事件不要只依赖“某个结构体字段变了”。边沿事件应有清晰语义：是否可丢、是否排队、是否只保留最后一次。

### 10.2 连续数据流

连续数据流表示高速、实时、采样类数据。

```text
sensor / bus / DMA
  -> ring or double buffer
  -> estimator / parser
  -> state snapshot
```

实时数据流中应避免：

- heap 分配。
- 文件 IO。
- 阻塞锁。
- 复杂日志。
- UI 调用。

### 10.3 状态流

状态流用于 UI、日志、上位机和调试。

```text
service internal state
  -> immutable snapshot
  -> ui / diagnostics / host query
```

外部模块不应为了显示或调试直接读取服务内部变量。每个关键服务应提供结构化 `snapshot()` 或写入专门的 diagnostic snapshot。

当前项目中大量 `g_xxx_debug` 和 Ozone 全局探针很实用，但需要统一归属到 diagnostics，否则调试变量会逐渐变成第二套隐式数据总线。

## 11. Blackboard 使用边界

`Blackboard + SeqVariable<T>` 是当前项目的数据交换核心，适合保留，但需要加边界。

建议规则：

- 按语义拆分字段：`Command`、`Event`、`Feedback`、`Output`、`Telemetry`、`Snapshot`。
- 每个字段文档化唯一写入者、允许读取者、更新频率和过期策略。
- 事件类字段不要混在状态结构里长期保存，除非明确“只保留最后一个事件”。
- 控制输出字段只允许控制服务写，执行服务读。
- 反馈字段只允许驱动/IO 服务写，控制服务读。
- UI 和调试只读 snapshot，不直接依赖控制服务内部变量。

建议把当前 `data-def.h` 拆成更小的文件：

```text
domain/input/remote_types.hpp
domain/control/chassis_command.hpp
domain/control/gimbal_command.hpp
domain/control/shoot_command.hpp
domain/state/motor_state.hpp
domain/state/imu_state.hpp
domain/output/actuator_output.hpp
domain/telemetry/chassis_telemetry.hpp
middleware/protocol/board_link_frame.hpp
middleware/protocol/vision_frame.hpp
middleware/protocol/referee_frame.hpp
```

拆分不改变运行逻辑，但能显著降低阅读成本。

## 12. 当前 Infantry 项目架构诊断

### 12.1 已经做得好的地方

- `ApplicationEntry()` 统一启动任务，启动链路清晰。
- `PeriodicApp`、`NotifyApp`、`QueueApp` 抽象了常见 FreeRTOS 调度模式。
- `Blackboard + SeqVariable` 避免了大量裸全局变量读写。
- 输入系统已经有 Control、Trigger、Action 的分层雏形。
- 射击和云台运动已经用 FSM 拆分状态文件，方向正确。
- `Algorithm` 目录中有一部分纯算法，适合未来单元测试。
- `Config/Chassis` 和 `Config/Gimbal` 已经意识到双目标差异。

### 12.2 主要维护性问题

#### 1. 命名层级和职责不一致

`Application`、`System/Service`、`Board-Support-Pack` 的边界不稳定。有些 Service 是业务服务，有些 Service 是设备 IO，有些 BSP 里也包含输入抽象。

建议逐步统一为：

- `services`：业务编排、状态机、任务。
- `drivers`：具体设备和通信设备。
- `platform`：HAL/RTOS 适配。
- `domain`：纯业务类型和算法。

#### 2. 双目标条件编译分散

`CHASSIS` / `GIMBAL` 分支散落在多个核心文件里，导致阅读一个模块时必须同时理解两个 MCU。

特别注意：多处出现 `#elifdef GIMBAL`，这不是标准 C/C++ 预处理语法。应改为：

```cpp
#elif defined(GIMBAL)
```

建议长期减少一个文件内的大块双目标分支。优先拆成：

```text
services/chassis_motion/
services/gimbal_motion/
board/chassis/
board/gimbal/
```

#### 3. Blackboard 语义过宽

当前黑板像“全局共享内存”。它能工作，但读者很难知道每个字段的唯一写入者、生命周期和事件语义。

建议给每个字段补充 owner 表，并把一次性事件从持续状态里拆出来。

#### 4. 静态 singleton 隐藏依赖和启动顺序

`[[maybe_unused]] static auto& forceInit = Xxx::instance();` 可以自动注册任务，但长期会造成：

- 初始化顺序依赖链接和静态对象构造。
- 难以看出哪个服务依赖哪个服务。
- 难以在测试中替换依赖。
- 难以按目标裁剪任务。

短期保留可以，但建议增加显式 `composition_root` 或 `application_registry`，集中列出任务、周期、优先级和依赖。

#### 5. 服务文件承担过多角色

典型例子：

- `motor-actuator.cpp` 同时创建 CAN driver、电机对象、读取反馈、写黑板、下发输出。
- `commander.cpp` 同时处理遥控设备、触发器、仲裁、业务命令和板间通信字段。
- `movtion-ctrl-app.cpp` 同时承载底盘运动和云台运动两个大逻辑。

建议优先拆“角色”，不一定马上移动目录：

- parser / mapper / arbiter / command writer。
- motor driver / motor feedback collector / actuator output sender。
- chassis motion controller / gimbal motion controller。

#### 6. 协议和业务类型混放

`data-def.h` 里既有业务命令，也有通信 bitfield union。协议帧结构应和业务命令模型分离。

建议：

```text
BoardLinkFrame <-> BoardLinkCodec <-> GimbalToChassisCommand
VisionRxFrame  <-> VisionCodec     <-> VisionCommand
```

这样协议字段变化不会污染业务控制代码。

#### 7. 平台 API 泄漏到业务层

`xTaskGetTickCount`、`pdMS_TO_TICKS`、HAL 类型和 HAL 发送函数在业务文件中出现较多。短期可以接受，但应避免继续扩散。

建议先从新代码开始使用 `ClockPort`、`CanBusPort`、`SerialPort` 或薄封装。

#### 8. 调试变量缺少统一出口

Ozone 探针有价值，但散落的 `g_xxx_debug` 会增加维护成本。建议用 `diagnostics/snapshots` 管理，每个服务提供一个稳定快照结构。

#### 9. 错误模型和健康状态不足

许多模块依赖 `online` 或简单 return，但缺少统一错误分类：超时、CRC 错误、协议长度错误、设备离线、总线错误、热量限制、功率限制、急停。

建议每个主动服务至少提供：

- health state。
- last error。
- last update tick。
- degraded mode。

#### 10. 测试接缝不足

当前最适合先测试的是纯逻辑：

- Trigger/Action。
- S 曲线速度规划。
- 热量控制。
- 功率限制。
- 弹速补偿。
- 协议 encode/decode。
- FSM 状态转移条件。

要做到这一点，需要把这些逻辑从 HAL/FreeRTOS 类型中隔离出来。

#### 11. 代码卫生问题会放大架构成本

建议尽快处理：

- `movtion` 拼写错误。
- `#elifdef` 非标准预处理语法。
- `System/crtp.h` 和 `tools/crtp.h` 重复。
- 注释编码和中英文术语不统一。
- magic number 缺少命名常量或单位说明。
- 电机 ID、数组 index、register id 的转换规则需要集中封装。

## 13. Infantry 建议目标映射

当前结构和目标结构可以这样对应：

```text
Solution/app-main.cpp
  -> app/main.cpp + app/composition_root.cpp

Solution/Application/fire-ctrl-app.*
  -> services/fire_control/ + domain/fire/

Solution/Application/movtion-ctrl-app.*
  -> services/chassis_motion/ + services/gimbal_motion/ + domain/motion/

Solution/System/Service/commander.*
  -> services/commander/ + domain/input/ + drivers/remote/

Solution/System/Service/motor-actuator.*
  -> services/motor_io/ + drivers/motor/ + ports/motor_port.hpp

Solution/System/Service/state-estimator.*
  -> services/imu_estimator/ + drivers/imu/bmi088/ + domain/imu/

Solution/System/Service/real-time-comm.*
  -> services/board_link/ + middleware/protocol/board_link/

Solution/System/Service/vision-comm.*
  -> services/vision_link/ + middleware/protocol/vision/

Solution/System/DataHub/data-def.h
  -> domain/* + middleware/protocol/* + diagnostics/snapshot types

Solution/Algorithm/*
  -> domain algorithms or middleware/math

Solution/Board-Support-Pack/*
  -> drivers/* or platform adapters

Solution/Config/*
  -> board/chassis, board/gimbal, config/algorithm
```

建议目标目录不是短期必须完成的改动。优先用它指导新代码和拆分方向。

## 14. 分阶段重构路线

### Phase 0：不改变架构，只降低风险

- 修正 `#elifdef` 为 `#elif defined(...)`。
- 统一文件编码为 UTF-8。
- 修正明显拼写：`movtion` -> `motion`，同时处理 include 和构建引用。
- 去掉重复 `crtp.h` 或明确唯一来源。
- 为每个 Blackboard 字段写 owner/update rate/staleness 注释。
- 给任务清单补充周期、优先级、栈大小、依赖服务。

### Phase 1：整理数据定义，不移动运行逻辑

- 拆分 `data-def.h`。
- 把协议 frame/union 从业务 command/state 中分离。
- 为 ShootEvent 这类一次性事件定义事件通道语义。
- 为关键服务增加 snapshot 结构。
- 把 magic number 提炼成带单位的常量。

### Phase 2：分离平台和协议边界

- 为板间通信、视觉通信、裁判系统建立 codec 层。
- 将 HAL send/receive 细节限制在 link service 或 platform adapter。
- 将时间获取封装为最小 `Clock` 或 `now_ms()` 边界。
- 将 Ozone debug 全局变量迁移到 diagnostics snapshot。

### Phase 3：拆分大服务

- `CommanderSrvc` 拆为 remote update、action mapping、control arbitration、command writer。
- `MotActSrvc` 拆为 motor driver init、feedback collector、actuator sender。
- `MovtionCtrlApp` 拆为 chassis motion 和 gimbal motion 两个独立模块。
- `FireCtrlApp` 保留 FSM 文件拆分，但将热量、弹速、trigger 位置计算进一步纯函数化。

### Phase 4：建立测试和仿真接缝

- 为算法和协议建立 host-side 单元测试。
- 为 FSM 状态转移建立无 HAL/FreeRTOS 的测试 harness。
- 为 remote input mapping 建立输入样例测试。
- 为 power/heat/fire safety 建立边界条件测试。

## 15. 接口设计规则

接口应服务于隔离变化，而不是形式上的面向对象。

建议先定义少量稳定能力接口：

```cpp
class RemoteInputPort {
public:
    virtual ~RemoteInputPort() = default;
    virtual RemoteSample sample() const = 0;
    virtual bool online() const = 0;
};

class MotorFeedbackPort {
public:
    virtual ~MotorFeedbackPort() = default;
    virtual MotorState read(MotorId id) const = 0;
};

class MotorCommandPort {
public:
    virtual ~MotorCommandPort() = default;
    virtual void send(MotorId id, MotorCommand command) = 0;
};
```

不要建立巨大的万能 `IDevice`：

```cpp
class IDevice {
public:
    virtual void init();
    virtual void open();
    virtual void close();
    virtual void read();
    virtual void write();
    virtual void reset();
};
```

这种接口语义太弱，无法指导正确调用顺序，也无法表达实时约束。

## 16. 资源管理

资源必须有明确所有权和生命周期。

资源类型包括：

- HAL handle、bus handle、Zephyr device。
- FreeRTOS task、queue、mutex、event group、timer。
- DMA buffer、ring buffer、block pool。
- 电机对象、传感器对象、协议 parser 状态。
- 校准参数、配置参数、运行状态。
- 中断订阅、回调注册。
- 调试计数器和 snapshot。

建议规则：

- 实时路径不做动态资源申请。
- C++ 对象构造函数不要做复杂硬件初始化，硬件初始化放 `init()`，并返回可诊断结果。
- HAL 静态 handle 通常由平台生命周期管理，C++ 只持有非拥有引用。
- 所有跨线程共享状态必须说明同步机制。
- placement new 和 reinterpret_cast 管理对象生命周期时必须集中封装，不要扩散到业务代码。
- 全局对象只用于不可变配置、平台句柄或装配根创建的服务引用。

## 17. 现代 C++ 使用准则

推荐：

- `enum class` 表达强类型状态和错误。
- `std::array` 表达固定容量。
- `std::span` 表达非拥有连续缓冲区。
- `std::optional` 表达可缺省值。
- `[[nodiscard]]` 标注必须处理的结果。
- 强类型单位，例如 `Radians`、`RadPerSec`、`Millisecond`、`Ampere`。
- 小型纯函数，用于算法、协议和状态转移测试。

谨慎或禁用：

- 异常。
- RTTI。
- 实时路径动态分配。
- 复杂模板元编程。
- 隐式全局单例。
- 构造函数中执行复杂硬件初始化。

命名建议：

- 类型：`PascalCase`。
- 函数和变量：项目统一即可，但不要新旧风格混用。
- 常量必须带单位或所在命名空间说明含义。
- C 接口和 HAL callback 遵循平台既有命名。
- 文件名避免拼写错误和历史包袱。

## 18. Rust 引入建议

Rust 不是当前项目的优先事项。若未来引入，适合放在纯逻辑或 host-side 工具中：

- 协议帧解析。
- 参数校验。
- CRC 和编码解码。
- 日志文件解析。
- 可 fuzz 的纯逻辑。

不建议初期用于：

- HAL callback。
- DMA ISR。
- FreeRTOS task glue。
- 需要频繁访问厂商 HAL 的驱动。

## 19. 模块设计检查清单

新增或重构模块前回答：

- 这个模块说的是硬件语言、能力语言还是业务语言？
- 它的唯一职责是什么？
- 它的上层调用者是谁？
- 它允许依赖哪些目录？
- 它拥有哪些资源？
- 它是否会被中断调用？
- 它是否会被多个线程调用？
- 它的输入是 command、event、data 还是 snapshot？
- 它的输出写到哪里？
- 谁是它写入数据的 owner？
- 它是否需要 mock 或替换实现？
- 它的错误是否可恢复？
- 它的状态如何被诊断工具观察？

如果这些问题答不清楚，先不要创建新目录或新抽象。

## 20. 总结规则

- 先按依赖方向分类，再按变化原因细分，最后才按设备或功能命名文件。
- 中断入口只做分发，ISR 只做通知或极短数据搬运，业务逻辑在线程上下文处理。
- 驱动负责硬件可靠工作，服务负责业务状态机和编排。
- 接口只用于隔离真实变化，不要求所有设备都有接口。
- 不做万能 `IDevice`，按能力定义小而稳定的 port。
- Blackboard 可以保留，但必须明确 command、event、feedback、output、telemetry、snapshot 的语义和 owner。
- 实时数据路径避免 heap、阻塞锁、文件 IO 和复杂日志。
- 调试观察走状态快照，不破坏封装。
- 平台代码适配 HAL/RTOS，业务代码尽量不直接依赖 HAL/RTOS。
- 架构演进应分阶段进行，先降低理解成本，再拆目录和抽象。
