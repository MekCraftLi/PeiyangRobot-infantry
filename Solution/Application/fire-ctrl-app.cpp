/**
 *******************************************************************************
 * @file    fire-ctrl-app.cpp
 * @brief   简要描述
 *******************************************************************************
 * @attention
 *
 * none
 *
 *******************************************************************************
 * @note
 *
 * none
 *
 *******************************************************************************
 * @author  MekLi
 * @date    2026/3/3
 * @version 1.0
 *******************************************************************************
 */




/* ------- define ----------------------------------------------------------------------------------------------------*/





/* ------- include ---------------------------------------------------------------------------------------------------*/



/* I. header */

#include "fire-ctrl-app.h"

#include "Config/Gimbal/hw-config.h"
#include "System/DataHub/blackboard.h"
#include "pyro_dwt_drv.h"

/* II. other application */


/* III. standard lib */




/* ------- class prototypes-----------------------------------------------------------------------------------------*/




/* ------- macro -----------------------------------------------------------------------------------------------------*/





/* ------- variables -------------------------------------------------------------------------------------------------*/

[[maybe_unused]] static auto& forceInit = FireCtrlApp::instance();

// 专供 Ozone 示波器实时采样的热量观测探针
struct HeatDebugOzone {
    float local_heat;       // 算法预估的本地实时热量
    float referee_heat;     // 裁判系统真实下发的热量 (带延迟)
    float heat_limit;       // 热量上限 (通常为红线)
    float safe_margin_line; // 安全警戒线 (limit - safe_margin)
    float target_rpm;       // 拨弹电机目标转速
    uint8_t physical_shot;  // 物理发弹脉冲 (每次打出一发跳变一次，用于观测时序)
} g_heat_debug;

// 专供 Ozone 示波器实时采样的弹速观测探针
volatile struct SpeedDebugOzone {
    float ref_bullet_speed;     // 裁判系统回传的真实弹速 (m/s)
    float target_bullet_speed;  // 期望压制的安全弹速 (m/s)，固定值参考
    float base_fric_target;     // 基础摩擦轮设定转速 (开环值)
    float final_fric_target;    // 经过闭环补偿后的最终目标转速
    float fric_left_real;       // 左摩擦轮实际反馈转速
    float comp_integration;     // 补偿器内部积攒的补偿量 (rad/s 或 rpm)
    uint8_t physical_shot;      // 物理发弹脉冲 (每次打出一发跳变一次，用于时间轴对齐)
} g_speed_debug;
/* ------- application attribute -------------------------------------------------------------------------------------*/

#define APPLICATION_ENABLE     true

#define APPLICATION_NAME       "FireCtrl"

#define APPLICATION_STACK_SIZE 512

#define APPLICATION_PRIORITY   4

static StackType_t appStack[APPLICATION_STACK_SIZE];



/* ------- message interface attribute -------------------------------------------------------------------------------*/





/* ------- function prototypes ---------------------------------------------------------------------------------------*/





/* ------- function implement ----------------------------------------------------------------------------------------*/


FireCtrlApp::FireCtrlApp()
    : PeriodicApp(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE, appStack, APPLICATION_PRIORITY, 1),
      _ctx() {}


void FireCtrlApp::init() {
    /* driver object initialize */

    _fsm.change_state(&_statePassive);
    _fsm.enter(_ctx);
}

