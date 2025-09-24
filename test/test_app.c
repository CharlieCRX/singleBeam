#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "../app/wavegen_app.h" // 包含app层头文件，待实现
#include "mocks/mock_ad5932_api.h" // 包含api层测试桩

// ----------------------------------------
// Test Cases for ad5932_output_stable_waveform
// ----------------------------------------

void test_wavegen_output_stable_waveform_calls_api_with_correct_params() {
  // 1. 准备阶段：重置 mock 状态
  ad5932_mock_api_reset();

  // 定义测试参数
  int test_wave_type = 0; // 0 代表正弦波
  uint32_t test_frequency = 50000; // 50kHz

  // 2. 执行阶段：调用被测函数
  wavegen_output_fixed_wave(test_wave_type, test_frequency);

  // 3. 验证阶段：使用 assert 验证结果
  assert(mock_ad5932_set_waveform_called_times == 1);
  assert(mock_ad5932_set_waveform_param_wave_type == test_wave_type);

  assert(mock_ad5932_set_start_frequency_called_times == 1);
  assert(mock_ad5932_set_start_frequency_param_freq == test_frequency);
}

int main(void) {
  printf("Running app layer tests...\n");
  test_wavegen_output_stable_waveform_calls_api_with_correct_params();
  printf("All app layer tests passed!\n");
  return 0;
}