#include "sbeam.h"
#include "../dev/ad5932.h"
#include "../dev/dac63001.h"
#include "../dev/fpga.h"
#include "../utils/log.h"
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
  fpga_set_acq_enable(false);

  // 创建扫频线程
  pthread_t tid;
  SweepTask *task = malloc(sizeof(SweepTask));
  task->cfg = *cfg;
  pthread_create(&tid, NULL, sweep_thread, task);
  pthread_detach(tid); // 后台执行，用户不需 join
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
  dac63001_init(i2c_dev);
  // 配置外部参考模式
  if (dac63001_setup_external_ref() < 0) {
    LOG_ERROR("DAC配置失败\n");
    dac63001_close();
    return 1;
  }
  
  // 设置增益扫描
  if (start_gain == end_gain) {
    float voltage = ad8338_gain_to_voltage(start_gain);
    LOG_INFO("起始和结束增益相同，设置固定电压 %.3fV\n", voltage);
    dac63001_set_fixed_voltage(voltage);
    LOG_INFO("当前增益: %d dB (%.3fV)\n", start_gain, voltage);
    dac63001_close();
    return 1;
  }

  if (dac63001_set_gain_sweep(start_gain, end_gain, gain_duration_us) < 0) {
    LOG_ERROR("增益扫描设置失败\n");
    dac63001_close();
    return 1;
  }
  dac63001_start_waveform();
  usleep(5000); // 5 ms 延迟确保波形开始
  dac63001_stop_waveform();
  usleep(5000); // 5 ms 延迟确保波形停止
  fpga_set_acq_enable(false);
  dac63001_close();
}



int transmit_and_receive_single_beam(
  const DDSConfig *cfg,
  uint16_t start_gain,
  uint16_t end_gain,
  uint32_t gain_duration_us,
  NetPacketCallback callback
) {
  // 1. 初始化FPGA网络头
  fpga_init(i2c_dev);
  fpga_initialize_udp_header(&udp_header_params);
  
  // 2. 配置扫频参数
  ad5932_init();
  ad5932_reset();
  ad5932_set_start_frequency(cfg->start_freq);
  ad5932_set_delta_frequency(cfg->delta_freq, cfg->positive_incr);
  ad5932_set_number_of_increments(cfg->num_incr);
  ad5932_set_increment_interval(0, cfg->mclk_mult, cfg->interval_val);
  ad5932_set_waveform(cfg->wave_type);
  
  // 3. 配置接收增益和回调函数
  dac63001_init(i2c_dev);
  if (dac63001_setup_external_ref() < 0) {
    LOG_ERROR("DAC外部参考模式配置失败\n");
    dac63001_close();
    return -1;
  }

  // 固定增益模式，直接设置电压
  if (start_gain == end_gain) {
    float voltage = ad8338_gain_to_voltage(start_gain);
    LOG_INFO("起始和结束增益相同，直接设置固定电压进行接收信号 %.3fV\n", voltage);
    dac63001_set_fixed_voltage(voltage);
    LOG_INFO("当前增益: %d dB (%.3fV)\n", start_gain, voltage);
    dac63001_close();
  }

  // 锯齿波增益扫描模式，仅需配置
  if (dac63001_set_gain_sweep(start_gain, end_gain, gain_duration_us) < 0) {
    LOG_ERROR("增益扫描设置失败\n");
    dac63001_close();
    return -1;
  }

  // 4. 配置GPIO触发以同步增益扫描开始
  if (dac63001_enable_gpio_start_stop_trigger() < 0) {
    LOG_ERROR("DAC GPIO触发配置失败\n");
    dac63001_close();
    return -1;
  }
  
  // 5. 启动FPGA发送网络包 + 启动网络监听（先于扫频开始）
  net_listener_start(eth_ifname, callback);
  fpga_set_acq_enable(true);
  
  // 6. 启动扫频信号，并同步等待扫频结束(同时也是增益输出的触发信号)
  ad5932_start_sweep();
  LOG_INFO("扫频信号开始生成...\n");
  
  // 7. 扫频结束后硬件 GPIO 触发立即启动增益扫描波形
  while (!ad5932_is_sweep_done()) {
    usleep(1); // 1微秒轮询等待
  }
  LOG_INFO("扫频信号生成完成\n");
  
  // 8. 结束增益扫描
  usleep(2000); // 2ms 后输出
  dac63001_stop_waveform();
  
  // 8. 停止FPGA发送网络包
  fpga_set_acq_enable(false);
  LOG_INFO("单波束收发流程完成\n");
  
  // 清理资源
  ad5932_reset();
  ad5932_set_standby(false);
  dac63001_close();
  
  return 0;
}