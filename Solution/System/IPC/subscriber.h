/**
 *******************************************************************************
 * @file    subscriber.h
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
 * @date    2026/2/4
 * @version 1.0
 *******************************************************************************
 */


/* Define to prevent recursive inclusion -----------------------------------------------------------------------------*/

#ifndef INFANTRY_CHASSIS_SUBSCRIBER_H
#define INFANTRY_CHASSIS_SUBSCRIBER_H



/*-------- 1. includes and imports -----------------------------------------------------------------------------------*/

#include "topic.h"



/*-------- 2. enum and define ----------------------------------------------------------------------------------------*/




/*-------- 3. interface ----------------------------------------------------------------------------------------------*/

template <typename T> class Subscriber {
  public:
    /**
     * @brief 构造函数：自动订阅
     * @param topic 要订阅的话题
     * @param queueLen 队列深度 (控制任务通常设为1，日志任务设大点)
     */
    Subscriber(Topic<T>& topic, uint32_t queueLen = 1) {
        // 1. 创建属于自己的收件箱
        m_queue = xQueueCreate(queueLen, sizeof(T));

        // 2. 去广播站注册
        topic.Subscribe(m_queue);
    }

    /**
     * @brief 获取数据 (阻塞等待)
     * @return true 成功拿到数据, false 超时
     */
    bool Pop(T& outData, uint32_t waitMs = 0) {
        return xQueueReceive(m_queue, &outData, pdMS_TO_TICKS(waitMs)) == pdTRUE;
    }

    /**
     * @brief 获取最新数据 (非阻塞)
     * @note 适合控制循环，只想要最新的，不关心历史
     */
    bool PopLatest(T& outData) {
        // 简单的实现：读到空为止，保留最后一个
        bool hasData = false;
        while (xQueueReceive(m_queue, &outData, 0) == pdTRUE) {
            hasData = true;
        }
        return hasData; // 如果本来就是空的，返回 false
    }

  private:
    QueueHandle_t m_queue;
};



/*-------- 4. decorator ----------------------------------------------------------------------------------------------*/




/*-------- 5. factories ----------------------------------------------------------------------------------------------*/





#endif /*INFANTRY_CHASSIS_SUBSCRIBER_H*/
