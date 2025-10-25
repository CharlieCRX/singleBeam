#include "dac63001.h"
#include "../protocol/i2c_hal.h"
#include <unistd.h>
#include <stdio.h>
#include <math.h>

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

  printf("配置 DAC63001 外部参考电压模式。外部参考电压为 %f V\n", DAC63001_EXT_REF_VOLTAGE);

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

// 适用于 DAC-X-DATA 寄存器：电压转DAC代码 (12位分辨率), 左对齐到16位寄存器
static uint16_t voltage_to_dac_code(float voltage, float vref) {
  if (voltage < 0) voltage = 0;
  if (voltage > vref) voltage = vref;
  
  // 方程（3） DAC代码 = (Vout / Vref) * 2 ^ N （DAC63001：N = 12）
  uint32_t code = (uint32_t)((voltage / vref) * 4096.0f);

  // DAC-X-DATA：12位DAC：12位数据存放在 BIT15-BIT4，BIT3-BIT0是无关位
  return (code << 4); // 左对齐到16位寄存器
}

int dac63001_set_fixed_voltage(float voltage) {
  int ret;
  uint16_t verify;

  printf("设置固定电压输出: %.3fV\n", voltage);
  
  // 停止任何可能正在运行的波形生成
  ret = dac63001_stop_waveform();
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
  ret = dac63001_stop_waveform();
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

  return 0;
}

int dac63001_start_waveform(void) {
  printf("开始函数生成\n");
  return i2c_hal_write_reg16(DAC63001_I2C_ADDR, COMMON_DAC_TRIG, 0x0001);
}

int dac63001_stop_waveform(void) {
  printf("停止函数生成\n");
  return i2c_hal_write_reg16(DAC63001_I2C_ADDR, COMMON_DAC_TRIG, 0x0000);
}

void dac63001_close(void) {
  i2c_hal_close();
}



// 计算电阻比值对应的增益偏移
float calculate_gain_offset(void) {
  float resistance_ratio = AD8338_R_FEEDBACK / AD8338_R_N;
  float log_term = 20.0f * log10f(resistance_ratio);
  return log_term - 26.0f;
}

// 增益到电压的转换（使用具体电阻值）
// 增益到电压的转换（反比例关系）
float ad8338_gain_to_voltage(uint16_t gain_dB) {
  if (gain_dB > AD8338_GAIN_MAX_DB) {
      gain_dB = AD8338_GAIN_MAX_DB;
  }
  
  // 计算增益偏移
  float gain_offset = calculate_gain_offset();
  
  // 反比例关系：增益越大，电压越小
  // 使用公式：V_GAIN = 1.1 - (Gain - gain_offset) / 80 * (1.1 - 0.1)
  float voltage_range = AD8338_VGAIN_MIN_V - AD8338_VGAIN_MAX_V; // 1.1 - 0.1 = 1.0
  float voltage = AD8338_VGAIN_MIN_V - ((float)gain_dB - gain_offset) / 80.0f * voltage_range;
  
  // 确保电压在有效范围内
  if (voltage < AD8338_VGAIN_MAX_V) {  // 最小电压是0.1V
      voltage = AD8338_VGAIN_MAX_V;
  }
  if (voltage > AD8338_VGAIN_MIN_V) {  // 最大电压是1.1V
      voltage = AD8338_VGAIN_MIN_V;
  }
  
  return voltage;
}

// 电压到增益的转换（使用具体电阻值）
// 电压到增益的转换（反比例关系）
uint16_t ad8338_voltage_to_gain(float voltage) {
  // 确保电压在有效范围内
  if (voltage < AD8338_VGAIN_MAX_V) {
      voltage = AD8338_VGAIN_MAX_V;
  }
  if (voltage > AD8338_VGAIN_MIN_V) {
      voltage = AD8338_VGAIN_MIN_V;
  }
  
  // 计算增益偏移
  float gain_offset = calculate_gain_offset();
  
  // 反比例关系：电压越小，增益越大
  // 使用公式：Gain = (1.1 - V_GAIN) / (1.1 - 0.1) * 80 + gain_offset
  float voltage_range = AD8338_VGAIN_MIN_V - AD8338_VGAIN_MAX_V; // 1.1 - 0.1 = 1.0
  float gain = (AD8338_VGAIN_MIN_V - voltage) / voltage_range * 80.0f + gain_offset;
  
  if (gain < 0) gain = 0;
  if (gain > AD8338_GAIN_MAX_DB) gain = AD8338_GAIN_MAX_DB;
  
  return (uint16_t)gain;
}

