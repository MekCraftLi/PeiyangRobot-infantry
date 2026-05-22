# PeiyangRobot-infantry 构建脚本
# 用法: .\build.ps1 [-Target CHASSIS|GIMBAL|BOTH] [-BuildType Debug|Release] [-Remote DR16|VideoLink|GamePad]

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("CHASSIS", "GIMBAL", "BOTH")]
    [string]$Target = "CHASSIS",
    
    [Parameter(Mandatory=$false)]
    [ValidateSet("Debug", "Release")]
    [string]$BuildType = "Debug",
    
    [Parameter(Mandatory=$false)]
    [ValidateSet("DR16", "VideoLink", "GamePad")]
    [string]$Remote = "DR16",
    
    [Parameter(Mandatory=$false)]
    [switch]$Clean = $false,
    
    [Parameter(Mandatory=$false)]
    [switch]$ConfigureOnly = $false
)

$BuildDir = Join-Path $PSScriptRoot "build"
$ProjectRoot = $PSScriptRoot

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "PeiyangRobot-infantry Build Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Target: $Target" -ForegroundColor Yellow
Write-Host "Build Type: $BuildType" -ForegroundColor Yellow
Write-Host "Remote Device: $Remote" -ForegroundColor Yellow
Write-Host "========================================" -ForegroundColor Cyan

# 清理构建目录
if ($Clean) {
    Write-Host "`nCleaning build directory..." -ForegroundColor Green
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir
        Write-Host "Build directory cleaned." -ForegroundColor Green
    }
}

# 检查子模块
Write-Host "`nChecking Git submodules..." -ForegroundColor Green
$SubmoduleStatus = git submodule status 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "Error: Not a git repository or git is not installed." -ForegroundColor Red
    exit 1
}

# 初始化子模块（如果需要）
if ($SubmoduleStatus -match "^-") {
    Write-Host "Initializing Git submodules..." -ForegroundColor Yellow
    git submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: Failed to initialize submodules." -ForegroundColor Red
        exit 1
    }
}

# 配置项目
Write-Host "`nConfiguring project..." -ForegroundColor Green
$ConfigureArgs = @(
    "-B", $BuildDir
    "-G", "Ninja"
    "-DCMAKE_BUILD_TYPE=$BuildType"
    "-DBUILD_TARGET=$Target"
    "-DREMOTE_DEVICE=$Remote"
    "-S", $ProjectRoot
)

& cmake @ConfigureArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "`nError: CMake configuration failed." -ForegroundColor Red
    exit 1
}

Write-Host "Configuration completed successfully." -ForegroundColor Green

# 如果只需要配置，则退出
if ($ConfigureOnly) {
    Write-Host "`nConfiguration only mode. Exiting." -ForegroundColor Yellow
    exit 0
}

# 构建项目
Write-Host "`nBuilding project..." -ForegroundColor Green
$BuildArgs = @(
    "--build", $BuildDir
    "--config", $BuildType
    "-j", "14"
)

& cmake @BuildArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "`nError: Build failed." -ForegroundColor Red
    exit 1
}

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "Build completed successfully!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan

# 显示生成的文件
if ($Target -eq "CHASSIS" -or $Target -eq "BOTH") {
    $ChassisElf = Join-Path $BuildDir "Infantry-Chassis/infantry_chassis.elf"
    if (Test-Path $ChassisElf) {
        Write-Host "`nChassis ELF: $ChassisElf" -ForegroundColor Cyan
    }
}

if ($Target -eq "GIMBAL" -or $Target -eq "BOTH") {
    $GimbalElf = Join-Path $BuildDir "Infantry-Gimbal/infantry_gimbal.elf"
    if (Test-Path $GimbalElf) {
        Write-Host "Gimbal ELF: $GimbalElf" -ForegroundColor Cyan
    }
}
