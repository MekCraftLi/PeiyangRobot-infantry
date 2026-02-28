cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME infantry_gimbal)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

include(cmake/gcc-arm-none-eabi.cmake)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T \"${CMAKE_CURRENT_LIST_DIR}/STM32H723XG_FLASH.ld\"")

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
        "${CMAKE_CURRENT_LIST_DIR}/STM32H723XG_FLASH.ld"
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here

        "../Solution/app-main.cpp"

        "../Solution/Application/movtion-ctrl-app.cpp"

        "../Solution/Board-Support-Pack/BMI088/dev-bmi.cpp"
        "../Solution/Board-Support-Pack/USB/usb-descriptors.c"
        "../Solution/Board-Support-Pack/DR16/dr16.cpp"


        "../Solution/System/Service/commander.cpp"
        "../Solution/System/Service/app-imu.cpp"


        "../Solution/System/Service/heart-beat.cpp"



        # Third part sources

        # PYRo
        "../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Peripheral/CAN/pyro_can_drv.cpp"

        "../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Component/Motor/pyro_dji_motor_drv.cpp"
        "../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Component/Motor/pyro_dm_motor_drv.cpp"
        "../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Component/Motor/pyro_motor_base.cpp"
        "../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Peripheral/CAN/pyro_can_drv.cpp"
        "../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Algorithm/PID/pyro_algo_pid.cpp"
        "../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Peripheral/DWT/pyro_dwt_drv.cpp"
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
        "${CMAKE_CURRENT_LIST_DIR}/../Solution"


        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/TinyUSB/src"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo"

        # --- Core ---
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Core/Config"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Core/Def"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Core/ETL"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Core/FSM"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Core/Lock"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Core/Memory"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Core/Task"

        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Peripheral/CAN"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Peripheral/DWT"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Peripheral/UART"

        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Device/BMI088"

        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Algorithm/Common"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Algorithm/Kinematics"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Algorithm/OLS"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Algorithm/PID"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Algorithm/EKF"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Algorithm/KF"

        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Component/CRC"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Component/IMU"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Component/INS"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Component/Motor"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Component/Powercontrol"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Component/Powermeter"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Component/RC"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Component/Referee"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Component/Shoot"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Component/Supercap"

        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Module"

        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Communication"

        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Debug/VOFA"
        "${CMAKE_CURRENT_LIST_DIR}/../Solution/ThirdParty/PYRo-uCtrl-Unity/PYRo/Debug/JCOM"
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
        GIMBAL
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
