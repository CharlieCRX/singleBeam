#include "dac63001.h"
#include "../protocol/i2c_hal.h"
#include <unistd.h>
#include <stdio.h>

// Slew rate时间表 (单位: 微秒/步)
static const float slew_times[] = {
  0, 4, 8, 12, 18, 27.04, 40.48, 60.72, 91.12, 136.72, 239.2, 418.64, 732.56, 1282, 2563.96, 5127.92
};

// 代码步长表
static const uint16_t code_steps[] = {1, 2, 3, 4, 6, 8, 16, 32};

void dac63001_init(const char* i2c_bus) {
  i2c_hal_init(i2c_bus);
}

int dac63001_setup_external_ref(void) {
  int ret;
  uint16_t verify;

  printf("配置 DAC63001 外部参考电压模式\n");

  // 1. COMMON-CONFIG 配置
  uint16_t common_config = 0x0207; // 0000 0010 0000 0111
  ret = i2c_hal_write_reg16(DAC63001_I2C_ADDR, COMMON_CONFIG_REG, common_config);
  if (ret < 0) return ret;

  // 验证写入
  if (i2c_hal_read_reg16(DAC63001_I2C_ADDR, COMMON_CONFIG_REG, &verify) < 0) return -1;

  // 2. DAC-0-VOUT-CMP-CONFIG 配置
  uint16_t vout_config = 0x0000;
  ret = i2c_hal_write_reg16(DAC63001_I2C_ADDR, DAC_0_VOUT_CMP_CONFIG, vout_config);
  if (ret < 0) return ret;

  if (i2c_hal_read_reg16(DAC63001_I2C_ADDR, DAC_0_VOUT_CMP_CONFIG, &verify) < 0) return -1;

  usleep(50000); // 等待配置稳定
  return 0;
}

// 电压转DAC代码 (12位分辨率)
static uint16_t voltage_to_dac_code(float voltage, float vref) {
  if (voltage < 0) voltage = 0;
  if (voltage > vref) voltage = vref;
  
  uint32_t code = (uint32_t)((voltage / vref) * 4095.0f);
  return (code << 4); // 左对齐到16位寄存器
}

int dac63001_set_fixed_voltage(float voltage) {
  int ret;
  uint16_t verify;

  printf("设置固定电压输出: %.3fV\n", voltage);
  
  // 停止任何可能正在运行的波形生成
  ret = i2c_hal_write_reg16(DAC63001_I2C_ADDR, COMMON_DAC_TRIG, 0x0000);
  if (ret < 0) return ret;
  
  usleep(100000);

  // 计算DAC代码并设置
  uint16_t dac_code = voltage_to_dac_code(voltage, DAC63001_EXT_REF_VOLTAGE);
  ret = i2c_hal_write_reg16(DAC63001_I2C_ADDR, DAC_0_DATA_REG, dac_code);
  if (ret < 0) return ret;

  // 验证写入
  if (i2c_hal_read_reg16(DAC63001_I2C_ADDR, DAC_0_DATA_REG, &verify) < 0) return -1;

  return 0;
}

int dac63001_setup_sawtooth_wave(float min_voltage, float max_voltage, 
                dac63001_code_step_t code_step, 
                dac63001_slew_rate_t slew_rate) {
  int ret;
  
  printf("配置反向锯齿波生成\n");
  printf("电压范围: %.3fV 到 %.3fV\n", min_voltage, max_voltage);
  
  // 停止函数生成
  ret = i2c_hal_write_reg16(DAC63001_I2C_ADDR, COMMON_DAC_TRIG, 0x0000);
  if (ret < 0) return ret;
  
  usleep(100000);

  // 计算DAC代码
  uint16_t margin_high_code = voltage_to_dac_code(max_voltage, DAC63001_EXT_REF_VOLTAGE);
  uint16_t margin_low_code = voltage_to_dac_code(min_voltage, DAC63001_EXT_REF_VOLTAGE);
  printf("MARGIN_HIGH 代码: 0x%04X\n", margin_high_code);
  printf("MARGIN_LOW 代码: 0x%04X\n", margin_low_code);
  
  // 设置MARGIN_HIGH寄存器
  ret = i2c_hal_write_reg16(DAC63001_I2C_ADDR, DAC_0_MARGIN_HIGH, margin_high_code);
  if (ret < 0) return ret;
  
  // 设置MARGIN_LOW寄存器 
  ret = i2c_hal_write_reg16(DAC63001_I2C_ADDR, DAC_0_MARGIN_LOW, margin_low_code);
  if (ret < 0) return ret;
  
  // 配置DAC-0-FUNC-CONFIG寄存器
  // 位域说明:
  // bit15: CLR-SEL-0 = 0 (清零到零刻度)
  // bit14: SYNC-CONFIG-0 = 0 (立即更新)
  // bit13: BRD-CONFIG-0 = 0 (不响应广播)
  // bit12-11: PHASE-SEL-0 = 00 (0°相位)
  // bit10-8: FUNC-CONFIG-0 = 010 (反向锯齿波)
  // bit7: LOG-SLEW-EN-0 = 0 (线性slew)
  // bit6-4: CODE-STEP-0 = code_step (代码步长)
  // bit3-0: SLEW-RATE-0 = slew_rate (slew速率)
  uint16_t func_config = 0x0000;
  func_config |= (0x2 << 8);       // FUNC-CONFIG-0 = 010 (反向锯齿波)
  func_config |= (code_step << 4); // CODE-STEP-0
  func_config |= slew_rate;        // SLEW-RATE-0
  
  ret = i2c_hal_write_reg16(DAC63001_I2C_ADDR, DAC_0_FUNC_CONFIG, func_config);
  if (ret < 0) return ret;
  
  // 启动函数生成
  ret = i2c_hal_write_reg16(DAC63001_I2C_ADDR, COMMON_DAC_TRIG, 0x0001);
  if (ret < 0) return ret;
  
  // 计算预期频率
  uint16_t margin_high_value = margin_high_code >> 4;
  uint16_t margin_low_value = margin_low_code >> 4;
  uint16_t code_range = margin_high_value - margin_low_value + 1;
  
  // 从表7-6获取slew_rate时间 (单位: 微秒/步)
  float step_time = slew_times[slew_rate] / 1000000.0f; // 转换为秒
  
  // 代码步长从表7-5
  uint16_t actual_step = code_steps[code_step];
  
  float period = step_time * (code_range / actual_step);
  float frequency = 1.0f / period;
  
  printf("预期频率: %.2f Hz\n", frequency);
  printf("周期: %.6f 秒\n", period);
  
  usleep(100000); // 等待波形启动
  return 0;
}

int dac63001_stop_waveform(void) {
  printf("停止函数生成\n");
  return i2c_hal_write_reg16(DAC63001_I2C_ADDR, COMMON_DAC_TRIG, 0x0000);
}

void dac63001_close(void) {
  i2c_hal_close();
}