void FireCtrlApp::run() {
    // 【修复 3】：必须加 static，否则 dt 是野值！
    static uint32_t dwtCnt = 0;
    float dt               = pyro::dwt_drv_t::get_delta_t(&dwtCnt);

    ChassisToGimbalComm c2gData{};
    Blackboard::instance().shootCmd.read(_ctx.cmd);
    Blackboard::instance().boosterState.read(_ctx.fdb);
    Blackboard::instance().c2gComm.read(c2gData);

    uint32_t current_time_ms = xTaskGetTickCount();

    // 1.1 同步裁判系统真实数据
    _ctx.heatController.syncWithReferee(c2gData.msg.shooter17mmBarrelHeat, c2gData.msg.heatLimit,
                                        c2gData.msg.coolingRate, current_time_ms);

    // 【新增接入】：将裁判系统下发的最新弹速喂给补偿器
    // 假设 c2gData 中包含 bulletSpeed 字段 (如 c2gData.msg.bulletSpeed)
    // 补偿器内部会自动过滤重复数据和异常值
    _ctx.speedCompensator.update((float)c2gData.msg.initialSpeedX100 / 100.0f);



    // 1.2 高频本地冷却推演
    _ctx.heatController.tickCooling(dt);

    // =========================================================
    // 【修复 1 & 2】：严格的物理发弹检测与热量记录
    // =========================================================
    const int32_t ECD_PER_BULLET         = 8192 * 36 / 8; // M2006拨弹盘单发编码器跨度 (36864)

    // 获取电机当前绝对连续编码器值
    int32_t currentContinuousEcd         = _ctx.fdb.triggerEcd + _ctx.fdb.triggerRound * 8192;
    // 使用静态变量保存上一次发弹的物理位置底数
    static int32_t lastShotContinuousEcd = currentContinuousEcd;

    // 防抖对齐：如果刚开机或者刚刚进行过强行校准，导致差距过大，直接对齐
    if (std::abs(currentContinuousEcd - lastShotContinuousEcd) > ECD_PER_BULLET * 10) {
        lastShotContinuousEcd = currentContinuousEcd;
    }

    // 核心判定：电机是否向前转过了完整的一发子弹跨度
    if (currentContinuousEcd - lastShotContinuousEcd >= ECD_PER_BULLET) {

        // 【核心补全】：立刻向热量控制器注册开火，增加 10 点热量！
        _ctx.heatController.recordBulletShot(current_time_ms);

        // 【核心补全】：累加比较底数，准备迎接下一发！
        lastShotContinuousEcd += ECD_PER_BULLET;

        // Ozone 探针：产生一个瞬间脉冲
        g_heat_debug.physical_shot = 50;
    } else {
        // Ozone 探针：平时保持为 0，这样你才能在波形图上看到一根一根的“针”
        g_heat_debug.physical_shot = 0;
    }
    // =========================================================

    // 2. 提取瞬态边沿事件
    updateTransientEvent();

    // 3. 驱动状态机流转 (仅计算目标角度/速度，不涉及 PID)
    _fsm.execute(_ctx);

    // 4. 核心算法：统一进行 PID 计算
    BoosterOutput finalOut{};
    calculateCurrents(finalOut);

    // 给 Ozone 探针赋值
    g_heat_debug.local_heat       = _ctx.heatController.getLocalHeat();
    g_heat_debug.referee_heat     = c2gData.msg.shooter17mmBarrelHeat;
    g_heat_debug.heat_limit       = c2gData.msg.heatLimit;
    g_heat_debug.safe_margin_line = c2gData.msg.heatLimit - HeatController::SAFE_MARGIN;
    g_heat_debug.target_rpm       = _ctx.targetTriggerSpeed;


    g_speed_debug.ref_bullet_speed = (float)c2gData.msg.initialSpeedX100 / 100.0f;
    g_speed_debug.target_bullet_speed = 23.5f; // 与补偿器内部设置的目标值保持一致

    if (currentContinuousEcd - lastShotContinuousEcd >= ECD_PER_BULLET) {
        _ctx.heatController.recordBulletShot(current_time_ms);
        lastShotContinuousEcd += ECD_PER_BULLET;

        g_heat_debug.physical_shot = 50;
        g_speed_debug.physical_shot = 50; // 同步发弹脉冲
    } else {
        g_heat_debug.physical_shot = 0;
        g_speed_debug.physical_shot = 0;
    }
    // 5. 将计算好的电流写入黑板
    Blackboard::instance().boosterOut.write(finalOut);
}

constexpr float ANGLE_PER_BULLET = 45.0f; // 拨弹盘单发角度 (8发/圈)

/* ========================================================
 * 1. 禁用发射机构
 * ======================================================== */

void FireCtrlApp::StatePassive::enter(FireCtrlCtx& ctx) {
    ctx.isCalibrated            = false;
    ctx.targetFricSpeed         = 0.0f;
    ctx.useTriggerSpeedLoopOnly = false;
    ctx.state                   = FireState::Passive;

    ctx.speedCompensator.reset();
}

void FireCtrlApp::StatePassive::execute(FireCtrlCtx& ctx) {
    // 锁死当前位置，防止掉电滑转
    ctx.targetTriggerEcd = ctx.fdb.triggerEcd + ctx.fdb.triggerRound * 8192 - ctx.triggerOffset; // 锁死拨弹盘

    if (ctx.transientEvent == ShootEvent::FRIC_TOGGLE) {
        request_switch(&instance()._stateSpinUp);
    }
}



