/**
 *******************************************************************************
 * @file    action.h
 * @brief   简要描述
 *******************************************************************************
 * @attention
 *
 * none
 *
 *******************************************************************************
 * @note
 *
 * level IV
 *
 *******************************************************************************
 * @author  MekLi
 * @date    2026/2/9
 * @version 1.0
 *******************************************************************************
 */


/* Define to prevent recursive inclusion -----------------------------------------------------------------------------*/

#ifndef INFANTRY_CHASSIS_ACTION_H
#define INFANTRY_CHASSIS_ACTION_H



/*-------- 1. includes and imports -----------------------------------------------------------------------------------*/

#include <vector>
#include <functional>
#include "triggers.h"



/*-------- 2. enum and define ----------------------------------------------------------------------------------------*/




/*-------- 3. interface ----------------------------------------------------------------------------------------------*/

struct ActionBinding {
    IInputControl* control;  // Level 1: 物理控件 (数据源)
    InputTrigger* trigger;   // Level 3: 触发逻辑 (状态机)
};

class InputAction {
public:
    InputAction() : m_state(TriggerState::None){}

    // 绑定物理输入源，并挂载触发器
    void Bind(IInputControl* input_source, InputTrigger* trigger = nullptr) {
        m_bindings.push_back({input_source, trigger});
    }

    // 更新所有绑定 (由 Input_Task 调用)
    void Update(float dt) {
        float max_val = 0.0f;
        TriggerState combined_state = TriggerState::None;

        for (auto& bind : m_bindings) {
            // 获取物理值 (Level 1)
            float raw_val = bind.control->get();

            // 计算触发状态 (Level 3)
            TriggerState state = bind.trigger->update(raw_val, dt);

            // 仲裁逻辑：
            // A. 状态优先级：Triggered > Ongoing > Started > None
            if (state > combined_state) {
                combined_state = state;
            }

            // B. 数值优先级：取绝对值最大者 (Winner takes all)
            if (std::abs(raw_val) > std::abs(max_val)) {
                max_val = raw_val;
            }
        }

        m_state = combined_state;
        m_value = max_val;
    }

    bool IsTriggered() const { return m_state == TriggerState::Triggered; }
    float GetValue() const { return m_value; }

private:
    struct Binding {
        const float* source;    // 指向 RemoteBase 里的 m_val
        InputTrigger* trigger;  // 策略对象
    };

    std::vector<ActionBinding> m_bindings;
    TriggerState m_state = TriggerState::None;
    float m_value = 0.0f;
};




/*-------- 4. decorator ----------------------------------------------------------------------------------------------*/




/*-------- 5. factories ----------------------------------------------------------------------------------------------*/





#endif /*INFANTRY_CHASSIS_ACTION_H*/
