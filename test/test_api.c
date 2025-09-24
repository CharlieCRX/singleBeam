#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

// 引入要测试的 API 层头文件
#include "../api/ad5932_api.h"

// 引入模拟实现的头文件，这里包含了 ad5932_write 的 Mock 声明和全局变量
#include "mocks/mock_ad5932_driver.h"

// =========================================================================
// API 层测试用例
// =========================================================================

void test_ad5932_reset(void) {
  mock_ad5932_driver_reset(); // 重置模拟驱动状态

  // 调用待测试的 API 函数
  ad5932_reset();

  // 验证 ad5932_write 函数是否被调用了
  assert(ad5932_write_called == true);

  // 验证 ad5932_write 函数接收到的数据是否正确
  uint16_t expected_data = AD5932_REG_CONTROL | AD5932_CTRL_BASE | AD5932_CTRL_B24;
  assert(ad5932_last_written_data == expected_data);

  printf("PASS: %s\n", __FUNCTION__);
}

void test_ad5932_set_waveform(void) {
  // 测试正弦波
  mock_ad5932_driver_reset();
  ad5932_set_waveform(0);
  assert(ad5932_write_called == true);
  assert(ad5932_last_written_data == 0x0EDF);
  printf("PASS: %s (Sine)\n", __FUNCTION__);

  // 测试三角波
  mock_ad5932_driver_reset();
  ad5932_set_waveform(1);
  assert(ad5932_write_called == true);
  assert(ad5932_last_written_data == 0x0CDF);
  printf("PASS: %s (Triangle)\n", __FUNCTION__);

  // 测试方波
  mock_ad5932_driver_reset();
  ad5932_set_waveform(2);
  assert(ad5932_write_called == true);
  assert(ad5932_last_written_data == 0x09DF);
  printf("PASS: %s (Square)\n", __FUNCTION__);
}

void test_ad5932_set_start_frequency(void) {
  mock_ad5932_driver_reset(); // 重置模拟驱动状态
  uint32_t start_frequency_hz = 10000; // 10kHz
  ad5932_set_start_frequency(start_frequency_hz);

  // 验证总共进行了2次写入操作
  assert(ad5932_write_call_count == 2);

  // 计算频率字: FS_DATA = (F_OUT * 2^24) / MCLK(50MHz)
  uint32_t freq_word = 3355; // 10kHz的频率字
  
  // 验证第一次写入：地址0x01，数据为频率字的低12位
  assert(ad5932_write_call_history[0] == (0xC000 | (freq_word & 0x0FFF)));
  // 验证第二次写入：地址0x02，数据为频率字的高12位
  assert(ad5932_write_call_history[1] == (0xD000 | ((freq_word >> 12) & 0x0FFF)));

  printf("PASS: %s\n", __FUNCTION__);
}


void test_ad5932_set_delta_frequency(void) {
  uint32_t delta_freq_word;
  uint16_t expected_write_LSBs, expected_write_MSBs;

  // --- 测试正增量 ---
  mock_ad5932_driver_reset();
  double delta_freq = 500; // 500Hz
  ad5932_set_delta_frequency(delta_freq, true); // true表示正增量

  // 计算频率字: delta_freq_word = (delta_freq * 2^24) / MCLK(50MHz)
  delta_freq_word = 167; // 500Hz的频率字

  // 第一次写入: 0b0010 + 低12位
  expected_write_LSBs = 0x2000 | (delta_freq_word & 0x0FFF);

  // 第二次写入: 0b0011 + 0 + 高11位
  expected_write_MSBs = 0x3000 | ((delta_freq_word >> 12) & 0x07FF);

  assert(ad5932_write_call_count == 2);
  assert(ad5932_write_call_history[0] == expected_write_LSBs);
  assert(ad5932_write_call_history[1] == expected_write_MSBs);

  // --- 测试负增量 ---
  mock_ad5932_driver_reset();
  delta_freq = 600; // 500Hz
  ad5932_set_delta_frequency(delta_freq, false); // false表示负增量

  // 计算频率字: delta_freq_word = (delta_freq * 2^24) / MCLK(50MHz)
  delta_freq_word = 201; // 600Hz的频率字

  // 第一次写入: 0b0010 + 低12位
  expected_write_LSBs = 0x2000 | (delta_freq_word & 0x0FFF);

  // 第二次写入: 0b0011 + 1 + 高11位
  expected_write_MSBs = 0x3000 | 0x0800 | ((delta_freq_word >> 12) & 0x07FF);

  assert(ad5932_write_call_count == 2);
  assert(ad5932_write_call_history[0] == expected_write_LSBs);
  assert(ad5932_write_call_history[1] == expected_write_MSBs);
}


