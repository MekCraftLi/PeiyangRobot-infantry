/**
 *******************************************************************************
 * @file    blackboard.h
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
 * @date    2026/2/26
 * @version 1.0
 *******************************************************************************
 */


/* Define to prevent recursive inclusion -----------------------------------------------------------------------------*/

#ifndef INFANTRY_CHASSIS_BLACKBOARD_H
#define INFANTRY_CHASSIS_BLACKBOARD_H



/*-------- 1. includes and imports -----------------------------------------------------------------------------------*/

#include <cstdint>
#include "FreeRTOS.h"
#include "task.h"



/*-------- 2. enum and define ----------------------------------------------------------------------------------------*/



// 内存屏障，防止编译器重排指令 (ARM Cortex-M 适用)
#define MEMORY_BARRIER() __asm volatile ("dmb" ::: "memory")

/**
 * @brief 顺序锁包装器 (Seqlock)
 * @tparam T 必须是可通过赋值拷贝的平凡数据结构 (POD)
 */
template <typename T>
class SeqVariable {
public:
    SeqVariable() : _seq(0) {}

    /**
     * @brief 写者接口 (生产者如 ISR 或解析任务调用)
     * @note 会短暂关闭中断以保证结构体拷贝时不被更高优先级抢占
     */
    void Write(const T& new_data) {
        // 屏蔽操作系统的任务调度和普通中断，耗时仅几十纳秒
        taskENTER_CRITICAL();

        _seq++; // 奇数：标记“正在写入脏数据”
        MEMORY_BARRIER();

        _data = new_data; // 核心：多变量整体内存拷贝

        MEMORY_BARRIER();
        _seq++; // 偶数：标记“写入完成，数据干净”

        taskEXIT_CRITICAL(); // 恢复中断
    }

    /**
     * @brief 读者接口 (消费者如控制任务调用)
     * @note 绝对无锁，绝对不阻塞，通过自旋重试保证数据完整
     */
    void Read(T& out_data) const {
        uint32_t seq_before;
        do {
            seq_before = _seq;
            MEMORY_BARRIER();

            // 如果 seq 是奇数，说明此时写者正在操作，自旋等待 (在单核中其实极少发生，除非同优先级)
            if (seq_before & 1) {
                continue;
            }

            out_data = _data; // 拷贝走整个结构体

            MEMORY_BARRIER();

            // 关键逻辑：如果拷贝完发现版本号变了，说明刚才拷贝进行到一半时，
            // 被高优先级的写者抢占并覆盖了数据，这组数据作废，重新执行拷贝！
        } while (seq_before != _seq);
    }

private:
    volatile uint32_t _seq; // 必须 volatile，强制每次从内存读取，防止编译器优化掉 while 循环
    T _data;
};

/**
 * @brief 全局黑板 (Global Blackboard)
 * @note 采用单例模式，汇聚了机器人所有的瞬时状态、指令和输出
 */
class Blackboard {
public:
    // 获取全局唯一实例
    static Blackboard* instance() {
        static Blackboard instance;
        return &instance;
    }

    // =========================================================
    // [感知区] (Sensors & Feedback) -> 硬件中断/采集任务 写入
    // =========================================================
    SeqVariable<ImuStdData>      imu;
    SeqVariable<ChassisFeedback> chassisfb;

    // =========================================================
    // [意图区] (Commands & Intent)  -> 通信/遥控器任务 写入
    // =========================================================
    SeqVariable<ChassisCmd>      chassisCmd;
    SeqVariable<GimbalCmd>       gimbalCmd;

    // =========================================================
    // [执行区] (Outputs & Actions)  -> 核心算法控制任务 写入
    // =========================================================
    SeqVariable<MotorOutputCmd>  chassisOutput;
    SeqVariable<MotorOutputCmd>  gimbalOutput;

private:
    Blackboard() = default; // 禁用外部构造
    Blackboard(const Blackboard&) = delete; // 禁用拷贝
    Blackboard& operator=(const Blackboard&) = delete;
};
/*-------- 3. interface ----------------------------------------------------------------------------------------------*/




/*-------- 4. decorator ----------------------------------------------------------------------------------------------*/




/*-------- 5. factories ----------------------------------------------------------------------------------------------*/





#endif /*INFANTRY_CHASSIS_BLACKBOARD_H*/
