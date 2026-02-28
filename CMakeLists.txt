cmake_minimum_required(VERSION 3.22)


# 【核心修复】：强行设置目标操作系统为裸机 (Generic) 和架构为 ARM
# 注意：这两句话必须、一定、绝对要放在 project() 命令之前！
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)


project(Infantry NONE)

# 1. 定义一个下拉选项，让你在 IDE 中选择要编译的目标
set(BUILD_TARGET "CHASSIS" CACHE STRING "Select target to build: CHASSIS, GIMBAL, or BOTH")
# 限制下拉菜单的选项值
set_property(CACHE BUILD_TARGET PROPERTY STRINGS "CHASSIS" "GIMBAL" "BOTH")




message(STATUS "============= Current Build Target: ${BUILD_TARGET} =============")

# 2. 根据选择，按需添加子目录
if(BUILD_TARGET STREQUAL "CHASSIS" OR BUILD_TARGET STREQUAL "BOTH")
    message("编译底盘中")
    add_subdirectory(Infantry-Chassis)

endif()

if(BUILD_TARGET STREQUAL "GIMBAL" OR BUILD_TARGET STREQUAL "BOTH")
    message("编译云台中")
    add_subdirectory(Infantry-Gimbal)
endif()

