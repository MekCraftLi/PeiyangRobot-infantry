# ARCHITECTURE.md

## 目录映射

```
infantry/                          ← 仓库根目录
├── CMakeLists.txt                 ← 顶层 CMake：根据 BUILD_TARGET 调度子目录
├── .gitmodules                    ← 两个 submodule：PYRo-uCtrl-Unity, TinyUSB
│
├── Infantry-Chassis/              ← 【底盘 MCU 工程】STM32CubeMX 生成
│   ├── CMakeLists.txt             ← 定义 CHASSIS=1，链接 Solution/ 下业务源码
│   ├── CMakePresets.json          ← Ninja + gcc-arm-none-eabi, Debug/Release
│   ├── STM32H723XG_FLASH.ld      ← 链接脚本
│   ├── Core/                      ← CubeMX 生成的 HAL 初始化代码 (C 语言)
│   │   ├── Src/                   ← main.c → MX_FREERTOS_Init() → ApplicationEntry()
│   │   └── Inc/                   ← HAL 配置头文件 (FreeRTOSConfig.h 等)
│   ├── Drivers/                   ← STM32 HAL 库 + CMSIS (CubeMX 托管，勿手动改)
│   ├── Middlewares/               ← FreeRTOS Kernel (CubeMX 托管)
│   └── MDK-ARM/                   ← Keil 工程文件 (备选构建方式)
│
├── Infantry-Gimbal/               ← 【云台 MCU 工程】结构与 Chassis 对称
│   ├── CMakeLists.txt             ← 定义 GIMBAL=1，额外编译 FireCtrlApp / Vision 等
│   ├── Core/, Drivers/ ...        ← 同 Chassis，独立 CubeMX .ioc 配置
│
└── Solution/                      ← ★ 业务逻辑层 (两个 MCU 共享)
    ├── app-main.cpp               ← 入口: ApplicationEntry() → StaticAppBase::startApplications()
    │
    ├── Config/                    ← 编译时参数配置
    │   ├── config.h               ← 总分发: 根据 CHASSIS/GIMBAL 宏 include 对应子配置
    │   ├── Chassis/
    │   │   ├── hw-config.h        ← 底盘硬件映射 (CAN 拓扑, UART 外设, 机械参数)
    │   │   └── algo-config.h      ← 底盘算法参数 (PID, 速度限制, 死区)
    │   └── Gimbal/
    │       ├── hw-config.h        ← 云台硬件映射 (电机 ID, 摩擦轮参数)
    │       └── algo-config.h      ← 云台算法参数 (PID, 限幅)
    │
    ├── Application/               ← ★ 应用层：各 FreeRTOS 任务的具体业务
    │   ├── movtion-ctrl-app.h/cpp ← MotionCtrlApp: 底盘运动控制 / 云台姿态控制
    │   ├── fire-ctrl-app.h/cpp    ← FireCtrlApp [仅Gimbal]: 射击状态机 (FSM)
    │   └── ui-maker-app.h/cpp     ← UIMakerApp [仅Chassis]: 裁判系统 UI 绘制
    │
    ├── System/                    ← 框架层：调度基类、数据总线、服务
    │   ├── Thread/
    │   │   └── application-base.h ← ★ 核心基类: StaticAppBase → PeriodicApp / NotifyApp / QueueApp
    │   ├── DataHub/
    │   │   ├── blackboard.h       ← ★ 全局数据总线: Singleton<Blackboard> + SeqVariable<T>
    │   │   ├── data-def.h         ← Blackboard 中使用的 POD 数据结构定义
    │   │   ├── vision-protocol.h  ← 视觉通信协议结构体
    │   │   ├── referee-protocol.h ← 裁判系统协议结构体
    │   │   ├── ui-protocol.h      ← UI 绘制协议结构体
    │   │   └── super-cap-protocol.h← 超级电容协议结构体
    │   ├── Input/                  ← 输入抽象层: Action → Control (轴映射) / Trigger (事件)
    │   │   ├── action.h           ← 用户动作定义 (枚举)
    │   │   ├── control.h          ← 连续量映射 (摇杆轴 → 归一化值)
    │   │   └── triggers.h         ← 离散事件映射 (拨杆/按键 → 边沿/点击/长按)
    │   ├── Service/                ← 底层服务线程 (驱动 + 通信)
    │   │   ├── commander.h/cpp    ← 遥控器输入解析，写入 Blackboard
    │   │   ├── state-estimator.h  ← IMU / 底盘姿态融合
    │   │   ├── motor-actuator.h   ← CAN 电机驱动发送
    │   │   ├── real-time-comm.h   ← 板间通信 (Chassis ↔ Gimbal CAN)
    │   │   ├── vision-comm.h      ← 视觉上位机串口通信
    │   │   ├── referee.h          ← 裁判系统数据解析
    │   │   ├── super-cap-comm.h   ← 超级电容通信
    │   │   ├── heart-beat.h       ← 看门狗 / 心跳
    │   │   ├── usb-device.h       ← USB CDC 设备
    │   │   ├── ui-renderer-srvc.h ← UI 渲染发送
    │   │   └── interrupt-callback.cpp ← HAL 中断回调路由
    │   ├── crtp.h                 ← (旧位置, 与 tools/crtp.h 重复)
    │   └── defs.h                 ← 全局类型/宏定义
    │
    ├── Algorithm/                  ← 纯算法组件 (无 RTOS 依赖)
    │   ├── Motion/                 ← S-Curve 速度规划器 (Jerk 限制)
    │   ├── Power/                  ← 功率限制器 (裁判系统功率上限)
    │   └── Shoot/                  ← 弹速补偿器 + 热量控制器
    │
    ├── Board-Support-Pack/         ← 板级驱动封装
    │   ├── BMI088/                 ← IMU (SPI) 驱动 + IIO 适配器
    │   ├── DR16/                   ← 大疆 DR16 遥控器 (UART) 驱动
    │   ├── VideoLink/              ← 图传链路遥控器驱动
    │   ├── GamePad/                ← 蓝牙手柄驱动
    │   └── USB/                    ← TinyUSB 移植: tusb_config.h + CDC 描述符
    │
    ├── Adapter/                    ← 输入适配器 (将不同遥控器映射到统一 Input)
    │   └── adapter-remote.h
    │
    ├── tools/                      ← 工具组件
    │   ├── seq-variable.h         ← ★ SeqVariable<T>: Seqlock 无锁读写 (ISR 安全)
    │   ├── crtp.h                 ← Singleton<T>: CRTP 单例模板
    │   ├── perf-monitor.h         ← PerfCounter: 周期级性能统计
    │   └── crc.h/cpp              ← CRC 校验
    │
    └── ThirdParty/                 ← Git Submodule (勿手动修改)
        ├── PYRo-uCtrl-Unity/       ← 自研中间件: PID, FSM, CAN, 电机, INS, EKF
        └── TinyUSB/                ← USB 设备栈
```

