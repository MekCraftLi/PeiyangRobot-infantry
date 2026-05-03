/**
 *******************************************************************************
 * @file    trigger-config.h
 * @brief   标准触发器配置 — 统一阈值与触发参数
 *
 * 所有按键/开关类控件的输出经 ControlSwitch 归一化到 [-1.0, 1.0]:
 *   3档开关: Index 0→-1.0, Index 1→0.0, Index 2→1.0
 *   2档开关: Index 0→-1.0, Index 1→1.0
 *   按钮:    释放→-1.0, 按下→1.0
 *
 * 因此统一阈值为 0.0, 可正确区分 "激活" (≥0.0) 与 "未激活" (<0.0).
 *******************************************************************************
 * @author  MekLi
 * @date    2026/5/3
 * @version 1.0
 *******************************************************************************
 */

#ifndef INFANTRY_TRIGGER_CONFIG_H
#define INFANTRY_TRIGGER_CONFIG_H

#include "TriggerImpl/trigger-decorator-cycle.h"
#include "TriggerImpl/trigger-decorator-toggle.h"
#include "TriggerImpl/trigger-impl-edge.h"
#include "TriggerImpl/trigger-impl-hold.h"
#include "TriggerImpl/trigger-impl-linear.h"

namespace TriggerCfg {

// ── 通用 ──────────────────────────────────────────────────────────────────────
constexpr float JOYSTICK_DEADZONE = 0.02f; // 摇杆死区 (归一化值)

// ── 标准阈值 (所有按键/开关统一) ─────────────────────────────────────────────
constexpr float BTN_THRESHOLD = 0.0f; // 按键激活阈值 (归一化后 ≥0.0 即激活)

// ── 模式开关 ──────────────────────────────────────────────────────────────────
// 3档开关归一化: -1.0 (上) / 0.0 (中) / 1.0 (下)
// 仲裁逻辑直接用阈值比较, 不经过 TriggerHold
constexpr float MODE_SW_STOP_THRESH  = -0.5f; // < -0.5 → SAFE_STOP (上)
constexpr float MODE_SW_VISION_THRESH = 0.5f;  // > 0.5  → VISION (下)

// ── 射击系统 ──────────────────────────────────────────────────────────────────
constexpr float BURST_HOLD_TIME    = 1.5f;  // 连发长按判定时间 (秒)
constexpr float INSTANT_HOLD_TIME  = 0.001f; // 即时触发时间 (≈0)

} // namespace TriggerCfg

#endif /*INFANTRY_TRIGGER_CONFIG_H*/