// 根据持续时间计算最佳参数
void calculate_sweep_parameters(uint32_t duration_us, float voltage_range, 
                 dac63001_code_step_t* code_step, 
                 dac63001_slew_rate_t* slew_rate) {
  // 计算需要的总步数 (12位DAC，4096个代码点)
  uint16_t effective_codes = voltage_to_dac_code(voltage_range, DAC63001_EXT_REF_VOLTAGE) >> 4;
  
  if (effective_codes < 1) effective_codes = 1;
  
  // 计算理想状态下每步需要的时间 (微秒)：每步只改变1个LSB，需要的时间
  float expected_step_time_us = (float)duration_us / effective_codes;
  
  // 选择最接近的SLEW_RATE (优先选择较小的时间)
  *slew_rate = DAC63001_SLEW_4US; // 默认4us
  // 选择不超过理想步进时间的最大SLEW_RATE
  for (int i = 1; i < 16; i++) {
    if (slew_times[i] <= expected_step_time_us) {
      *slew_rate = (dac63001_slew_rate_t)i;
    } else {
      break;
    }
  }
  
  // 根据slew rate和持续时间选择code step
  float actual_step_time = slew_times[*slew_rate];
  float total_steps = (float)duration_us / actual_step_time;
  
  // 计算需要的code step大小
  uint16_t required_step = (uint16_t)(effective_codes / total_steps) + 1;
  
  *code_step = DAC63001_STEP_32LSB; // 默认最大步长
  
  // 找到合适的code step
  for (int i = 0; i < 8; i++) {
    if (code_steps[i] >= required_step) {
      *code_step = (dac63001_code_step_t)i;
      break;
    }
  }
  
  printf("扫描参数计算: 电压范围=%.3fV, 有效代码=%d, 每步时间=%.2fus\n", 
       voltage_range, effective_codes, expected_step_time_us);
  printf("选择参数: slew_rate=%d(%.2fus/步), code_step=%d(%dLSB/步)\n", 
       *slew_rate, slew_times[*slew_rate], *code_step, code_steps[*code_step]);
}

// 设置增益扫描（使用具体电阻值）
int dac63001_set_gain_sweep(uint16_t start_gain, uint16_t end_gain, uint32_t gain_duration_us) {
  if (start_gain == end_gain) {
    fprintf(stderr, "错误: 起始增益和结束增益不能相同\n");
    return -1;
  }

  int ret;
  
  printf("设置增益扫描: %ddB -> %ddB, 持续时间: %uus\n", 
       start_gain, end_gain, gain_duration_us);
  
  // 显示电阻配置
  float gain_offset = calculate_gain_offset();
  printf("电阻配置: R_FEEDBACK=%.0fΩ, R_N=%.0fΩ, 增益偏移=%.3fdB\n", 
       AD8338_R_FEEDBACK, AD8338_R_N, gain_offset);
  
  // 参数检查
  if (start_gain > AD8338_GAIN_MAX_DB || end_gain > AD8338_GAIN_MAX_DB) {
    fprintf(stderr, "错误: 增益值必须在 0-%d dB 范围内\n", AD8338_GAIN_MAX_DB);
    return -1;
  }
  
  if (gain_duration_us > 5000000) { // 5秒限制
    fprintf(stderr, "错误: 持续时间不能超过 5秒\n");
    return -1;
  }
  
  // 转换为电压（使用具体电阻值）
  float start_voltage = ad8338_gain_to_voltage(start_gain);
  float end_voltage = ad8338_gain_to_voltage(end_gain);
  
  printf("对应电压: %.3fV -> %.3fV\n", start_voltage, end_voltage);
  printf("验证: %.3fV = %ddB, %.3fV = %ddB\n", 
       start_voltage, ad8338_voltage_to_gain(start_voltage),
       end_voltage, ad8338_voltage_to_gain(end_voltage));
  
  // 计算扫描参数
  float voltage_range = fabsf(end_voltage - start_voltage);
  dac63001_code_step_t code_step;
  dac63001_slew_rate_t slew_rate;
  
  calculate_sweep_parameters(gain_duration_us, voltage_range, &code_step, &slew_rate);
  
  // 设置锯齿波
  // 注意: 反向锯齿波是从高电压到低电压
  // 由于最大增益对应最小电压(0.1V)，最小增益对应最大电压(1.1V)
  // 所以增益增加对应电压减小，增益减少对应电压增加
  
  float min_voltage, max_voltage;
  if (start_gain < end_gain) {
    // 增益增加：电压从高到低
    max_voltage = start_voltage;  // 起始电压（高电压，低增益）
    min_voltage = end_voltage;  // 结束电压（低电压，高增益）
    printf("增益增加模式: 电压从 %.3fV -> %.3fV\n", max_voltage, min_voltage);
  } else {
    // 增益减少：电压从低到高
    min_voltage = start_voltage;  // 起始电压（低电压，高增益）
    max_voltage = end_voltage;  // 结束电压（高电压，低增益）
    printf("增益减少模式: 电压从 %.3fV -> %.3fV\n", min_voltage, max_voltage);
  }
  
  // 配置锯齿波（反向锯齿波从MARGIN_HIGH到MARGIN_LOW）
  ret = dac63001_setup_sawtooth_wave(min_voltage, max_voltage, code_step, slew_rate);
  if (ret < 0) {
    fprintf(stderr, "错误: 锯齿波配置失败\n");
    return ret;
  }
  
  // 计算实际频率和周期
  uint16_t min_code = voltage_to_dac_code(min_voltage, DAC63001_EXT_REF_VOLTAGE);
  uint16_t max_code = voltage_to_dac_code(max_voltage, DAC63001_EXT_REF_VOLTAGE);
  uint16_t code_range = abs(max_code - min_code) + 1;
  
  float step_time = slew_times[slew_rate] / 1000000.0f;
  uint16_t actual_step = code_steps[code_step];
  
  float period = step_time * (code_range / actual_step);
  float frequency = 1.0f / period;
  
  printf("实际扫描参数: 频率=%.2f Hz, 周期=%.6f 秒\n", frequency, period);
  printf("单次扫描时间: %.3f 秒\n", period);
  printf("注意: 最大增益(80dB)对应0.1V，最小增益(0dB)对应1.1V\n");
  
  return 0;
}