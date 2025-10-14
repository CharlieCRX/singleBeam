#include "ad5932_api.h"
#include "../driver/ad5932_driver.h" // 包含驱动层头文件，以便调用 ad5932_write
#include <stdint.h>
#include <unistd.h> // 包含usleep函数，用于必要的延时
#include <stdio.h>  // 包含标准输入输出函数，例如 FILE

// 计算频率字的乘数因子 (2^24)
#define FREQ_WORD_MULTIPLIER 16777216.0 // 2^24

#define DELTAF_WORD_MULTIPLIER 8388608.0 // 2^23


/**
 * @brief 软复位AD5932芯片。
 *
 * 此函数通过向AD5932的控制寄存器写入一个包含所有保留位（必须为1）
 * 的默认控制字，从而执行复位操作。
 * 这种写入行为会使芯片的内部状态机回到初始状态。
 *
 * @note 根据AD5932数据手册，复位后可能需要短暂的延时以确保芯片
 * 处于就绪状态，因此这里添加了一个10毫秒（10000微秒）的延时。
 */
void ad5932_reset(void) {
  ad5932_init(); // 初始化驱动层（确保SPI已初始化）

  // 构造复位命令: 控制寄存器地址 + 基础控制字 + 24位频率模式
  uint16_t reset_command = AD5932_REG_CONTROL | AD5932_CTRL_BASE | AD5932_CTRL_B24;
  
  // 调用驱动层的写入函数，将复位命令发送给芯片
  ad5932_write(reset_command);
  
  // 添加一个短暂的延时，以确保复位操作完成
  usleep(10000); // 10毫秒
}

/**
 * @brief 设置AD5932输出波形类型。
 *
 * 该函数根据 `wave_type` 参数来配置AD5932的控制寄存器。
 * 1. 初始化控制字，包含所有保留位、24位频率模式和DAC使能。
 *    并且默认 SYNCOUT 使能和频率递增结束后同步输出信号。
 * 2. 使用 switch 语句根据波形类型修改控制字。
 *    - 正弦波：启用 AD5932_CTRL_SINE，禁用 AD5932_CTRL_MSBOUT_EN。
 *    - 三角波：禁用 AD5932_CTRL_SINE 和 AD5932_CTRL_MSBOUT_EN。
 *    - 方波  ：禁用 AD5932_CTRL_SINE 和 AD5932_CTRL_DAC_EN，启用 AD5932_CTRL_MSBOUT_EN。
 * 3. 将最终的控制字通过 ad5932_write 函数写入芯片。
 *
 * @param wave_type 波形类型(0=正弦波, 1=三角波, 2=方波)
 */
