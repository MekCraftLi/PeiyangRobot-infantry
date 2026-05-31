/**
 *******************************************************************************
 * @file    heat-controller.h
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
 * @date    2026/3/16
 * @version 1.0
 *******************************************************************************
 */


/* Define to prevent recursive inclusion -----------------------------------------------------------------------------*/

#ifndef INFANTRY_HEAT_CONTROLLER_H
#define INFANTRY_HEAT_CONTROLLER_H



/*-------- 1. includes and imports -----------------------------------------------------------------------------------*/




/*-------- 2. enum and define ----------------------------------------------------------------------------------------*/




/*-------- 3. interface ----------------------------------------------------------------------------------------------*/
class HeatController {
  public:
    static constexpr float HEAT_PER_BULLET     = 10.0f;
    static constexpr float SAFE_MARGIN         = 40.0f;

    // 裁判系统最大延迟容忍时间 (毫秒)。通常 10Hz 更新对应 100ms，这里给 200ms 绝对安全
    static constexpr uint32_t REFEREE_DELAY_MS = 200;

    // 热量限制开关: false 时跳过所有热量判断 (调试/测试用)
    void setHeatLimitEnabled(bool enabled) { _heatLimitEnabled = enabled; }
    [[nodiscard]] bool isHeatLimitEnabled() const { return _heatLimitEnabled; }

    // 当热量上限为零 或 开关关闭时，视为无热量限制
    [[nodiscard]] bool noHeatLimitSystem() const {
        return !_heatLimitEnabled || _heatLimit <= 0.0f;
    }

    HeatController()                           = default;

    void tickCooling(float dt) {
        if (_coolingRate > 0.0f && _localHeat > 0.0f) {
            // 热量随时间自然衰减
            _localHeat -= _coolingRate * dt;
            if (_localHeat < 0.0f) {
                _localHeat = 0.0f;
            }
        }
    }

    /**
     * @brief [核心修改]：带时间窗状态观测的硬同步
     * @param current_time_ms 传入当前的系统运行时间 (例如 xTaskGetTickCount())
     */
    void syncWithReferee(uint16_t refHeat, uint16_t refLimit, uint16_t refCoolingRate, uint32_t current_time_ms) {
        _heatLimit   = static_cast<float>(refLimit);
        _coolingRate = static_cast<float>(refCoolingRate);

        // 如果距离上一次物理开火已经过去了足够久，证明裁判系统数据已是最新真实状态
        if ((current_time_ms - _lastShotTimeMs) > REFEREE_DELAY_MS) {
            // 【消除误差】：完全信任裁判系统，直接覆写，清空本地累计的漂移误差
            _localHeat = static_cast<float>(refHeat);
        } else {
            // 【交火保命】：刚开完火，裁判系统可能没反应过来，取最大值防止超限
            _localHeat = std::max(_localHeat, static_cast<float>(refHeat));
        }
    }

    /**
     * @brief [核心修改]：记录发射事件同时记录时间戳
     */
    void recordBulletShot(uint32_t current_time_ms) {
        _localHeat += HEAT_PER_BULLET;
        _lastShotTimeMs = current_time_ms; // 刷新最后一次开火的时间
    }


    [[nodiscard]] bool canShootSingle() const {
        if (noHeatLimitSystem()) return true;
        static uint8_t canShootSingle;
        static float localHeat;
        canShootSingle = (_localHeat + HEAT_PER_BULLET) <= (_heatLimit - SAFE_MARGIN);
        localHeat      = _localHeat;
        (void)localHeat;
        (void)(canShootSingle);
        return canShootSingle;
    }

    [[nodiscard]] bool canShootBurst() const {
        if (noHeatLimitSystem()) return true;
        // 连发模式要求至少能容忍4发，以提供更好的续杯体验
        return (_localHeat + 4 * HEAT_PER_BULLET) <= (_heatLimit - SAFE_MARGIN);
    }

    // 【新增】连发模式是否处于高热量警戒区
    [[nodiscard]] bool isApproachingHeatLimit() const {
        if (noHeatLimitSystem()) return false;
        // 当剩余热量不足以容纳两发子弹时，视为逼近上限 (进入警戒区)
        return _localHeat > (_heatLimit - SAFE_MARGIN);
    }


    [[nodiscard]] float getLocalHeat() const { return _localHeat; }

  private:
    float _localHeat{0.0f};
    float _heatLimit{240.0f};
    float _coolingRate{40.0f};

    // 记录上一次真实打出子弹的系统时间戳
    uint32_t _lastShotTimeMs{0};

    // 热量限制开关 (默认开启)
    bool _heatLimitEnabled{true};
};
/*-------- 4. decorator ----------------------------------------------------------------------------------------------*/




/*-------- 5. factories ----------------------------------------------------------------------------------------------*/





#endif /*INFANTRY_HEAT_CONTROLLER_H*/
