/**
 *******************************************************************************
 * @file    crtp.h
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
 * @date    2026/2/27
 * @version 1.0
 *******************************************************************************
 */


/* Define to prevent recursive inclusion -----------------------------------------------------------------------------*/

#ifndef INFANTRY_CHASSIS_CRTP_H
#define INFANTRY_CHASSIS_CRTP_H



/*-------- 1. includes and imports -----------------------------------------------------------------------------------*/

#include <cstddef>

/*-------- 2. enum and define ----------------------------------------------------------------------------------------*/




/*-------- 3. interface ----------------------------------------------------------------------------------------------*/


template <typename T>
class Singleton {
public:
    static T& instance() {
        static T _instance;
        return _instance;
    }

    /**
     * @brief 调试辅助：返回当前单例的类型签名
     * @note  GCC __PRETTY_FUNCTION__ 在模板实例化时会展开为包含 T 名称的完整签名
     *        例: "const char* Singleton<MovtionCtrlApp>::debugName()"
     *        Ozone 中: Eval Singleton<MovtionCtrlApp>::instance().debugName()
     *        GDB  中:  p Singleton<MovtionCtrlApp>::instance().debugName()
     */
    __attribute__((noinline))
    const char* debugName() const { return __PRETTY_FUNCTION__; }

protected:
    Singleton() = default;
    ~Singleton() = default;

private:
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
};



/*-------- 4. decorator ----------------------------------------------------------------------------------------------*/




/*-------- 5. factories ----------------------------------------------------------------------------------------------*/



#endif /*INFANTRY_CHASSIS_CRTP_H*/
