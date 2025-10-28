#ifndef SBEAM_H
#define SBEAM_H
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../dev/net_listener.h"

typedef struct {
  uint32_t start_freq;     // 起始频率 (Hz)
  uint32_t delta_freq;     // 频率递增步长 (Hz)，0 表示固定频率
  uint16_t num_incr;       // 频率递增次数 (2~4095)
  uint8_t  wave_type;      // 波形类型 (0=正弦波, 1=三角波, 2=方波)
  uint8_t  mclk_mult;      // MCLK 乘数 (0=1x, 1=5x, 2=100x, 3=500x)
  uint16_t interval_val;   // 每个频率的持续周期 T (2~2047)
  bool     positive_incr;  // 扫频方向 (true=正向, false=负向)
} DDSConfig;

// 缓存统计信息结构体
typedef struct {
  uint32_t total_packets;      // 总包数
  uint64_t total_bytes;        // 总字节数
  uint32_t cache_size;         // 缓存大小
  uint32_t cache_used;         // 已用缓存
  uint32_t dropped_packets;    // 丢弃的包数
} sbeam_cache_stats_t;


/**
 * @brief 生成单波束信号（配置并启动 AD5932）
 * @details 
 *  1. 初始化 FPGA 的 UDP 包头配置信息，以确保数据正确发送。
 *  2. 配置 AD5932 芯片的各项参数，包括起始频率、频率步长、递增次数、波形类型等。
 *  3. 启动 AD5932 的频率扫描功能，开始生成所需的单波束信号。
 * @param cfg DDS 配置参数
 */
void generate_single_beam_signal(const DDSConfig *cfg);



/**
 * @brief 接收单波束信号的反馈数据
 * @details
 *  用户调用此函数后，程序将执行以下操作：
 *    1. 启动监听网络接口（如果之前未启动监听）。
 *    2. 配置 ADC63001 芯片的数据寄存器，以实现可变锯齿波增益控制。
 *    3. 启动 ADC63001 的电压控制 VGC 芯片，实现增益从 start_gain 到 end_gain 的变化。
 *    4. 在增益持续时间 gain_duration_us（微秒）结束后，停止增益控制。
 *    5. 返回监听到的数据，并通过用户提供的回调函数进行处理。
 *
 * @param start_gain       初始增益值（单位可根据硬件定义，例如 DAC 数值或 mV）。
 * @param end_gain         结束增益值。
 * @param gain_duration_us 增益持续时间（微秒）。时间大于 6000 微秒（6 ms）, 小于 17000000 微秒（17 秒）。
 * @param callback         网络包回调函数，用于处理每个接收到的数据包。
 *                         定义如下：
 *                         typedef void (*NetPacketCallback)(const uint8_t *data, int length);
 *                         回调函数将被调用多次，每次传入接收到的数据包及其长度。
 */
void receive_single_beam_response(
  uint16_t start_gain,
  uint16_t end_gain,
  uint32_t gain_duration_us,
  NetPacketCallback callback
);


/**
 * @brief 完整的单波束信号收发处理
 * @details
 *  行为描述
 *  该函数执行完整的单波束信号收发流程：
 *    1. 初始化FPGA网络头配置
 *    2. 配置扫频参数准备信号生成
 *    3. 配置接收增益参数和网络数据回调函数
 *    4. 配置GPIO触发以同步增益扫描开始
 *    5. 启动FPGA数据包发送和网络监听（先于扫频开始）
 *    6. 启动扫频信号生成，并同步等待扫频结束
 *    7. 扫频结束后硬件 GPIO 触发立即启动增益扫描波形
 *    8. 延迟5ms后停止增益扫描信号
 *    9. 停止FPGA数据包发送
 * @details
 *  执行细节：
 *   - 在信号发送之前，就已经配置好网络监听和回调函数，以及监听的增益以确保不会错过任何数据包。
 *   - 如果起始增益和结束增益相同，则直接设置固定电压预备信号接收。
 *   - 如果增益不同，则配置锯齿波增益扫描，并在扫频结束后启动增益扫描。
 *
 * @param cfg               DDS扫频配置参数
 * @param start_gain        初始增益值
 * @param end_gain          结束增益值  
 * @param gain_duration_us  增益扫描持续时间（微秒）
 * @param callback          网络数据包回调处理函数
 * @return int              执行结果：0-成功，其他-错误码
 */
int transmit_and_receive_single_beam(
  const DDSConfig *cfg,
  uint16_t start_gain,
  uint16_t end_gain,
  uint32_t gain_duration_us,
  NetPacketCallback callback
);


/**
 * @brief 接收单波束信号的反馈数据（带缓存）
 */
void receive_single_beam_response_with_cache(
  uint16_t start_gain,
  uint16_t end_gain,
  uint32_t gain_duration_us,
  NetPacketCallback packet_cb,      // 实时包回调（可选）
  NetCacheCallback cache_cb,        // 缓存回调（必须）
  uint32_t cache_size               // 缓存大小
);



/**
 * @brief 完整的单波束信号收发处理（带缓存）
 */
int transmit_and_receive_single_beam_with_cache(
  const DDSConfig *cfg,
  uint16_t start_gain,
  uint16_t end_gain,
  uint32_t gain_duration_us,
  NetPacketCallback packet_cb,      // 实时包回调（可选）
  NetCacheCallback cache_cb,        // 缓存回调（必须）
  uint32_t cache_size               // 缓存大小
);


/**
 * @brief 获取缓存统计信息
 */
sbeam_cache_stats_t sbeam_get_cache_stats(void);

/**
 * @brief 手动清空缓存
 */
void sbeam_clear_cache(void);

/**
 * @brief 停止网络监听（带缓存数据传递）
 */
void sbeam_stop_listener_with_cache(const char *ifname);

#endif  // SBEAM_H