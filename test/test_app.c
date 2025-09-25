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
  // 验证 ad5932_set_standby 是否被调用了一次，且参数为 false
  assert(mock_ad5932_set_standby_called_times == 1);
  assert(mock_ad5932_set_standby_param_enable == false);
  
  // 验证 ad5932_reset 是否被调用了一次
  assert(mock_ad5932_reset_called_times == 1);

  // 验证 ad5932_set_waveform 是否被调用了一次
  assert(mock_ad5932_set_waveform_called_times == 1);
  // 验证 ad5932_set_waveform 传入的参数是否正确
  assert(mock_ad5932_set_waveform_param_wave_type == test_wave_type);

  // 验证 ad5932_set_start_frequency 是否被调用了一次
  assert(mock_ad5932_set_start_frequency_called_times == 1);
  // 验证 ad5932_set_start_frequency 传入的参数是否正确
  assert(mock_ad5932_set_start_frequency_param_freq == test_frequency);
}


/**
 * @brief 测试 wavegen_config_sweep_by_time_interval 函数
 * 是否正确调用了api层函数并传递了正确的参数。
 * 
 * todo: 暂时忽略时间间隔的计算正确性，只验证调用关系和参数传递。
 */
void test_wavegen_config_sweep_by_time_interval_calls_api_with_correct_params() {
  ad5932_mock_api_reset();

  // 定义测试参数
  int test_wave_type = 0; // 正弦波
  uint32_t start_freq = 10000;
  uint32_t delta_f_hz = 100;
  uint16_t sweep_points = 10; // 10个频点
  int mclk_mult = 0; // mclk不分频
  uint16_t interval = 1; // mclk不分频
  
  // 调用被测函数
  wavegen_config_sweep_by_time_interval(test_wave_type, start_freq, delta_f_hz, sweep_points, mclk_mult, interval);

  int expected_mode = 1; // 时间间隔模式

  // 验证调用
  // 1. 验证是否关闭了 STANDBY 引脚
  assert(mock_ad5932_set_standby_called_times == 1);
  assert(mock_ad5932_set_standby_param_enable == false);
  
  // 2. 验证是否复位了芯片
  assert(mock_ad5932_reset_called_times == 1);
  
  // 3. 验证是否写入了起始频率
  assert(mock_ad5932_set_start_frequency_called_times == 1);
  assert(mock_ad5932_set_start_frequency_param_freq == start_freq);

  // 4. 验证是否写入了频率递增值
  assert(mock_ad5932_set_delta_frequency_called_times == 1);
  assert(mock_ad5932_set_delta_frequency_param_delta_freq == delta_f_hz);

  // 5. 验证写入的频率递增次数是否为 sweep_points - 1
  assert(mock_ad5932_set_number_of_increments_param_frequency_increments == sweep_points - 1);

  // 6. 验证写入的时间间隔参数
  assert(mock_ad5932_set_increment_interval_called_times == 1);
  assert(mock_ad5932_set_increment_interval_param_mode == expected_mode);
  assert(mock_ad5932_set_increment_interval_param_mclk_mult == mclk_mult);
  assert(mock_ad5932_set_increment_interval_param_interval == interval);

  // 7. 验证是否设置了波形类型
  assert(mock_ad5932_set_waveform_called_times == 1);
  assert(mock_ad5932_set_waveform_param_wave_type == test_wave_type);

  printf("Test passed: wavegen_config_sweep_by_time_interval\n");
}


int main(void) {
  printf("Running app layer tests...\n");
  test_wavegen_output_stable_waveform_calls_api_with_correct_params();
  printf("All app layer tests passed!\n");
  return 0;
}