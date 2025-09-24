#ifndef _MOCK_AD5932_API_H_
#define _MOCK_AD5932_API_H_

#include <stdint.h>
#include <stdbool.h>

// 外部变量，用于记录调用次数和参数
extern int mock_ad5932_set_waveform_called_times;
extern int mock_ad5932_set_waveform_param_wave_type;

extern int mock_ad5932_set_start_frequency_called_times;
extern uint32_t mock_ad5932_set_start_frequency_param_freq;

// 用于重置所有mock状态的函数
void ad5932_mock_api_reset(void);

// 声明与原函数签名一致的测试桩函数
void ad5932_set_waveform(int wave_type);
void ad5932_set_start_frequency(uint32_t start_frequency_hz);

#endif // _MOCK_AD5932_API_H_