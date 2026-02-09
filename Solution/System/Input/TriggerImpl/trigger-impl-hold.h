/**
 *******************************************************************************
 * @file    trigger-impl-hold.h
 * @brief   长按触发器
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
 * @date    2026/2/9
 * @version 1.0
 *******************************************************************************
 */


/* Define to prevent recursive inclusion -----------------------------------------------------------------------------*/

#ifndef INFANTRY_CHASSIS_TRIGGER_IMPL_HOLD_H
#define INFANTRY_CHASSIS_TRIGGER_IMPL_HOLD_H



/*-------- 1. includes and imports -----------------------------------------------------------------------------------*/

#include "../triggers.h"



/*-------- 2. enum and define ----------------------------------------------------------------------------------------*/




/*-------- 3. interface ----------------------------------------------------------------------------------------------*/

class TriggerHold : public InputTrigger {
public:
    // hold_time: 需要按住多少秒才触发
    // one_shot: true=只触发一次; false=时间到了之后每帧都触发
    TriggerHold(float threshold, float hold_time, bool one_shot = false)
        : _threshold(threshold), _holdTime(hold_time), _oneShot(one_shot), _timer(0.0f) {}

    TriggerState update(float value, float dt) override {
        bool is_down = (value >= _threshold);

        if (!is_down) {
            _timer = 0.0f; // 松开重置
            return TriggerState::None;
        }

        // 正在按住
        if (_timer < _holdTime) {
            _timer += dt; // 累加时间
            return TriggerState::Started; // 还没到时间
        }

        // 时间已到
        if (_oneShot) {
            // 如果是单次模式，且之前已经触发过(这里简化逻辑，通常需状态位)
            // 简单实现略
            return TriggerState::Triggered;
        } else {
            return TriggerState::Triggered; // 持续触发
        }
    }

private:
    float _threshold;
    float _holdTime;
    bool _oneShot;
    float _timer;
};



/*-------- 4. decorator ----------------------------------------------------------------------------------------------*/




/*-------- 5. factories ----------------------------------------------------------------------------------------------*/





#endif /*INFANTRY_CHASSIS_TRIGGER_IMPL_HOLD_H*/
