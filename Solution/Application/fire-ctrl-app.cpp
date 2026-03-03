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

/* II. other application */


/* III. standard lib */




/* ------- class prototypes-----------------------------------------------------------------------------------------*/




/* ------- macro -----------------------------------------------------------------------------------------------------*/





/* ------- variables -------------------------------------------------------------------------------------------------*/

[[maybe_unused]] static auto& forceInit = FireCtrlApp::instance();



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
    // 1. 从黑板读取数据
    Blackboard::instance().shootCmd.read(_ctx.cmd);
    Blackboard::instance().boosterState.read(_ctx.fdb);

    // 2. 提取瞬态边沿事件
    updateTransientEvent();

    // 3. 驱动状态机流转 (仅计算目标角度/速度，不涉及 PID)
    _fsm.execute(_ctx);

    // 4. 核心算法：统一进行 PID 计算
    BoosterOutput finalOut{};
    calculateCurrents(finalOut);

    // 5. 将计算好的电流写入黑板，供 motor-actuator 线程发送 CAN
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
}

void FireCtrlApp::StatePassive::execute(FireCtrlCtx& ctx) {
    // 锁死当前位置，防止掉电滑转
    ctx.targetTriggerEcd = ctx.fdb.trigger.pos;

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
}

void FireCtrlApp::StateSpinUp::execute(FireCtrlCtx& ctx) {
    if (ctx.transientEvent == ShootEvent::FRIC_TOGGLE || ctx.transientEvent == ShootEvent::EMERGENCY_STOP) {
        request_switch(&instance()._statePassive);
        return;
    }

    ctx.targetTriggerEcd = ctx.fdb.triggerEcd + ctx.fdb.triggerRound * 8192 - ctx.triggerOffset; // 锁死拨弹盘

    // 判断摩擦轮是否达标 (容差 5%)
    if (std::abs(ctx.fdb.fric[Config::Hardware::MotorTopo::FRIC_LEFT_ID].vel) > ctx.targetFricSpeed * 0.95f &&
        std::abs(ctx.fdb.fric[Config::Hardware::MotorTopo::FRIC_RIGHT_ID].vel) > ctx.targetFricSpeed * 0.95f) {
        request_switch(&instance()._stateReady);
    }
}



/* ========================================================
 * 3. 摩擦轮准备完成
 * ======================================================== */


void FireCtrlApp::StateReady::enter(FireCtrlCtx& ctx) {
    ctx.useTriggerSpeedLoopOnly = false;
}

void FireCtrlApp::StateReady::execute(FireCtrlCtx& ctx) {
    if (ctx.transientEvent == ShootEvent::FRIC_TOGGLE || ctx.transientEvent == ShootEvent::EMERGENCY_STOP) {
        request_switch(&instance()._statePassive);
        return;
    }

    if (ctx.transientEvent == ShootEvent::SINGLE_FIRE) {
        if (!ctx.isCalibrated)
            request_switch(&instance()._stateCaliReverse);
        else
            request_switch(&instance()._stateSingleFire);
    } else if (ctx.transientEvent == ShootEvent::BURST_START) {
        ctx.isCalibrated = false; // 连发会破坏绝对槽位
        request_switch(&instance()._stateBurstFire);
    }
}




/* ========================================================
 * 4. 校准回复
 * ======================================================== */



void FireCtrlApp::StateCaliReverse::enter(FireCtrlCtx& ctx) {
    ctx.blockTimer              = 0;
    ctx.useTriggerSpeedLoopOnly = true; // 开启纯速度环反转
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
    // 以当前的编码器值为新的起始点
    ctx.triggerOffset = (ctx.fdb.triggerEcd + ctx.fdb.triggerRound * 8192 + 8192 * 2) % (8192 * 36);
}




/* ========================================================
 * 5. 校准前移
 * ======================================================== */


void FireCtrlApp::StateCaliForward::enter(FireCtrlCtx& ctx) {
    ctx.useTriggerSpeedLoopOnly = false; // 恢复位置环
    ctx.targetTriggerEcd        = 0;
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
    ctx.blockTimer = 0;
    ctx.targetTriggerEcd += 8192 * 36 / 8;
    ctx.useTriggerSpeedLoopOnly = false;
}

