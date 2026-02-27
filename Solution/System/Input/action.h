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
    void bind(IInputControl* input_source, InputTrigger* trigger = nullptr) {
        m_bindings.push_back({input_source, trigger});
    }

    // 更新所有绑定 (由 Input_Task 调用)
    void update(float dt) {
        float final_output = 0.0f; // 最终要输出的值
        TriggerState combined_state = TriggerState::None;

        for (auto& bind : m_bindings) {
            float raw_val = bind.control->get();
            float processed_val = raw_val; // 经过修饰器/触发器处理后的值
            TriggerState state = TriggerState::None;

            if (bind.trigger != nullptr) {
                // 1. 假设你的 Trigger 以后支持修改数值（比如死区把小数值改为 0）
                // processed_val = bind.trigger->process(raw_val);

                // 2. 更新状态
                state = bind.trigger->update(processed_val, dt);

                // 【特别修复】如果你没写 process 函数，至少在这里做个强制屏蔽：
                if (state == TriggerState::None) {
                    processed_val = 0.0f; // 触发器认为没触发（比如在死区内），强制归零！
                }
            } else {
                if (std::abs(raw_val) > 1e-5f) {
                    state = TriggerState::Triggered;
                }
            }

            if (state > combined_state) {
                combined_state = state;
            }

            // 【重要修复】比较的是经过处理后的值，而不是 raw_val
            if (std::abs(processed_val) > std::abs(final_output)) {
                final_output = processed_val;
            }
        }

        m_state = combined_state;
        m_value = final_output; // 输出干净的数据
    }

    bool isTriggered() const { return m_state == TriggerState::Triggered; }
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


// --- 模式切换意图 ---
extern InputAction Action_CtrlMode;   // 决定系统控制权 (映射到右侧开关)
extern InputAction Action_FuncMode;   // 决定附加功能 (映射到左侧开关)

// --- 底盘运动意图 ---
extern InputAction Action_MoveX;      // 前后移动
extern InputAction Action_MoveY;      // 左右移动
extern InputAction Action_Spin;       // 底盘自旋

// --- 云台/发射意图 ---
extern InputAction Action_GimbalYaw;
extern InputAction Action_GimbalPitch;

/*-------- 4. decorator ----------------------------------------------------------------------------------------------*/




/*-------- 5. factories ----------------------------------------------------------------------------------------------*/





#endif /*INFANTRY_CHASSIS_ACTION_H*/
