#include "wavegen_app.h"
#include "../api/ad5932_api.h"

/**
 * @brief 设置波形发生器输出固定频率和波形。
 * @param wave_type 波形类型（例如：正弦波、三角波、方波）。
 * @param frequency_hz 目标输出频率，单位为赫兹。
 */
void wavegen_output_fixed_wave(int wave_type, uint32_t frequency_hz) {
  // 设置频率。
  ad5932_set_start_frequency(frequency_hz);

  // 设置波形类型。
  ad5932_set_waveform(wave_type);
}