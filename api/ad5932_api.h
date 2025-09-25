#ifndef AD5932_API_H
#define AD5932_API_H

#include <stdint.h>
#include <stdbool.h> // 包含stdbool.h以定义bool类型

// =============================================================================
// AD5932 寄存器地址定义
// =============================================================================

#define AD5932_REG_CONTROL        0x0000  // 控制寄存器
#define AD5932_REG_NUM_INCR       0x1000  // 频率递增次数寄存器
#define AD5932_REG_DELTA_FREQ_L   0x2000  // 频率递增字低 12 位寄存器
#define AD5932_REG_DELTA_FREQ_H   0x3000  // 频率递增字高 12 位寄存器
#define AD5932_REG_INCR_INTVL     0x4000  // 频率递增间隔寄存器
#define AD5932_REG_FSTART_L       0xC000  // 起始频率字低 12 位寄存器
#define AD5932_REG_FSTART_H       0xD000  // 起始频率字高 12 位寄存器

// =============================================================================
// AD5932 控制寄存器位定义
// =============================================================================
/* D11 : B24 模式选择（24 位频率字访问方式） */
#define AD5932_CTRL_B24        (1U << 11)  // 1 = 两次写入组成完整 24 位频率字
                                           // 0 = 高 12 位和低 12 位可独立更新

/* D10 : DAC 使能 */
#define AD5932_CTRL_DAC_EN     (1U << 10)  // 1 = 启用 DAC，VOUT 输出正弦/三角波
                                           // 0 = 关闭 DAC（省电，仅可用 MSBOUT 引脚）

/* D9 : 输出波形选择 */
#define AD5932_CTRL_SINE       (1U << 9)   // 1 = 正弦波输出（通过 SIN ROM）
#define AD5932_CTRL_TRIANGLE   (0U << 9)   // 0 = 三角波输出（绕过 SIN ROM）

/* D8 : MSBOUT 输出使能 */
#define AD5932_CTRL_MSBOUT_EN  (1U << 8)   // 1 = 使能 MSBOUT 输出（方波）
                                           // 0 = 禁用（高阻态）

/* D7, D6 : 保留位 → 必须写 1 */
#define AD5932_CTRL_RSVD7      (1U << 7)   // 必须为 1
#define AD5932_CTRL_RSVD6      (1U << 6)   // 必须为 1

/* D5 : 频率递增方式选择 */
#define AD5932_CTRL_EXT_INCR   (1U << 5)   // 1 = 外部触发递增（CTRL 引脚）
                                           // 0 = 内部自动递增

/* D4 : 保留位 → 必须写 1 */
#define AD5932_CTRL_RSVD4      (1U << 4)   // 必须为 1

/* D3 : SYNCOUT 输出模式 */
#define AD5932_CTRL_SYNC_EOS   (1U << 3)   // 1 = SYNCOUT 在扫描结束时拉高
                                           // 0 = 每次递增输出 4×TCLOCK 脉冲

/* D2 : SYNCOUT 输出使能 */
#define AD5932_CTRL_SYNC_EN    (1U << 2)   // 1 = 使能 SYNCOUT 引脚
                                           // 0 = 禁用（高阻态）

/* D1, D0 : 保留位 → 必须写 1 */
#define AD5932_CTRL_RSVD1      (1U << 1)   // 必须为 1
#define AD5932_CTRL_RSVD0      (1U << 0)   // 必须为 1

/* 默认控制字（仅包含保留位），用户在此基础上按位或添加功能 */
#define AD5932_CTRL_BASE   (AD5932_CTRL_RSVD7 | AD5932_CTRL_RSVD6 | \
              AD5932_CTRL_RSVD4 | AD5932_CTRL_RSVD1 | AD5932_CTRL_RSVD0)

/**
 * @brief 软复位AD5932芯片。
 *
 * 通过向控制寄存器写入默认值，来复位AD5932内部的状态机和寄存器。
 * 这是一个基本的、必须首先调用的API函数，以确保芯片处于已知状态。
 */
void ad5932_reset(void);

/**
 * @brief 设置AD5932输出波形类型。
 *
 * 该函数根据输入的波形类型，向控制寄存器写入相应的控制字。
 * 它处理了正弦波、三角波和方波三种模式，并相应地启用或禁用DAC和MSBOUT。
 *
 * @param wave_type 波形类型(0=正弦波, 1=三角波, 2=方波)
 */
void ad5932_set_waveform(int wave_type);


/**
 * @brief 设置频率扫描的起始频率。
 *
 * @param freq 起始频率，单位为Hz。
 */
void ad5932_set_start_frequency(uint32_t start_frequency_hz);


/**
 * @brief 设置频率递增字和递增方向。
 *
 * @param delta_freq 频率递增值，单位为Hz。
 * @param positive   递增方向，true表示正增量，false表示负增量。
 */
void ad5932_set_delta_frequency(uint32_t delta_freq, bool positive);


/**
 * @brief 设置频率递增的次数。
 *
 * @param frequency_increments 递增次数，范围2~4095。
 */
void ad5932_set_number_of_increments(uint16_t frequency_increments);

/**
 * @brief 设置频率递增的时间间隔。
 *
 * @param mode        递增间隔模式，0表示基于输出周期，1表示基于MCLK(50MHz)。
 * @param mclk_mult   MCLK倍频选择，仅在mode=1时有效。0=1x, 1=5x, 2=100x, 3=500x。
 * @param interval    递增间隔值，范围0~2047。
 */
void ad5932_set_increment_interval(int mode, int mclk_mult, uint16_t interval);


// ========== 与引脚相关的操作 ==========

/**
 * @brief INTERRUPT 引脚：终止扫频，恢复初始电平
 */
void ad5932_interrupt(void);

/**
 * @brief SYNCOUT 引脚：查询是否扫频结束
 */
bool ad5932_is_sweep_done(void);

/**
 * @brief CTRL 引脚：触发频率递增
 */
void ad5932_start_sweep(void);

/**
 * @brief STANDBY 引脚：暂停或恢复输出 + 配合 reset 进入低功耗模式
 */
void ad5932_set_standby(bool enable);

#endif // AD5932_API_H