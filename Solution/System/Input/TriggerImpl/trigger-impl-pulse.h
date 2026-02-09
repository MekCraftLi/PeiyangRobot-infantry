/**
 *******************************************************************************
 * @file    trigger-impl-pulse.h
 * @brief   脉冲触发器
 *******************************************************************************
 * @attention
 *
 * none
 *
 *******************************************************************************
 * @note
 *
 * 触发条件：控件值产生上升沿
 *
 *******************************************************************************
 * @author  MekLi
 * @date    2026/2/9
 * @version 1.0
 *******************************************************************************
 */


/* Define to prevent recursive inclusion -----------------------------------------------------------------------------*/

#ifndef INFANTRY_CHASSIS_TRIGGER_IMPL_PULSE_H
#define INFANTRY_CHASSIS_TRIGGER_IMPL_PULSE_H



/*-------- 1. includes and imports -----------------------------------------------------------------------------------*/

#include "../triggers.h"



/*-------- 2. enum and define ----------------------------------------------------------------------------------------*/




/*-------- 3. interface ----------------------------------------------------------------------------------------------*/

class Trigger_Pulse : public InputTrigger {
public:
    explicit Trigger_Pulse(float threshold = 0.5f)
        : _threshold(threshold), _hasTriggered(false) {}

    TriggerState update(float value, float dt) override {
        bool is_down = (value >= _threshold);

        if (is_down) {
            if (!_hasTriggered) {
                _hasTriggered = true; // 锁定
                return TriggerState::Triggered; // 仅第一帧触发
            }
            return TriggerState::Ongoing; // 按住中，但不再次触发
        } else {
            _hasTriggered = false; // 松开，重置锁
            return TriggerState::None;
        }
    }

private:
    float _threshold;
    bool _hasTriggered;
};



/*-------- 4. decorator ----------------------------------------------------------------------------------------------*/




/*-------- 5. factories ----------------------------------------------------------------------------------------------*/





#endif /*INFANTRY_CHASSIS_TRIGGER_IMPL_PULSE_H*/
