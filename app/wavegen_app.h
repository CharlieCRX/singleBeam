#ifndef WAVEGEN_APP_H
#define WAVEGEN_APP_H

#include <stdint.h>
#include <stdbool.h>

void wavegen_output_fixed_wave(int wave_type, uint32_t frequency_hz);
void wavegen_config_sweep_by_time_interval(int wave_type, uint32_t start_freq_hz, uint32_t delta_f_hz, uint16_t sweep_points, int mclk_mult, uint16_t interval);
void wavegen_config_sweep_by_output_cycle_interval(int wave_type, uint32_t start_freq_hz, uint32_t delta_f_hz, uint16_t sweep_points, int mclk_mult, uint16_t interval);
bool wavegen_is_sweep_finished();
void wavegen_abort_sweep();

#endif // WAVEGEN_APP_H