#include "mock_ad5932_api.h"

int mock_ad5932_set_waveform_called_times = 0;
int mock_ad5932_set_waveform_param_wave_type = -1;
int mock_ad5932_set_start_frequency_called_times = 0;
uint32_t mock_ad5932_set_start_frequency_param_freq = 0;

// 新增的 mock 变量定义
int mock_ad5932_set_standby_called_times = 0;
bool mock_ad5932_set_standby_param_enable = false;
int mock_ad5932_reset_called_times = 0;

uint32_t mock_ad5932_set_delta_frequency_param_delta_freq = 0;
bool mock_ad5932_set_delta_frequency_param_positive = false;
uint16_t mock_ad5932_set_number_of_increments_param_frequency_increments = 0;

int mock_ad5932_set_increment_interval_called_times = 0;
int mock_ad5932_set_delta_frequency_called_times = 0;
int mock_ad5932_set_number_of_increments_called_times = 0;
int mock_ad5932_set_increment_interval_param_mode = -1;
int mock_ad5932_set_increment_interval_param_mclk_mult = -1;
uint16_t mock_ad5932_set_increment_interval_param_interval = 0;

void ad5932_mock_api_reset(void) {
  mock_ad5932_reset_called_times = 0;

  mock_ad5932_set_waveform_called_times = 0;
  mock_ad5932_set_waveform_param_wave_type = -1;
  mock_ad5932_set_start_frequency_called_times = 0;
  mock_ad5932_set_start_frequency_param_freq = 0;

  mock_ad5932_set_standby_called_times = 0;
  mock_ad5932_set_standby_param_enable = false;

  mock_ad5932_set_delta_frequency_param_delta_freq = 0;
  mock_ad5932_set_delta_frequency_param_positive = false;
  mock_ad5932_set_number_of_increments_param_frequency_increments = 0;

  mock_ad5932_set_increment_interval_called_times = 0;
  mock_ad5932_set_delta_frequency_called_times = 0;
  mock_ad5932_set_number_of_increments_called_times = 0;
  mock_ad5932_set_increment_interval_param_mode = -1;
  mock_ad5932_set_increment_interval_param_mclk_mult = -1;
  mock_ad5932_set_increment_interval_param_interval = 0;
}

void ad5932_reset(void) {
  mock_ad5932_reset_called_times++;
}

void ad5932_set_waveform(int wave_type) {
  mock_ad5932_set_waveform_called_times++;
  mock_ad5932_set_waveform_param_wave_type = wave_type;
}

void ad5932_set_start_frequency(uint32_t start_frequency_hz) {
  mock_ad5932_set_start_frequency_called_times++;
  mock_ad5932_set_start_frequency_param_freq = start_frequency_hz;
}

void ad5932_set_delta_frequency(uint32_t delta_freq, bool positive) {
  mock_ad5932_set_delta_frequency_called_times++;
  mock_ad5932_set_delta_frequency_param_delta_freq = delta_freq;
  mock_ad5932_set_delta_frequency_param_positive = positive;
}

void ad5932_set_number_of_increments(uint16_t frequency_increments) {
  mock_ad5932_set_number_of_increments_called_times++;
  mock_ad5932_set_number_of_increments_param_frequency_increments = frequency_increments;
}

void ad5932_set_increment_interval(int mode, int mclk_mult, uint16_t interval) {
  mock_ad5932_set_increment_interval_called_times++;
  mock_ad5932_set_increment_interval_param_mode = mode;
  mock_ad5932_set_increment_interval_param_mclk_mult = mclk_mult;
  mock_ad5932_set_increment_interval_param_interval = interval;
}

// 新增的 mock 函数实现
void ad5932_set_standby(bool enable) {
  mock_ad5932_set_standby_called_times++;
  mock_ad5932_set_standby_param_enable = enable;
}

// 其他与引脚相关的API函数的空实现
void ad5932_interrupt(void) {
  // 空实现
}

bool ad5932_is_sweep_done(void) {
  // 默认返回 false，表示扫频未完成
  return false;
}

void ad5932_start_sweep(void) {
  // 空实现
}