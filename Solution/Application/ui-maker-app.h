/**
 *******************************************************************************
 * @file    ui-maker-app.h
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


/* Define to prevent recursive inclusion -----------------------------------------------------------------------------*/

#ifndef INFANTRY_UI_MAKER_APP_H
#define INFANTRY_UI_MAKER_APP_H




/*-------- 1. includes and imports -----------------------------------------------------------------------------------*/

#ifdef __cplusplus

/* I. interface */

#include "System/DataHub/ui-protocol.h"
#include "System/Thread/application-base.h"
#include "tools/crtp.h"

/* II. OS */


/* III. middlewares */


/* IV. drivers */


/* V. standard lib */





/*-------- 2. enum ---------------------------------------------------------------------------------------------------*/




/*-------- 3. interface ---------------------------------------------------------------------------------------------*/

class UiMakerApp final : public PeriodicApp, public Singleton<UiMakerApp> {
  public:
    UiMakerApp();

    void init() override;


    void run() override;

    /************ setter & getter ***********/



  private:
    // 状态机核心标志位
    bool _uiNeedsInit = true;      // 是否需要进行全量 Add 初始化
    uint16_t _lastRobotId = 0;     // 用于检测裁判系统是否掉线重连

    // 将属性定义为类成员
    GraphicProperties _capBoxCfg;
    GraphicProperties _capBarCfg;
    GraphicProperties _capTextCfg;

    // 抽离动态绘制函数
    void drawDynamicGraphics();
    /* message interface */

    // 1. message queue

    // 2. mutex

    // 3. semphr

    // 4. notify

    // 5. stream or message

    // 6. event group
};
#endif


#ifdef __cplusplus
extern "C" {
#endif

/* C Interface */

#ifdef __cplusplus
}
#endif




/*-------- 4. decorator ----------------------------------------------------------------------------------------------*/




/*-------- 5. factories ----------------------------------------------------------------------------------------------*/

#endif