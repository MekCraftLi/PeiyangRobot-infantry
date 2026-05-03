/**
 *******************************************************************************
 * @file    trigger-decorator-cycle.h
 * @brief   多值循环触发器装饰器 (Cycle Decorator)
 *
 * 效果：当被装饰的原 Trigger 产生一次有效的 Triggered 边沿时，
 * 内部计数器 +1 并对 cycleLength 取模。
 * 提供 getIndex() 获取当前循环索引 (0 ~ cycleLength-1)。
 *
 * 用法：
 *   TriggerEdge rise(0.0f, EdgeType::Rising);
 *   TriggerCycle cycle(rise, 3);  // 0 → 1 → 2 → 0 → ...
 *
 *   cycle.update(value, dt);
 *   uint8_t mode = cycle.getIndex(); // 0, 1, 2
 *
 *******************************************************************************
 * @author  MekLi
 * @date    2026/5/3
 * @version 1.0
 *******************************************************************************
 */

#ifndef INFANTRY_CHASSIS_TRIGGER_DECORATOR_CYCLE_H
#define INFANTRY_CHASSIS_TRIGGER_DECORATOR_CYCLE_H

#include "../triggers.h"
#include <cstdint>

class TriggerCycle : public InputTrigger {
public:
    /**
     * @brief 构造函数
     * @param base_trigger 被装饰的原触发器
     * @param cycle_length 循环长度 (≥2)
     */
    explicit TriggerCycle(InputTrigger& base_trigger, uint8_t cycle_length)
        : _baseTrigger(base_trigger),
          _cycleLength(cycle_length < 2 ? 2 : cycle_length),
          _index(0),
          _wasTriggeredLastFrame(false) {}

    TriggerState update(float value, float dt) override {
        TriggerState baseState = _baseTrigger.update(value, dt);
        bool isTriggeredNow = (baseState == TriggerState::Triggered);

        if (isTriggeredNow && !_wasTriggeredLastFrame) {
            _index = (_index + 1) % _cycleLength;
        }

        _wasTriggeredLastFrame = isTriggeredNow;
        return isTriggeredNow ? TriggerState::Triggered : TriggerState::None;
    }

    void reset() override {
        InputTrigger::reset();
        _baseTrigger.reset();
        _index = 0;
        _wasTriggeredLastFrame = false;
    }

    /** @brief 获取当前循环索引 (0 ~ cycleLength-1) */
    uint8_t getIndex() const { return _index; }

    /** @brief 获取循环长度 */
    uint8_t getCycleLength() const { return _cycleLength; }

    /** @brief 设置当前索引 */
    void setIndex(uint8_t idx) { _index = idx % _cycleLength; }

private:
    InputTrigger& _baseTrigger;
    uint8_t _cycleLength;
    uint8_t _index;
    bool _wasTriggeredLastFrame;
};

#endif /*INFANTRY_CHASSIS_TRIGGER_DECORATOR_CYCLE_H*/