/* ========================================================
 * 2. 摩擦轮加速
 * ======================================================== */


void FireCtrlApp::StateSpinUp::enter(FireCtrlCtx& ctx) {
    ctx.targetFricSpeed         = Config::Hardware::MotorTopo::FRIC_TARGET_SPEED;
    ctx.useTriggerSpeedLoopOnly = false;
    ctx.state                   = FireState::SpinUp;
}

void FireCtrlApp::StateSpinUp::execute(FireCtrlCtx& ctx) {
    if (ctx.transientEvent == ShootEvent::FRIC_TOGGLE || ctx.transientEvent == ShootEvent::EMERGENCY_STOP) {
        request_switch(&instance()._statePassive);
        return;
    }

    if (ctx.transientEvent == ShootEvent::SINGLE_FIRE) {
        // 【新增安全拦截】：只有热量安全，才允许切入发弹状态

        // if (!ctx.isCalibrated)
        //     request_switch(&instance()._stateSingleFire);
        // else
        //     request_switch(&instance()._stateSingleFire);

    } else if (ctx.transientEvent == ShootEvent::BURST_START || ctx.cmd.state.burstShot == 1) {
        ctx.isCalibrated = false;
        request_switch(&instance()._stateBurstFire);
    }
}



/* ========================================================
 * 3. 摩擦轮准备完成
 * ======================================================== */


void FireCtrlApp::StateReady::enter(FireCtrlCtx& ctx) {
    // 1. 强制切回位置环控制，提供电机的物理刚性，防止溜弹
    ctx.useTriggerSpeedLoopOnly = false;

    // 2. 获取当前的绝对连续编码器位置
    int32_t currentEcd          = ctx.fdb.triggerEcd + ctx.fdb.triggerRound * 8192 - ctx.triggerOffset;
    int32_t ecdPerBullet        = 8192 * 36 / 8; // M2006 单发跨度*/

    // 3. 核心计算：四舍五入对齐到最近的物理槽位
    // 利用 (x + step/2) / step * step 实现整数四舍五入
    ctx.targetTriggerEcd        = ((currentEcd + ecdPerBullet / 2) / ecdPerBullet) * ecdPerBullet;
}

void FireCtrlApp::StateReady::execute(FireCtrlCtx& ctx) {
    if (ctx.transientEvent == ShootEvent::FRIC_TOGGLE || ctx.transientEvent == ShootEvent::EMERGENCY_STOP) {
        request_switch(&instance()._statePassive);
        return;
    }
    //
    // if (ctx.transientEvent == ShootEvent::SINGLE_FIRE) {
    //     if (!ctx.isCalibrated)
    //         request_switch(&instance()._stateCaliReverse);
    //     else
    //         request_switch(&instance()._stateSingleFire);
    // } else if (ctx.transientEvent == ShootEvent::BURST_START || ctx.cmd.state.burstShot == 1) {
    //
    //     if (ctx.heatController.isApproachingHeatLimit()) {
    //         if (ctx.heatController.canShootSingle()) {
    //             // 【修改】：热量警戒，进入角度环连发！
    //             request_switch(&instance()._stateSafeBurst);
    //         }
    //     } else {
    //         // 热量健康，进入传统速度环连发
    //         ctx.isCalibrated = false;
    //         request_switch(&instance()._stateBurstFire);
    //     }
    // }

    if (ctx.transientEvent == ShootEvent::BURST_START || ctx.cmd.state.burstShot == 1) {
        request_switch(&instance()._stateBurstFire);
    }

}




/* ========================================================
 * 4. 校准回复
 * ======================================================== */



void FireCtrlApp::StateCaliReverse::enter(FireCtrlCtx& ctx) {
    // 剩余热量不足，不会进入校准的反转状态
    if (!ctx.heatController.canShootSingle()) {
        return;
    }

    ctx.blockTimer              = 0;
    ctx.useTriggerSpeedLoopOnly = true; // 开启纯速度环反转
    ctx.state                   = FireState::CaliReverse;
}

void FireCtrlApp::StateCaliReverse::execute(FireCtrlCtx& ctx) {
    ctx.targetTriggerSpeed = -Config::Hardware::MotorTopo::TRIGGER_SPEED; // 高速反转寻找机械死区

    if (ctx.transientEvent == ShootEvent::FRIC_TOGGLE || ctx.transientEvent == ShootEvent::EMERGENCY_STOP) {
        request_switch(&instance()._statePassive);
        return;
    }


    // 堵转检测 (实际速度远小于目标速度)
    if (std::abs(ctx.fdb.trigger.vel - ctx.targetTriggerSpeed) > 150.0f) {
        ctx.blockTimer++;
        if (ctx.blockTimer > 30) {
            request_switch(&instance()._stateCaliForward);
        }
    } else {
        ctx.blockTimer = 0;
    }
}