## 核心入口表

### 启动链路

```
startup_stm32h723xx.s (Reset_Handler)
  → main.c::main()
    → HAL 初始化, FreeRTOS 初始化
    → MX_FREERTOS_Init()              ← 定义在 Core/Src/freertos.c
      → ApplicationEntry()            ← 定义在 Solution/app-main.cpp
        → StaticAppBase::startApplications()  ← 遍历静态注册表，创建所有 FreeRTOS 任务
```

### FreeRTOS 任务分配

所有任务均通过全局静态构造自动注册到 `StaticAppBase::_taskInfoRegistry`，由 `startApplications()` 统一创建。各任务实现 `init()` + `run()` 方法。

| 任务类 | 头文件 | 调度策略 | 作用 |
|--------|--------|----------|------|
| `MovtionCtrlApp` | `Application/movtion-ctrl-app.h` | PeriodicApp (固定周期) | 底盘全向运动 / 云台姿态 PID 控制 |
| `FireCtrlApp` | `Application/fire-ctrl-app.h` | PeriodicApp [仅Gimbal] | 射击 FSM (摩擦轮 + 拨弹) |
| `UIMakerApp` | `Application/ui-maker-app.h` | PeriodicApp [仅Chassis] | 裁判系统 UI 绘制 |
| `Commander` | `System/Service/commander.h` | QueueApp / NotifyApp | 遥控器输入解析 → Blackboard |
| `StateEstimator` | `System/Service/state-estimator.h` | PeriodicApp | IMU 姿态融合 |
| `MotorActuator` | `System/Service/motor-actuator.h` | NotifyApp | CAN 电机电流下发 |
| `RealTimeComm` | `System/Service/real-time-comm.h` | PeriodicApp | 板间 CAN 通信 |
| `VisionComm` | `System/Service/vision-comm.h` | QueueApp [仅Gimbal] | 视觉串口通信 |
| `Referee` | `System/Service/referee.h` | QueueApp [仅Chassis] | 裁判系统解析 |
| `SuperCapComm` | `System/Service/super-cap-comm.h` | PeriodicApp [仅Chassis] | 超级电容串口通信 |
| `HeartBeat` | `System/Service/heart-beat.h` | PeriodicApp | 看门狗喂狗 |
| `USBDevice` | `System/Service/usb-device.h` | ContinuousApp | USB CDC 任务 |
| `UIRendererSrvc` | `System/Service/ui-renderer-srvc.h` | PeriodicApp [仅Chassis] | UI 渲染发送 |

### 数据流

```
遥控器/视觉 ──UART──→ Commander ──write──→ Blackboard (SeqVariable) ──read──→ App (控制算法)
                                ↑                                          │
                            StateEstimator                              MotorActuator
                           (IMU/Sensor)                               ──CAN──→ 电机
```

`Blackboard` 是全局唯一的数据交换中心，所有模块通过 `SeqVariable<T>` (seqlock) 无锁读写，ISR 安全。

### 条件编译边界

`#ifdef CHASSIS` / `#elifdef GIMBAL` 主要出现在以下文件中：
- `Config/config.h` — 分发 hw-config / algo-config
- `DataHub/blackboard.h` — 两套不同的 Blackboard 字段
- `Application/movtion-ctrl-app.h` — 底盘电机组 vs 云台 PID
- 各 Service 的 `.cpp` 中个别条件逻辑
