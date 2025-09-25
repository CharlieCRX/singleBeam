#include "wavegen_app.h"
#include "../api/ad5932_api.h"

/**
 * @brief 设置波形发生器输出固定频率和波形。
 * @param wave_type 波形类型（例如：0正弦波、1三角波、2方波）。
 * @param frequency_hz 目标输出频率，单位为赫兹。
 */
void wavegen_output_fixed_wave(int wave_type, uint32_t frequency_hz) {
  ad5932_set_standby(false); // 退出待机模式
  ad5932_reset();            // 复位芯片

  // 设置频率。
  ad5932_set_start_frequency(frequency_hz);

  // 设置波形类型。
  ad5932_set_waveform(wave_type);
}


/**
 * @brief 配置波形发生器进行频率扫描，按时间间隔递增频率。
 * 
 * @param wave_type 波形类型（例如：0正弦波、1三角波、2方波）。
 * @param start_freq_hz 起始频率，单位为赫兹。
 * @param delta_f_hz 每步频率递增值，单位为赫兹。
 * @param sweep_points 扫描的频点总数。
 * @param sweep_duration_us 扫描总时长，单位为微秒。
 */
void wavegen_config_sweep_by_time_interval(int wave_type, uint32_t start_freq_hz, uint32_t delta_f_hz, uint16_t sweep_points, uint32_t sweep_duration_us) {
  ad5932_set_standby(false); // 退出待机模式
  ad5932_reset();            // 复位芯片

  // 设置起始频率。
  ad5932_set_start_frequency(start_freq_hz);

  // 设置每步频率递增值。
  ad5932_set_delta_frequency(delta_f_hz, true); // true表示正向递增

  // 设置扫描的频点总数。
  ad5932_set_number_of_increments(sweep_points);

  // 计算每步时间间隔 (微秒)
  uint32_t t_step_us = sweep_duration_us / sweep_points; // 例如：1000 / 10 = 100us

  // 设置时间间隔 
  ad5932_set_increment_interval(0, 1, t_step_us); // mode=0(时间间隔)，mclk_mult=1(不分频)，interval=t_step_us // todo: 这里的 mode 和 mclk_mult 需要根据具体需求调整

  // 设置波形类型。
  ad5932_set_waveform(wave_type);
} 