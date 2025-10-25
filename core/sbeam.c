#include "sbeam.h"
#include "../dev/ad5932.h"
#include "../dev/dac63001.h"
#include "../dev/fpga.h"
#include <pthread.h>
#include <unistd.h>

static const char *i2c_dev = "/dev/i2c-2";
static const char *eth_ifname = "eth0";

typedef struct {
  DDSConfig cfg;
} SweepTask;

S_udp_header_params udp_header_params = {
  .dst_mac_high = 0xb07b,
  .dst_mac_low  = 0x2500c019, // b0:7b:25:00:c0:19
  .src_mac_high = 0x0043,
  .src_mac_low  = 0x4d494e40, // 00:43:4D:49:4E:40
  .src_ip       = 0xa9fe972f, // 169.254.151.47
  .dst_ip       = 0xc0a80105, // 192.168.1.5
  .src_port     = 5000,
  .dst_port     = 5030,
  .ip_total_len = 0x041c, // 不生效
  .udp_data_len = 0x408
};


void* sweep_thread(void* arg) {
  SweepTask *task = (SweepTask*)arg;

  ad5932_init();
  ad5932_reset();
  ad5932_set_start_frequency(task->cfg.start_freq);
  ad5932_set_delta_frequency(task->cfg.delta_freq, task->cfg.positive_incr);
  ad5932_set_number_of_increments(task->cfg.num_incr);
  ad5932_set_increment_interval(0, task->cfg.mclk_mult, task->cfg.interval_val);
  ad5932_set_waveform(task->cfg.wave_type);
  ad5932_start_sweep();

  while (!ad5932_is_sweep_done()) {
    usleep(10); // 10微秒 轮询等待
  }

  ad5932_reset();
  ad5932_set_standby(false);

  free(task);
  return NULL;
}


void generate_single_beam_signal(const DDSConfig *cfg) {
  // 初始化 FPGA UDP 头
  fpga_init(i2c_dev);
  fpga_initialize_udp_header(&udp_header_params);

  // 创建扫频线程
  pthread_t tid;
  SweepTask *task = malloc(sizeof(SweepTask));
  task->cfg = *cfg;
  pthread_create(&tid, NULL, sweep_thread, task);
  pthread_detach(tid); // 后台执行，用户不需 join
}


/**
 * @brief 将AD8338增益(dB)转换为GAIN引脚电压（通用版本，支持外部电阻）
 * @param gain_db: 目标增益值
 * @param r_feedback: 反馈电阻值（欧姆）
 * @param r_input: 输入电阻值（欧姆）
 * @return GAIN引脚电压值 (V)
 */
float gain_to_voltage_ex(float gain_db, float r_feedback, float r_input)
{
  // 通用增益公式: G_dB = 80×(V_GAIN-0.1) + 20log(R_FEEDBACK/R_N) - 26
  // 转换为: V_GAIN = (G_dB - 20log(R_FEEDBACK/R_N) + 26)/80 + 0.1
  
  float log_term = 20.0f * log10f(r_feedback / r_input);
  float voltage = (gain_db - log_term + 26.0f) / 80.0f + 0.1f;
  
  // 确保电压在有效范围内 (0.1V 到 1.1V)
  if (voltage < 0.1f) {
    voltage = 0.1f;
  } else if (voltage > 1.1f) {
    voltage = 1.1f;
  }
  
  return voltage;
}

void receive_single_beam_response(
  uint16_t start_gain,
  uint16_t end_gain,
  uint32_t gain_duration_us,
  NetPacketCallback callback
) {
  // 启动FPGA采集
  fpga_init(i2c_dev);
  fpga_set_acq_enable(true);

  // 启动网络监听
  net_listener_start(eth_ifname, callback);

  // 配置DAC63001增益控制
}