#ifndef WAVEGEN_APP_H
#define WAVEGEN_APP_H

#include <stdint.h>

void wavegen_output_fixed_wave(int wave_type, uint32_t frequency_hz);


void wavegen_config_sweep_by_time_interval(int wave_type, uint32_t start_freq_hz, uint32_t delta_f_hz, uint16_t sweep_points, uint32_t sweep_duration_us);

#endif // WAVEGEN_APP_H