void ad5932_set_waveform(int wave_type) {
  uint16_t control = AD5932_REG_CONTROL | AD5932_CTRL_BASE | AD5932_CTRL_B24 | AD5932_CTRL_DAC_EN 
    | AD5932_CTRL_SYNC_EN | AD5932_CTRL_SYNC_EOS;

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
  usleep(10000); // 10 ms 延时确保设置生效
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
    delta_high11 |= (1 << 11); // 设置D11为1，表示负增量
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
 * @param frequency_increments 递增次数，范围2~4095。
 */
void ad5932_set_number_of_increments(uint16_t frequency_increments) {
  // 确保递增次数在有效范围内
  if (frequency_increments < 2) {
    frequency_increments = 2;
  } else if (frequency_increments > 4095) {
    frequency_increments = 4095;
  }
  // 写入递增次数寄存器
  ad5932_write(AD5932_REG_NUM_INCR | (frequency_increments & 0x0FFF));
  usleep(10000);
}


/**
 * @brief 设置频率递增的时间间隔。
 *
 * 该函数根据输入的模式、乘数和间隔值，计算出递增间隔寄存器（TINC）的值
 * 并写入芯片。函数会确保:
 * `mode`      在0或1之间，
 * `mclk_mult` 在0到3之间，
 * `interval`  在2到2047之间。
 *
 * @param mode        递增间隔模式， 0 = 基于输出信号的周期数, 1 = 基于MCLK。
 * @param mclk_mult   当mode为1时的乘数选择，0=1x, 1=5x, 2=100x, 3=500x。(这里暂时不考虑 mode 是否为1)//TODO
 * @param interval    递增间隔值，范围0~2047。
 */
void ad5932_set_increment_interval(int mode, int mclk_mult, uint16_t interval) {
  uint16_t reg_value = AD5932_REG_INCR_INTVL;
  
  // 确保在有效范围内
  if (interval < 2) interval = 2;
  if (interval > 2047) interval = 2047;
  if (mode < 0) mode = 0;
  if (mode > 1) mode = 1;
  if (mclk_mult < 0) mclk_mult = 0;
  if (mclk_mult > 3) mclk_mult = 3;

  // 设置间隔值(低11位)
  reg_value |= (interval & 0x07FF);

  // 设置乘数(位11-12)
  reg_value |= ((mclk_mult & 0x03) << 11);

  // 设置模式(位13)
  reg_value |= ((mode & 0x01) << 13);

  ad5932_write(reg_value);
  usleep(10000);
}



// ========== 与引脚相关的操作 ==========
/**
 * - `0x10`：CTRL引脚
   - `0x11`：INTERUPT引脚。只能打断 DAC 输出的模拟波，不能打断 MSBOUT
   - `0x12`：STANDBY引脚。作为SYNCOUT - STANDBY 关联
   - `0x13`：输出引脚 SYNCOUT
 */
/**
 * @brief INTERRUPT 引脚：终止扫频，恢复初始电平
 */
void ad5932_interrupt(void){
  // todo
}

/**
 * @brief SYNCOUT 引脚：查询是否扫频结束
 * @return true表示扫频结束，false表示未结束
 * @note SYNCOUT 引脚在频率递增结束时会拉高，这时对应 STANDBY 引脚也会拉高
 *      通过 FPGA 读取 STANDBY 引脚状态来判断扫频是否完成
 */
bool ad5932_is_sweep_done(void) {
  FILE *fp;
  char buffer[64];
  int value = -1;

  // 打开命令管道，执行 fpga -r 0x12
  fp = popen("fpga -r 0x12", "r");
  if (fp == NULL) {
    perror("错误: 无法执行命令 fpga -r 0x12");
    return -1;
  }

  // 从命令输出中读取一行（已知输出形如 00000000 或 00000001）
  if (fgets(buffer, sizeof(buffer), fp) != NULL) {
    // 去掉可能的换行符
    buffer[strcspn(buffer, "\n")] = '\0';

    // 将读取的字符串转换为整数
    value = atoi(buffer);
  } else {
    perror("错误: 无法读取命令输出");
  }

  // 关闭命令管道
  pclose(fp);

  // 根据读取的值判断扫频是否完成
  if (value > 0) {
    return true;
  } else {
    return false;
  }
}

/**
 * @brief 通过 FPGA 控制 AD5932 CTRL 引脚触发扫频/输出
 * @note FPGA 寄存器地址 0x10 对应 CTRL 引脚
 */
void ad5932_start_sweep(void) {
  // 拉低 CTRL
  if (system("fpga -w 0x10 0") != 0) {
    perror("错误: 拉低 CTRL 失败");
  }

  // 确保 CTRL 有足够脉宽 (~1us-10us)
  usleep(10000); // 10ms 延迟

  // 拉高 CTRL
  if (system("fpga -w 0x10 1") != 0) {
    perror("错误: 拉高 CTRL 失败");
  }
}

/**
 * @brief STANDBY 引脚：暂停或恢复输出 + 也可配合 reset 进入低功耗模式
 */
void ad5932_set_standby(bool enable) {
  if (enable) {
    // 拉高 STANDBY
    if (system("fpga -w 0x12 1") != 0) {
      perror("错误: 拉高 STANDBY 失败");
    }
  } else {
    // 拉低 STANDBY
    if (system("fpga -w 0x12 0") != 0) {
      perror("错误: 拉低 STANDBY 失败");
    }
  }
}