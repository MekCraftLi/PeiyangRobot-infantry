/**
 *******************************************************************************
 * @file    ui-maker-app.cpp
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
 * @date    2026/3/20
 * @version 1.0
 *******************************************************************************
 */




/* ------- define ----------------------------------------------------------------------------------------------------*/





/* ------- include ---------------------------------------------------------------------------------------------------*/



/* I. header */

#include "ui-maker-app.h"

#include "System/DataHub/blackboard.h"
#include "System/DataHub/data-def.h"
#include "System/DataHub/referee-data-hub.h"
#include "System/DataHub/referee-protocol.h"
#include "System/DataHub/ui-protocol.h"
#include "System/Service/ui-renderer-srvc.h"

/* II. other application */


/* III. standard lib */




/* ------- class prototypes-----------------------------------------------------------------------------------------*/




/* ------- macro -----------------------------------------------------------------------------------------------------*/





/* ------- variables -------------------------------------------------------------------------------------------------*/

[[maybe_unused]] static auto& forceInit = UiMakerApp::instance();



/* ------- application attribute -------------------------------------------------------------------------------------*/

#define APPLICATION_ENABLE     true

#define APPLICATION_NAME       "UiMaker"

#define APPLICATION_STACK_SIZE 512

#define APPLICATION_PRIORITY   4

static StackType_t appStack[APPLICATION_STACK_SIZE];



/* ------- message interface attribute -------------------------------------------------------------------------------*/






/* ------- function prototypes ---------------------------------------------------------------------------------------*/





/* ------- function implement ----------------------------------------------------------------------------------------*/


UiMakerApp::UiMakerApp()
    : PeriodicApp(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE,  appStack, APPLICATION_PRIORITY, 1000){
}


void UiMakerApp::init() {
    /* driver object initialize */
    // 初始状态全部默认配置为 Add
    _capBoxCfg  = {{'C','P','B'}, GraphicAction::Add, 0, UiColor::White, 2, 860, 450};
    _capBarCfg  = {{'C','P','L'}, GraphicAction::Add, 1, UiColor::Green, 16, 860, 450};
    _capTextCfg = {{'C','P','T'}, GraphicAction::Add, 1, UiColor::Green, 2, 960, 470};
}

void UiMakerApp::run() {
    auto& ui = UiRendererSrvc::instance();

    // -------------------------------------------------------------
    // 1. 掉线重连与上线检测 (RM 实战防弹机制)
    // -------------------------------------------------------------
    RMRobotStatus robotStatus{};
    RefereeDataHub::instance().robotStatus.read(robotStatus);

    if (robotStatus.robotId == 0) return; // 裁判系统未连通，直接退出

    // 检测到 ID 从 0 突变为有效值，说明客户端刚连上，必须重新 Add
    if (_lastRobotId == 0 && robotStatus.robotId != 0) {
        _uiNeedsInit = true;
    }
    _lastRobotId = robotStatus.robotId;

    // -------------------------------------------------------------
    // 2. 状态机路由
    // -------------------------------------------------------------
    if (_uiNeedsInit) {
        /* ========== 阶段 A：全量 Add (只执行一帧) ========== */

        // 强制确保所有属性恢复为 Add
        _capBoxCfg.action  = GraphicAction::Add;
        _capBarCfg.action  = GraphicAction::Add;
        _capTextCfg.action = GraphicAction::Add;

        // 1. 绘制静态背景图层 (只发这一次，绝不进入高频循环)
        ui.drawRectangle(_capBoxCfg, 1060, 470);

        // 2. 绘制动态图形的初次 Add
        drawDynamicGraphics();

        // 3. 核心魔法：将动态图形的 Action 自动翻转为 Update
        _capBarCfg.action  = GraphicAction::Update;
        _capTextCfg.action = GraphicAction::Update;

        // 4. 退出初始化状态
        _uiNeedsInit = false;

    } else {
        /* ========== 阶段 B：高频 Update (常态循环) ========== */

        // 直接调用，此时 _capBarCfg.action 已经是 Update 了
        drawDynamicGraphics();
    }
}

// -------------------------------------------------------------
// 抽离出的纯动态逻辑（不关心当前是 Add 还是 Update）
// -------------------------------------------------------------
void UiMakerApp::drawDynamicGraphics() {
    auto& ui = UiRendererSrvc::instance();
    SuperCapState capState{};
    Blackboard::instance().capState.read(capState);
    // 计算电容数据
    float currentV = capState.voltage;
    uint16_t barEndX = 860 + static_cast<uint16_t>((currentV - 15.0f) / 13.0f * 200.0f);

    // 变色逻辑
    if(currentV > 24.0f) {
        _capBarCfg.color = UiColor::Green; _capTextCfg.color = UiColor::Green;
    } else if(currentV > 16.0f) {
        _capBarCfg.color = UiColor::Yellow; _capTextCfg.color = UiColor::Yellow;
    } else {
        _capBarCfg.color = UiColor::Orange; _capTextCfg.color = UiColor::Orange;
    }

    // 推入渲染管线 (底层的 Action 会跟随状态机自动变化)
    ui.drawLine(_capBarCfg, barEndX, 450);
    ui.drawFloat(_capTextCfg, 15, currentV);
}