void FireCtrlApp::StateCaliReverse::exit(FireCtrlCtx& ctx) {
    // 退出速度控制的反转模式的时候清空速度环的积分项
    instance()._triggerSpdPid.clear();
    // 以当前的编码器值为新的起始点
    ctx.triggerOffset = (ctx.fdb.triggerEcd + ctx.fdb.triggerRound * 8192 + 8192 * 2) % (8192 * 36);
}




/* ========================================================
 * 5. 校准前移
 * ======================================================== */


void FireCtrlApp::StateCaliForward::enter(FireCtrlCtx& ctx) {
    ctx.useTriggerSpeedLoopOnly = false; // 恢复位置环
    ctx.targetTriggerEcd        = 0;
    ctx.state                   = FireState::CaliForward;
}

void FireCtrlApp::StateCaliForward::execute(FireCtrlCtx& ctx) {
    if (ctx.transientEvent == ShootEvent::FRIC_TOGGLE || ctx.transientEvent == ShootEvent::EMERGENCY_STOP) {
        request_switch(&instance()._statePassive);
        return;
    }

    if (std::abs((int32_t)(ctx.fdb.triggerEcd + ctx.fdb.triggerRound * 8192 - ctx.triggerOffset)) < 1000) {
        ctx.isCalibrated = true;
        request_switch(&instance()._stateSingleFire); // 校准完立刻打一发
    }
}



/* ========================================================
 * 6. 单发
 * ======================================================== */


void FireCtrlApp::StateSingleFire::enter(FireCtrlCtx& ctx) {

    // 剩余热量不足，不进入单发模式
    if (!ctx.heatController.canShootSingle()) {
        return;
    }

    ctx.blockTimer = 0;
    ctx.targetTriggerEcd += 8192 * 36 / 8;
    ctx.useTriggerSpeedLoopOnly = false;
    ctx.state                   = FireState::SingleFire;
}

void FireCtrlApp::StateSingleFire::execute(FireCtrlCtx& ctx) {
    if (ctx.transientEvent == ShootEvent::FRIC_TOGGLE || ctx.transientEvent == ShootEvent::EMERGENCY_STOP) {
        request_switch(&instance()._statePassive);
        return;
    }



    // 将目标ECD换算为弧度制
    float targetTriggerAngle = (float)(ctx.targetTriggerEcd) / (float)(8192 * 36) * 2 * M_PI;

    // 计算当前的ECD值
    int32_t ecd              = ctx.fdb.triggerEcd + ctx.fdb.triggerRound * 8192 - ctx.triggerOffset;
    // 进行循环限幅
    while (ecd < 0) {
        ecd += 8192 * 36;
    }
    // 将当前ECD换算为真实角度
    float realTriggerAngle = (float)(ecd) / (float)(8192 * 36) * 2 * M_PI;

    // 计算角度误差
    float err              = targetTriggerAngle - realTriggerAngle;

    while (err > M_PI) {
        err -= 2.0 * M_PI;
    }
    while (err < -M_PI) {
        err += 2.0 * M_PI;
    }

    // 卡弹检测：角度误差大且速度极低
    if (err > 15.0f && std::abs(ctx.fdb.trigger.vel) < 10.0f) {
        ctx.blockTimer++;
        if (ctx.blockTimer > 50) {
        }
        // request_switch(&instance()._stateJamClear);
    } else {
        ctx.blockTimer = 0;
        if (err < 0.01f)
            request_switch(&instance()._stateReady);
    }
}



/* ========================================================
 * 7. 连发
 * ======================================================== */


void FireCtrlApp::StateBurstFire::enter(FireCtrlCtx& ctx) {
    ctx.blockTimer              = 0;
    ctx.useTriggerSpeedLoopOnly = true; // 连发使用纯速度环
    ctx.state                   = FireState::BurstFire;
}

