# 快速参考指南

## 🚀 快速开始

### 1. 首次设置
```powershell
# 初始化 Git 子模块
git submodule update --init --recursive

# 配置并构建底盘项目（Debug 模式）
.\build.ps1 -Target CHASSIS -BuildType Debug -Remote DR16
```

### 2. 常用命令

#### 使用构建脚本
```powershell
# 构建底盘
.\build.ps1 -Target CHASSIS

# 构建云台
.\build.ps1 -Target GIMBAL

# 清理并重新构建
.\build.ps1 -Target CHASSIS -Clean

# 仅配置不构建
.\build.ps1 -Target GIMBAL -ConfigureOnly
```

#### 使用 VSCode 任务
- `Ctrl+Shift+P` → "Tasks: Run Task"
- 选择相应的构建任务

#### 使用 CMake Tools
- `Ctrl+Shift+P` → "CMake: Configure"
- `Ctrl+Shift+P` → "CMake: Build"

## 📋 构建选项

| 参数 | 选项 | 说明 |
|------|------|------|
| `-Target` | CHASSIS, GIMBAL, BOTH | 构建目标 |
| `-BuildType` | Debug, Release | 构建类型 |
| `-Remote` | DR16, VideoLink, GamePad | 遥控设备类型 |
| `-Clean` | (开关) | 清理构建目录 |
| `-ConfigureOnly` | (开关) | 仅配置不构建 |

## 🔧 常见问题解决

### 问题 1: CMake 配置失败
```powershell
# 解决方案：清理并重新配置
.\build.ps1 -Target CHASSIS -Clean
```

### 问题 2: 子模块未初始化
```powershell
# 解决方案：初始化子模块
git submodule update --init --recursive
```

### 问题 3: 链接器错误
检查以下文件：
- `Infantry-Chassis/CMakeLists.txt`
- `Infantry-Gimbal/CMakeLists.txt`

确保链接脚本路径正确。

### 问题 4: 找不到头文件
1. 确保 CMake Tools 已正确配置
2. 重新加载 VSCode 窗口
3. 检查 `.vscode/settings.json`

## 📁 项目结构

```
PeiyangRobot-infantry/
├── Infantry-Chassis/          # 底盘控制
│   ├── CMakeLists.txt
│   ├── STM32H723XG_FLASH.ld   # 链接脚本
│   └── ...
├── Infantry-Gimbal/           # 云台控制
│   ├── CMakeLists.txt
│   ├── STM32H723XG_FLASH.ld   # 链接脚本
│   └── ...
├── Solution/                  # 应用层代码
├── build/                     # 构建输出目录
├── .vscode/                   # VSCode 配置
│   ├── settings.json
│   ├── tasks.json
│   ├── launch.json
│   └── extensions.json
├── build.ps1                  # 构建脚本
└── CMakeLists.txt             # 根 CMake 配置
```

## 🎯 调试

### J-Link 调试
1. 构建项目（生成 .elf 文件）
2. 按 `F5` 启动调试
3. 选择调试配置：
   - Debug Chassis (J-Link)
   - Debug Gimbal (J-Link)

### 调试快捷键
- `F5`: 启动调试
- `Shift+F5`: 停止调试
- `F9`: 切换断点
- `F10`: 单步执行
- `F11`: 步入函数

## 📝 配置文件说明

### .vscode/settings.json
- CMake 工具配置
- 默认构建目标
- 编译器设置

### .vscode/tasks.json
- 预定义的构建任务
- 配置和清理任务

### .vscode/launch.json
- J-Link 调试配置
- 断点和启动设置

### .vscode/extensions.json
- 推荐的 VSCode 扩展

## 🔗 有用链接

- [CMake 官方文档](https://cmake.org/documentation/)
- [VSCode CMake Tools](https://github.com/microsoft/vscode-cmake-tools)
- [Cortex-Debug](https://github.com/Marus/cortex-debug)
- [STM32CubeMX](https://www.st.com/en/development-tools/stm32cubemx.html)

## 💡 提示

1. **始终使用 PowerShell 反引号** (`) 作为续行符，而不是 CMD 的 `^`
2. **定期清理构建目录**以避免缓存问题
3. **保持子模块最新**: `git submodule update --recursive`
4. **使用 Ninja 构建系统**以获得更快的构建速度
5. **在 Debug 模式下开发**，Release 模式用于最终测试
