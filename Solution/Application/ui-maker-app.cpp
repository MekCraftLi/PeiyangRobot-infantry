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
    // 1. 配置上层图形属性 (对应底层 15 字节载荷中的通用属性)
    GraphicProperties centerLineProps = {
        {'L', 'I', 'N'},       // name:      图形索引名为 "LIN"
        GraphicAction::Add,    // action:    操作类型为 "增加" (1)
        0,                     // layer:     图层号为 0
        UiColor::White,        // color:     颜色为 "白色" (8)
        3,                     // lineWidth: 线宽为 3 像素
        860,                   // startX:    起点 X 坐标 (屏幕中心偏左)
        540                    // startY:    起点 Y 坐标 (屏幕绝对垂直中心)
    };

    // 2. 将属性和终点坐标传给渲染管线
    // 这里的 1060 和 540 分别对应底层的 endX 和 endY
    UiRendererSrvc::instance().drawLine(centerLineProps, 1060, 540);
}