void FireCtrlApp::StateBurstFire::execute(FireCtrlCtx& ctx) {
    if (ctx.transientEvent == ShootEvent::BURST_STOP || ctx.transientEvent == ShootEvent::SINGLE_FIRE) {
        request_switch(&instance()._stateReady);
        return;
    }
    if (ctx.transientEvent == ShootEvent::FRIC_TOGGLE || ctx.transientEvent == ShootEvent::EMERGENCY_STOP) {
        request_switch(&instance()._statePassive);
        return;
    }

    // 【新增逻辑】：热量逼近警戒线时，强制退出纯速度环的连发模式
    //
    // if (ctx.heatController.isApproachingHeatLimit()) {
    //     request_switch(&instance()._stateSafeBurst);
    //     return;
    // }


    // 【核心修改】：通过热控器获取当前允许的最大安全射频
    // 假设 Config::Hardware::MotorTopo::TRIGGER_SPEED 是你的极致爆射转速（如 8000.0f）
    // 第二个参数 36.0f 是你的拨弹电机减速比
    // ctx.targetTriggerSpeed = ctx.heatController.getSafeBurstRpm(Config::Hardware::MotorTopo::TRIGGER_SPEED, 36.0f);

    ctx.targetTriggerSpeed = Config::Hardware::MotorTopo::TRIGGER_SPEED;
    if (std::abs(ctx.fdb.trigger.vel) < 10.0f && ctx.targetTriggerSpeed > 100.0f) {
        ctx.blockTimer++;
        if (ctx.blockTimer > 50) {
            // request_switch(&instance()._stateJamClear);
        }
    } else {
        ctx.blockTimer = 0;
    }
}
void FireCtrlApp::StateBurstFire::exit(FireCtrlCtx& ctx) {
    instance()._triggerSpdPid.clear();

    // 【修复原代码 BUG】：正确计算连续的编码器目标位置
    int32_t currentEcd          = ctx.fdb.triggerEcd + ctx.fdb.triggerRound * 8192 - ctx.triggerOffset;
    int32_t ecdPerBullet        = 8192 * 36 / 8; // M2006 拨弹盘单发编码器跨度 (36864)

    // 向上取整找到正前方的最近槽位，确保当前正在出膛的半颗子弹能完整打出
    ctx.targetTriggerEcd        = ((currentEcd + ecdPerBullet - 1) / ecdPerBullet) * ecdPerBullet;

    ctx.useTriggerSpeedLoopOnly = false; // 切回位置环进行急刹车
}


/* ========================================================
 * 8. 角度环连发
 * ======================================================== */


void FireCtrlApp::StateSafeBurst::enter(FireCtrlCtx& ctx) {
    ctx.useTriggerSpeedLoopOnly = false; // 严格使用位置环

    int32_t currentEcd          = ctx.fdb.triggerEcd + ctx.fdb.triggerRound * 8192 - ctx.triggerOffset;
    int32_t ecdPerBullet        = 8192 * 36 / 8;

    // 进入时，先锁定前方最近的那个槽位作为首发目标
    ctx.targetTriggerEcd        = ((currentEcd + ecdPerBullet - 1) / ecdPerBullet) * ecdPerBullet;

    ctx.state                   = FireState::SafeBurst;
}

void FireCtrlApp::StateSafeBurst::execute(FireCtrlCtx& ctx) {
    // 1. 如果松开扳机，或者发生状态切换，立刻回 Ready (Ready会自动锁死当前位置)
    if (ctx.transientEvent == ShootEvent::BURST_STOP || !ctx.cmd.state.burstShot) {
        request_switch(&instance()._stateReady);
        return;
    }

    int32_t currentEcd   = ctx.fdb.triggerEcd + ctx.fdb.triggerRound * 8192 - ctx.triggerOffset;
    int32_t ecdPerBullet = 8192 * 36 / 8;

    // 2. 核心逻辑：当前位置距离目标位置不到 半发 子弹时，判断是否续杯
    if (ctx.targetTriggerEcd - currentEcd < ecdPerBullet / 2) {
        if (ctx.heatController.canShootSingle()) {
            // 热量足够：续杯！目标位置往前推一发，位置环会驱动电机继续全速运转
            ctx.targetTriggerEcd += ecdPerBullet;
        } else {
            // 热量见底：不续杯了。直接切回 Ready。
            // 此时 targetTriggerEcd 停留在最后一发的位置，Ready 状态接管后会精准刹车在槽位上。
            request_switch(&instance()._stateReady);
        }
    }
}

/* ========================================================
 * 8. 堵弹
 * ======================================================== */

