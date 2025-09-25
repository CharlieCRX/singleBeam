#ifndef _MOCK_AD5932_API_H_
#define _MOCK_AD5932_API_H_

#include <stdint.h>
#include <stdbool.h>

// 外部变量，用于记录调用次数和参数
extern int mock_ad5932_set_waveform_called_times;
extern int mock_ad5932_set_waveform_param_wave_type;

extern int mock_ad5932_set_start_frequency_called_times;
extern uint32_t mock_ad5932_set_start_frequency_param_freq;

extern int mock_ad5932_set_standby_called_times;
extern bool mock_ad5932_set_standby_param_enable;
extern int mock_ad5932_reset_called_times;

extern uint32_t mock_ad5932_set_delta_frequency_param_delta_freq;
extern bool mock_ad5932_set_delta_frequency_param_positive;
extern uint16_t mock_ad5932_set_number_of_increments_param_frequency_increments;

extern int mock_ad5932_set_delta_frequency_called_times;
extern int mock_ad5932_set_increment_interval_called_times;
extern int mock_ad5932_set_number_of_increments_called_times;
extern int mock_ad5932_set_increment_interval_param_mode;
extern int mock_ad5932_set_increment_interval_param_mclk_mult;
extern uint16_t mock_ad5932_set_increment_interval_param_interval;

// 用于重置所有mock状态的函数
void ad5932_mock_api_reset(void);

// 声明与原函数签名一致的测试桩函数
void ad5932_set_waveform(int wave_type);
void ad5932_set_start_frequency(uint32_t start_frequency_hz);
void ad5932_set_standby(bool enable);
void ad5932_reset(void);
void ad5932_set_delta_frequency(uint32_t delta_freq, bool positive);
void ad5932_set_number_of_increments(uint16_t frequency_increments);
void ad5932_set_increment_interval(int mode, int mclk_mult, uint16_t interval);

#endif // _MOCK_AD5932_API_H_