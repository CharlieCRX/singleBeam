#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <math.h>
#include "../dev/dac63001.h"

static volatile bool keep_running = true;

void signal_handler(int sig) {
  printf("\n收到停止信号，正在停止...\n");
  keep_running = false;
}

void print_usage(const char* program_name) {
  printf("用法: %s <起始增益dB> <结束增益dB> <持续时间微秒>\n", program_name);
  printf("示例: %s 0 80 1000000  # 从0dB到80dB，持续1秒\n", program_name);
  printf("     %s 80 0 600000   # 从80dB到0dB，持续0.6秒\n", program_name);
  printf("     %s 20 40 2000000   # 从20dB到40dB，持续2秒\n", program_name);
  printf("     %s 30 30 1000000   # 固定30dB增益，持续1秒\n", program_name);
  printf("\n电阻配置: R_FEEDBACK=%.0fΩ, R_N=%.0fΩ\n", AD8338_R_FEEDBACK, AD8338_R_N);
  printf("注意: 最大增益80dB对应0.1V，最小增益0dB对应1.1V\n");
  printf("      持续时间必须大于600ms (600000微秒)\n");
  printf("      建议持续时间: 1-5秒 (1000000-5000000微秒)\n");
}

// 测试增益-电压转换
void test_gain_voltage_conversion() {
  printf("\n增益-电压转换测试 (R_F=%.0fΩ, R_N=%.0fΩ):\n", 
       AD8338_R_FEEDBACK, AD8338_R_N);
  printf("==========================================\n");
  
  float gain_offset = calculate_gain_offset();
  printf("计算得到的增益偏移: %.3f dB\n", gain_offset);
  
  printf("\n增益-电压对应表:\n");
  printf("增益(dB) | 电压(V) | 验证增益(dB)\n");
  printf("---------|---------|-------------\n");
  
  for (int gain = 0; gain <= 80; gain += 10) {
    float voltage = ad8338_gain_to_voltage(gain);
    uint16_t converted_gain = ad8338_voltage_to_gain(voltage);
    printf("   %2d  |  %.3f  |   %2d\n", gain, voltage, converted_gain);
  }
  
  // 验证数据手册中的例子
  printf("\n验证数据手册例子:\n");
  printf("V_GAIN=0.1V时:\n");
  float voltage = 0.1f;
  uint16_t gain = ad8338_voltage_to_gain(voltage);
  printf("  理论增益: 80dB, 计算增益: %ddB\n", gain);
  
  voltage = 1.1f;
  gain = ad8338_voltage_to_gain(voltage);
  printf("V_GAIN=1.1V时:\n");
  printf("  理论增益: 0dB, 计算增益: %ddB\n", gain);
}

int main(int argc, char* argv[]) {
  if (argc != 4) {
    print_usage(argv[0]);
    test_gain_voltage_conversion();
    return 1;
  }
  
  // 解析参数
  uint16_t start_gain = (uint16_t)atoi(argv[1]);
  uint16_t end_gain = (uint16_t)atoi(argv[2]);
  uint32_t duration_us = (uint32_t)atoi(argv[3]);
  
  // 参数验证
  if (start_gain > 80 || end_gain > 80) {
    fprintf(stderr, "错误: 增益值必须在 0-80 dB 范围内\n");
    return 1;
  }
  
  printf("AD8338 增益扫描测试 (使用具体电阻值)\n");
  printf("==================================\n");
  printf("电阻配置: R_FEEDBACK=%.0fΩ, R_N=%.0fΩ\n", AD8338_R_FEEDBACK, AD8338_R_N);
  printf("起始增益: %d dB\n", start_gain);
  printf("结束增益: %d dB\n", end_gain); 
  printf("持续时间: %u us (%.3f 秒)\n", duration_us, duration_us / 1000000.0f);
  
  // 显示增益-电压关系
  float start_voltage = ad8338_gain_to_voltage(start_gain);
  float end_voltage = ad8338_gain_to_voltage(end_gain);
  printf("对应电压: %.3fV -> %.3fV\n", start_voltage, end_voltage);
  
  // 设置信号处理
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  
  // 初始化DAC
  dac63001_init("/dev/i2c-2");
  
  // 配置外部参考模式
  if (dac63001_setup_external_ref() < 0) {
    fprintf(stderr, "错误: DAC配置失败\n");
    dac63001_close();
    return 1;
  }
  
  // 设置增益扫描
  if (start_gain == end_gain) {
    float voltage = ad8338_gain_to_voltage(start_gain);
    printf("起始和结束增益相同，设置固定电压 %.3fV\n", voltage);
    dac63001_set_fixed_voltage(voltage);
    printf("当前增益: %d dB (%.3fV)\n", start_gain, voltage);
    dac63001_close();
    return 1;
  }

  if (dac63001_set_gain_sweep(start_gain, end_gain, duration_us) < 0) {
    fprintf(stderr, "错误: 增益扫描设置失败\n");
    dac63001_close();
    return 1;
  }
  dac63001_start_waveform();
  usleep(5000); // 5 ms 延迟确保波形开始
  dac63001_stop_waveform();
  
  dac63001_close();
  return 0;
}