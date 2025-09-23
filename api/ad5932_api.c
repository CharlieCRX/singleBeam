#include "ad5932_api.h"
#include "../driver/ad5932_driver.h" // 包含驱动层头文件，以便调用 ad5932_write
#include <stdint.h>
#include <unistd.h> // 包含usleep函数，用于必要的延时

// AD5932 主时钟频率 (根据实际硬件配置)
#define MCLK_FREQUENCY 50000000.0 // 50 MHz

// 计算频率字的乘数因子 (2^24)
#define FREQ_WORD_MULTIPLIER 16777216.0 // 2^24

/**
 * @brief 软复位AD5932芯片。
 *
 * 此函数通过向AD5932的控制寄存器（AD5932_REG_CONTROL）写入一个
 * 包含所有保留位（必须为1）的默认控制字，从而执行复位操作。
 * 这种写入行为会使芯片的内部状态机回到初始状态。
 *
 * @note 根据AD5932数据手册，复位后可能需要短暂的延时以确保芯片
 * 处于就绪状态，因此这里添加了一个10毫秒（10000微秒）的延时。
 */
void ad5932_reset(void) {
  // 构造复位命令: 控制寄存器地址 + 基础控制字 + 24位频率模式
  uint16_t reset_command = AD5932_REG_CONTROL | AD5932_CTRL_BASE | AD5932_CTRL_B24;
  
  // 调用驱动层的写入函数，将复位命令发送给芯片
  ad5932_write(reset_command);
  
  // 添加一个短暂的延时，以确保复位操作完成
  usleep(10000); 
}

/**
 * @brief 设置AD5932输出波形类型。
 *
 * 该函数根据 `wave_type` 参数来配置AD5932的控制寄存器。
 * 1. 初始化控制字，包含所有保留位、24位频率模式和DAC使能。
 * 2. 使用 switch 语句根据波形类型修改控制字。
 * - 正弦波：启用 AD5932_CTRL_SINE，禁用 AD5932_CTRL_MSBOUT_EN。
 * - 三角波：禁用 AD5932_CTRL_SINE 和 AD5932_CTRL_MSBOUT_EN。
 * - 方波：禁用 AD5932_CTRL_SINE 和 AD5932_CTRL_DAC_EN，启用 AD5932_CTRL_MSBOUT_EN。
 * 3. 将最终的控制字通过 ad5932_write 函数写入芯片。
 *
 * @param wave_type 波形类型(0=正弦波, 1=三角波, 2=方波)
 */
void ad5932_set_waveform(int wave_type) {
  uint16_t control = AD5932_REG_CONTROL | AD5932_CTRL_BASE | AD5932_CTRL_B24 | AD5932_CTRL_DAC_EN;

  switch(wave_type) {
    case 0: // 正弦波
      control |= AD5932_CTRL_SINE;
      control &= ~AD5932_CTRL_MSBOUT_EN;
      break;
    case 1: // 三角波
      control &= ~AD5932_CTRL_SINE;
      control &= ~AD5932_CTRL_MSBOUT_EN;
      break;
    case 2: // 方波
      control &= ~AD5932_CTRL_SINE;
      control |= AD5932_CTRL_MSBOUT_EN;
      // 方波输出需要关闭DAC
      control &= ~AD5932_CTRL_DAC_EN;
      break;
    default:
      control |= AD5932_CTRL_SINE; // 默认正弦波
      break;
  }

  ad5932_write(control);
  usleep(10000); // 添加一个短暂的延时，以确保设置操作完成
}


/**
 * @brief 设置频率扫描的起始频率。
 *
 * 该函数根据输入的起始频率，计算出24位的频率字，并分两次写入到
 * AD5932的起始频率寄存器（FSTART_L, FSTART_H）。
 *
 * @param freq 起始频率，单位为Hz。
 */
void ad5932_set_start_frequency(uint32_t start_frequency_hz) {
  uint32_t freq_word;

  // 计算频率字: FREQ_WORD = (fout * 2^24) / MCLK
  freq_word = (uint32_t)((start_frequency_hz * FREQ_WORD_MULTIPLIER) / MCLK_FREQUENCY);
  
  // 写入低12位
  ad5932_write(AD5932_REG_FSTART_L | (freq_word & 0x0FFF));
  // 写入高12位
  ad5932_write(AD5932_REG_FSTART_H | ((freq_word >> 12) & 0x0FFF));

  usleep(10000);
}


/**
 * @brief 设置频率递增字和递增方向。
 *
 * 该函数根据输入的频率增量值和方向，计算出23位的频率递增字，并分两次
 * 写入到AD5932的频率递增寄存器（FDELTA_L, FDELTA_H）。如果 `positive` 为
 * false，则将频率递增字取反以实现负向递增。
 *
 * @param delta_freq 频率递增值，单位为Hz。
 * @param positive   递增方向，true表示正增量，false表示负增量。
 */
void ad5932_set_delta_frequency(uint32_t delta_freq, bool positive) {
  uint32_t delta_word;
  uint16_t delta_low12, delta_high11;

  // 计算频率递增字: DELTA_WORD = (delta_f * 2^24) / MCLK
  delta_word = (uint32_t)((delta_freq * FREQ_WORD_MULTIPLIER) / MCLK_FREQUENCY);

  delta_low12  = delta_word & 0x0FFF;         // 低12位
  delta_high11 = (delta_word >> 12) & 0x07FF; // 高11位

  // 如果是负增量，则D11的位置为1
  if (!positive) {
    delta_high11 |= 0x0800; // 设置D11为1，表示负增量
  }

  // 写入低12位
  ad5932_write(AD5932_REG_DELTA_FREQ_L | delta_low12);
  // 写入高11位
  ad5932_write(AD5932_REG_DELTA_FREQ_H | delta_high11);

  usleep(10000);
}


/**
 * @brief 设置频率递增的次数。
 *
 * 该函数将输入的递增次数写入到AD5932的递增次数寄存器（NINC）。
 * 递增次数的有效范围是2到4095，函数会确保输入值在此范围内。
 *
 * @param num_increments 递增次数，范围2~4095。
 */
void ad5932_set_number_of_increments(uint16_t num_increments) {
  // 确保递增次数在有效范围内
  if (num_increments < 2) {
    num_increments = 2;
  } else if (num_increments > 4095) {
    num_increments = 4095;
  }
  // 写入递增次数寄存器
  ad5932_write(AD5932_REG_NUM_INCR | (num_increments & 0x0FFF));
  usleep(10000);
}