void test_ad5932_set_number_of_increments(void) {
  // 测试一个中间值
  mock_ad5932_driver_reset();
  uint16_t num_increments = 1000;
  ad5932_set_number_of_increments(num_increments);
  assert(ad5932_write_call_count == 1);
  assert(ad5932_write_call_history[0] == (0x1000 | num_increments));
  printf("PASS: %s (Mid-range)\n", __FUNCTION__);

  // 测试最小值
  mock_ad5932_driver_reset();
  num_increments = 2;
  ad5932_set_number_of_increments(num_increments);
  assert(ad5932_write_call_count == 1);
  assert(ad5932_write_call_history[0] == (0x1000 | num_increments));
  printf("PASS: %s (Min-range)\n", __FUNCTION__);

  // 测试最大值
  mock_ad5932_driver_reset();
  num_increments = 4095;
  ad5932_set_number_of_increments(num_increments);
  assert(ad5932_write_call_count == 1);
  assert(ad5932_write_call_history[0] == (0x1000 | num_increments));
  printf("PASS: %s (Max-range)\n", __FUNCTION__);

  // 测试超出范围的值，应该被限制在最大值4095
  mock_ad5932_driver_reset();
  num_increments = 5000; // 超出范围
  ad5932_set_number_of_increments(num_increments);
  assert(ad5932_write_call_count == 1);
  assert(ad5932_write_call_history[0] == (0x1000 | 4095));
  printf("PASS: %s Max (Out-of-range)\n", __FUNCTION__);

  // 测试超出范围的值，应该被限制在最小值2
  mock_ad5932_driver_reset();
  num_increments = 1; // 超出范围
  ad5932_set_number_of_increments(num_increments);
  assert(ad5932_write_call_count == 1);
  assert(ad5932_write_call_history[0] == (0x1000 | 2));
  printf("PASS: %s Min (Out-of-range)\n", __FUNCTION__);
}


void test_ad5932_set_increment_interval(void) {
  uint16_t expected_data;

  // --- 场景1: 基于输出周期，interval=1000 ---
  mock_ad5932_driver_reset();
  ad5932_set_increment_interval(0, 0, 1000);
  expected_data = 0x4000 | 1000;
  assert(ad5932_write_call_count == 1);
  assert(ad5932_write_call_history[0] == expected_data);
  printf("PASS: %s (Output Cycles Mode)\n", __FUNCTION__);

  // --- 场景2.1: 基于基于MCLK，multiplier=1 (5x)，interval=500 ---
  mock_ad5932_driver_reset();
  ad5932_set_increment_interval(1, 1, 500);
  expected_data = 0x4000 | (1 << 13) | (1 << 11) | 500;
  assert(ad5932_write_call_count == 1);
  assert(ad5932_write_call_history[0] == expected_data);
  printf("PASS: %s (MCLK Mode, Multiplier=1)\n", __FUNCTION__);

  // --- 场景2.2: 基于基于MCLK，multiplier=3 (500x)，interval=2000 ---
  mock_ad5932_driver_reset();
  ad5932_set_increment_interval(1, 3, 2000);
  expected_data = 0x4000 | (1 << 13) | (1 << 12) | (1 << 11) | 2000;
  assert(ad5932_write_call_count == 1);
  assert(ad5932_write_call_history[0] == expected_data);
  printf("PASS: %s (MCLK Mode, Multiplier=3)\n", __FUNCTION__);

  // --- 场景3: interval超出范围，应该被限制在2047 ---
  mock_ad5932_driver_reset();
  ad5932_set_increment_interval(0, 0, 3000); // interval超出范围
  expected_data = 0x4000 | 2047; // 应该被限制在2047
  assert(ad5932_write_call_count == 1);
  assert(ad5932_write_call_history[0] == expected_data);  
  printf("PASS: %s (Interval Out-of-range)\n", __FUNCTION__);

  // --- 场景4.1: mode无效，应该被限制在1 ---
  mock_ad5932_driver_reset();
  ad5932_set_increment_interval(5, 0, 1000); // mode无效
  expected_data = 0x4000 | (1 << 13) | 1000; // 超过1的部分被限制为1
  assert(ad5932_write_call_count == 1);
  assert(ad5932_write_call_history[0] == expected_data);  
  printf("PASS: %s (Mode Out-of-range)\n", __FUNCTION__);

  // --- 场景4.2: mode无效，应该被限制在0 ---
  mock_ad5932_driver_reset();
  ad5932_set_increment_interval(-3, 0, 1000); // mode
  expected_data = 0x4000 | 1000; // 低于0的部分被限制为0
  assert(ad5932_write_call_count == 1);
  assert(ad5932_write_call_history[0] == expected_data);
  printf("PASS: %s (Mode Out-of-range)\n", __FUNCTION__);

  // --- 场景5.1: mclk_mult无效，应该被限制在3 ---
  mock_ad5932_driver_reset();
  ad5932_set_increment_interval(1, 5, 1000); // mclk_mult无效
  expected_data = 0x4000 | (1 << 13) | (3 << 11) | 1000; // 超出则限制为3
  assert(ad5932_write_call_count == 1);
  assert(ad5932_write_call_history[0] == expected_data);  
  printf("PASS: %s (MCLK Multiplier Out-of-range)\n", __FUNCTION__);

  // --- 场景5.2: mclk_mult无效，应该被限制在0 ---
  mock_ad5932_driver_reset();
  ad5932_set_increment_interval(1, -2, 1000); // mclk_mult无效
  expected_data = 0x4000 | (1 << 13) | 1000; // 低于0的部分被限制为0
  assert(ad5932_write_call_count == 1);
  assert(ad5932_write_call_history[0] == expected_data);  
  printf("PASS: %s (MCLK Multiplier Out-of-range)\n", __FUNCTION__);
}


// 主测试运行器
int main(void) {
  printf("--- Running API Layer Tests ---\n");
  test_ad5932_reset();
  test_ad5932_set_waveform();
  test_ad5932_set_start_frequency();
  test_ad5932_set_delta_frequency();
  test_ad5932_set_number_of_increments();
  test_ad5932_set_increment_interval();

  printf("\nAll API tests passed successfully!\n");
  return EXIT_SUCCESS;
}