void FireCtrlApp::StateSingleFire::execute(FireCtrlCtx& ctx) {
    if (ctx.transientEvent == ShootEvent::FRIC_TOGGLE || ctx.transientEvent == ShootEvent::EMERGENCY_STOP) {
        request_switch(&instance()._statePassive);
        return;
    }

    // 将目标ECD换算为弧度制
    float targetTriggerAngle = (float)(ctx.targetTriggerEcd) / (float)(8192 * 36) * 2 * M_PI;

    // 计算当前的ECD值
    int32_t ecd = ctx.fdb.triggerEcd + ctx.fdb.triggerRound * 8192 - ctx.triggerOffset;
    // 进行循环限幅
    while ( ecd < 0 ) {ecd += 8192 * 36 ;}
    // 将当前ECD换算为真实角度
    float realTriggerAngle = (float)(ecd) / (float)(8192 * 36) * 2 * M_PI;

    // 计算角度误差
    float err = targetTriggerAngle - realTriggerAngle;

    while (err > M_PI) {
        err -= 2.0 * M_PI;
    }
    while (err < -M_PI) {
        err += 2.0 * M_PI;
    }

    // 卡弹检测：角度误差大且速度极低
    if (err > 15.0f && std::abs(ctx.fdb.trigger.vel) < 10.0f) {
        ctx.blockTimer++;
        if (ctx.blockTimer > 50){}
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
}

void FireCtrlApp::StateBurstFire::execute(FireCtrlCtx& ctx) {
    if (ctx.transientEvent == ShootEvent::BURST_STOP) {
        request_switch(&instance()._stateReady);
        return;
    }
    if (ctx.transientEvent == ShootEvent::FRIC_TOGGLE || ctx.transientEvent == ShootEvent::EMERGENCY_STOP) {
        request_switch(&instance()._statePassive);
        return;
    }

    ctx.targetTriggerSpeed = Config::Hardware::MotorTopo::TRIGGER_SPEED;

    if (std::abs(ctx.fdb.trigger.vel) < 10.0f) {
        ctx.blockTimer++;
        if (ctx.blockTimer > 50){}
           // request_switch(&instance()._stateJamClear);
    } else {
        ctx.blockTimer = 0;
    }
}

void FireCtrlApp::StateBurstFire::exit(FireCtrlCtx& ctx) {
    // 退出连发时，利用当前物理位置，向上取整找最近的 45 度槽位！这是防松手卡壳的神技。
    ctx.targetTriggerEcd        = std::ceil(ctx.fdb.trigger.pos / ANGLE_PER_BULLET) * ANGLE_PER_BULLET;
    ctx.useTriggerSpeedLoopOnly = false; // 切回位置环进行急刹车
}



/* ========================================================
 * 8. 堵弹
 * ======================================================== */

void FireCtrlApp::StateJamClear::enter(FireCtrlCtx& ctx) {
    ctx.stateTimer              = 0;
    ctx.useTriggerSpeedLoopOnly = true;
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
    // 【摩擦轮】纯速度环
    out.fricLeftCurrent = _fricLeftSpdPid.calculate(
        _ctx.targetFricSpeed, _ctx.fdb.fric[(uint8_t)Config::Hardware::MotorTopo::FRIC_LEFT_ID].vel);
    // 右摩擦轮通常反向安装，因此目标速度取反
    out.fricRightCurrent = _fricRightSpdPid.calculate(
        -_ctx.targetFricSpeed, _ctx.fdb.fric[(uint8_t)Config::Hardware::MotorTopo::FRIC_RIGHT_ID].vel);

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

            int32_t ecd = _ctx.fdb.triggerEcd + _ctx.fdb.triggerRound * 8192 - _ctx.triggerOffset;
            while ( ecd < 0 ) {ecd += 8192 * 36 ;}
            realTriggerAngle = (float)(ecd) / (float)(8192 * 36) * 2 * M_PI;

            float err = targetTriggerAngle - realTriggerAngle;

            while (err > M_PI) {
                err -= 2.0 * M_PI;
            }
            while (err < -M_PI) {
                err += 2.0 * M_PI;
            }

            float alignedTgtTrigger = realTriggerAngle + err;



            spdTarget = _triggerPosPid.calculate(alignedTgtTrigger, realTriggerAngle);
        }

        // 最终的速度->电流 内环计算
        out.triggerCurrent = _triggerSpdPid.calculate(spdTarget, _ctx.fdb.trigger.vel);
        //out.triggerCurrent = 0;
    }
}