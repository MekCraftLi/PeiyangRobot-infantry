/**
 *******************************************************************************
 * @file    topic.h
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

#ifndef INFANTRY_CHASSIS_TOPIC_H
#define INFANTRY_CHASSIS_TOPIC_H



/*-------- 1. includes and imports -----------------------------------------------------------------------------------*/

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include <vector>


/*-------- 2. enum and define ----------------------------------------------------------------------------------------*/




/*-------- 3. interface ----------------------------------------------------------------------------------------------*/

template <typename T>
class Topic {
public:
    Topic() {
        // 创建一个互斥锁，保护订阅列表的操作安全
        m_mutex = xSemaphoreCreateMutex();
    }

    /**
     * @brief 注册订阅者
     * @param queue 订阅者的队列句柄
     */
    void Subscribe(QueueHandle_t queue) {
        if (m_mutex) {
            xSemaphoreTake(m_mutex, portMAX_DELAY);
            m_subscribers.push_back(queue);
            xSemaphoreGive(m_mutex);
        }
    }

    /**
     * @brief 发布数据 (广播)
     * @param data 要发送的数据引用
     */
    void Publish(const T& data) {
        if (m_mutex) {
            xSemaphoreTake(m_mutex, portMAX_DELAY);
            // 遍历所有订阅者，把数据塞给他们
            for (auto target_queue : m_subscribers) {
                // wait_ticks = 0: 如果对方满了，直接丢弃，不要阻塞发布者！
                // 也可以根据需求改为 xQueueOverwrite (仅针对长度为1的队列)
                xQueueSendToBack(target_queue, &data, 0);
            }
            xSemaphoreGive(m_mutex);
        }
    }

    void PublishFromISR(const T& data) {
        if (m_mutex) {
            BaseType_t higher;
            xSemaphoreTakeFromISR(m_mutex, &higher);
            for (auto target_queue : m_subscribers) {
                xQueueSendFromISR(target_queue, &data, 0);
            }
            xSemaphoreGiveFromISR(m_mutex, &higher);
        }
    }

private:
    std::vector<QueueHandle_t> m_subscribers; // 订阅者列表
    SemaphoreHandle_t m_mutex = nullptr;
};



/*-------- 4. decorator ----------------------------------------------------------------------------------------------*/




/*-------- 5. factories ----------------------------------------------------------------------------------------------*/





#endif /*INFANTRY_CHASSIS_TOPIC_H*/
