#include "mock_ad5932_api.h"

int mock_ad5932_set_waveform_called_times = 0;
int mock_ad5932_set_waveform_param_wave_type = -1;

int mock_ad5932_set_start_frequency_called_times = 0;
uint32_t mock_ad5932_set_start_frequency_param_freq = 0;

// 新增的 mock 变量定义
int mock_ad5932_set_standby_called_times = 0;
bool mock_ad5932_set_standby_param_enable = false;
int mock_ad5932_reset_called_times = 0;

void ad5932_mock_api_reset(void) {
  mock_ad5932_set_waveform_called_times = 0;
  mock_ad5932_set_waveform_param_wave_type = -1;
  mock_ad5932_set_start_frequency_called_times = 0;
  mock_ad5932_set_start_frequency_param_freq = 0;
  // 重置新增的 mock 变量
  mock_ad5932_set_standby_called_times = 0;
  mock_ad5932_set_standby_param_enable = false;
  mock_ad5932_reset_called_times = 0;
}

void ad5932_set_waveform(int wave_type) {
  mock_ad5932_set_waveform_called_times++;
  mock_ad5932_set_waveform_param_wave_type = wave_type;
}

void ad5932_set_start_frequency(uint32_t start_frequency_hz) {
  mock_ad5932_set_start_frequency_called_times++;
  mock_ad5932_set_start_frequency_param_freq = start_frequency_hz;
}

// 新增的 mock 函数实现
void ad5932_set_standby(bool enable) {
  mock_ad5932_set_standby_called_times++;
  mock_ad5932_set_standby_param_enable = enable;
}

void ad5932_reset(void) {
  mock_ad5932_reset_called_times++;
}