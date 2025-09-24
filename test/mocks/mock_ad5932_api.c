#include "mock_ad5932_api.h"

// 记录调用次数和参数的变量
int mock_ad5932_set_waveform_called_times = 0;
int mock_ad5932_set_waveform_param_wave_type = -1;

int mock_ad5932_set_start_frequency_called_times = 0;
uint32_t mock_ad5932_set_start_frequency_param_freq = 0;

void ad5932_mock_api_reset(void) {
  mock_ad5932_set_waveform_called_times = 0;
  mock_ad5932_set_waveform_param_wave_type = -1;
  mock_ad5932_set_start_frequency_called_times = 0;
  mock_ad5932_set_start_frequency_param_freq = 0;
}

// 模拟 ad5932_set_waveform 函数，记录调用情况
void ad5932_set_waveform(int wave_type) {
  mock_ad5932_set_waveform_called_times++;
  mock_ad5932_set_waveform_param_wave_type = wave_type;
}

// 模拟 ad5932_set_start_frequency 函数，记录调用情况
void ad5932_set_start_frequency(uint32_t start_frequency_hz) {
  mock_ad5932_set_start_frequency_called_times++;
  mock_ad5932_set_start_frequency_param_freq = start_frequency_hz;
}