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
  assert(ad5932_last_written_data == 0x0ED3);
  printf("PASS: %s (Sine)\n", __FUNCTION__);

  // 测试三角波
  mock_ad5932_driver_reset();
  ad5932_set_waveform(1);
  assert(ad5932_write_called == true);
  assert(ad5932_last_written_data == 0x0CD3);
  printf("PASS: %s (Triangle)\n", __FUNCTION__);

  // 测试方波
  mock_ad5932_driver_reset();
  ad5932_set_waveform(2);
  assert(ad5932_write_called == true);
  assert(ad5932_last_written_data == 0x09D3);
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


// 主测试运行器
int main(void) {
  printf("--- Running API Layer Tests ---\n");
  test_ad5932_reset();
  test_ad5932_set_waveform();
  test_ad5932_set_start_frequency();

  printf("\nAll API tests passed successfully!\n");
  return EXIT_SUCCESS;
}