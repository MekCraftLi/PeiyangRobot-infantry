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
    // 初始化测试图形属性
    _dashboardCfg = {{'T', 'S', 'T'}, GraphicAction::Add, 0, UiColor::Green, 2, 0, 0};
}


void UiMakerApp::run() {
    // -------------------------------------------------------------
    // 【阶段 1 通信测试】：每秒钟在屏幕中央画一个圆，测试链路封装是否正确。
    // 如果屏幕上成功出现绿色的圆圈，说明底层 Frame、CRC、DMA 全部打通！
    // -------------------------------------------------------------

    _dashboardCfg.startX = 960;
    _dashboardCfg.startY = 540;

    // 调用服务层的接口，将其推入渲染队列
    UiRendererSrvc::instance().drawCircle(_dashboardCfg, 50);
}