void FireCtrlApp::StateJamClear::enter(FireCtrlCtx& ctx) {
    ctx.stateTimer              = 0;
    ctx.useTriggerSpeedLoopOnly = true;
    ctx.state                   = FireState::JamClear;
}

void FireCtrlApp::StateJamClear::execute(FireCtrlCtx& ctx) {
    ctx.targetTriggerSpeed = -300.0f; // 强行反转退弹
    ctx.stateTimer++;
    if (ctx.stateTimer > 150) { // 反转 150ms
        request_switch(&instance()._stateReady);
    }
}

void FireCtrlApp::StateJamClear::exit(FireCtrlCtx& ctx) {
    ctx.targetTriggerEcd        = std::round(ctx.fdb.trigger.pos / ANGLE_PER_BULLET) * ANGLE_PER_BULLET;
    ctx.isCalibrated            = false;
    ctx.useTriggerSpeedLoopOnly = false;
}

void FireCtrlApp::updateTransientEvent() {
    _ctx.transientEvent = ShootEvent::NONE;
    if (_ctx.cmd.event != _lastEvent) {
        _ctx.transientEvent = _ctx.cmd.event;
        _lastEvent          = _ctx.cmd.event;
    }
}

void FireCtrlApp::calculateCurrents(BoosterOutput& out) {
    // 【新增接入】：获取经过弹速闭环补偿后的最终目标转速
    // 如果 compensator 没有误差，它将原样返回 _ctx.targetFricSpeed
    float finalFricTargetSpeed = _ctx.speedCompensator.getCompensatedRadPerSec(_ctx.targetFricSpeed);

    // 记录控制环关键数据到探针
    g_speed_debug.base_fric_target = _ctx.targetFricSpeed;
    g_speed_debug.final_fric_target = finalFricTargetSpeed;
    g_speed_debug.comp_integration = finalFricTargetSpeed - _ctx.targetFricSpeed; // 实际补偿增量
    g_speed_debug.fric_left_real = _ctx.fdb.fric[(uint8_t)Config::Hardware::MotorTopo::FRIC_LEFT_ID].vel;

    // 【修改】：使用补偿后的 finalFricTargetSpeed 进行 PID 计算
    out.fricLeftCurrent = _fricLeftSpdPid.calculate(
        finalFricTargetSpeed, _ctx.fdb.fric[(uint8_t)Config::Hardware::MotorTopo::FRIC_LEFT_ID].vel);

    // 右摩擦轮通常反向安装，因此目标速度取反
    out.fricRightCurrent = _fricRightSpdPid.calculate(
        -finalFricTargetSpeed, _ctx.fdb.fric[(uint8_t)Config::Hardware::MotorTopo::FRIC_RIGHT_ID].vel);
    // 【拨弹盘】
    if (_ctx.targetFricSpeed < 10.0f) {
        // 安全模式下，摩擦轮关闭时，直接彻底断开拨弹盘动力
        out.triggerCurrent = 0.0f;
        _triggerPosPid.clear();
        _triggerSpdPid.clear();
    } else {
        float spdTarget = 0.0f;

        if (_ctx.useTriggerSpeedLoopOnly) {
            // 连发或校准反转时，直接使用速度环
            spdTarget = _ctx.targetTriggerSpeed;
        } else {
            // 单发或就绪锁死时，使用 角度->速度 的位置外环
            static float targetTriggerAngle;
            static float realTriggerAngle;
            targetTriggerAngle = (float)(_ctx.targetTriggerEcd) / (float)(8192 * 36) * 2 * M_PI;

            int32_t ecd        = _ctx.fdb.triggerEcd + _ctx.fdb.triggerRound * 8192 - _ctx.triggerOffset;
            while (ecd < 0) {
                ecd += 8192 * 36;
            }
            realTriggerAngle = (float)(ecd) / (float)(8192 * 36) * 2 * M_PI;

            float err        = targetTriggerAngle - realTriggerAngle;

            while (err > M_PI) {
                err -= 2.0 * M_PI;
            }
            while (err < -M_PI) {
                err += 2.0 * M_PI;
            }

            float alignedTgtTrigger = realTriggerAngle + err;

            spdTarget               = _triggerPosPid.calculate(alignedTgtTrigger, realTriggerAngle);
        }

        // 最终的速度->电流 内环计算
        out.triggerCurrent = _triggerSpdPid.calculate(spdTarget, _ctx.fdb.trigger.vel);
        // out.triggerCurrent = 0;
    }
}