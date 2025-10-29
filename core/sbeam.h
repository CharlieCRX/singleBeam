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
 * @param start_gain        初始增益值(小于结束增益值)
 * @param end_gain          结束增益值  
 * @param gain_duration_us  增益扫描持续时间（1ms - 250ms）
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
 * @brief 执行单波束收发流程（支持缓存回调模式）
 * 
 * @details
 * 该函数负责控制 DDS（AD5932）、DAC（DAC63001）、FPGA 及网络模块，
 * 实现单次波束发射与同步接收的数据采集过程。
 * 支持两种模式：
 *  - **实时回调模式**：每接收到网络包立即触发回调处理。
 *  - **缓存回调模式**：先将网络数据缓存，再由 cache_cb 回调统一处理。
 * 
 * 函数主要执行以下步骤：
 *  1. 初始化 FPGA 并写入 UDP/IP 头部配置；
 *  2. 配置 DDS（AD5932）的扫频参数；
 *  3. 初始化 DAC（DAC63001）并设置接收增益；
 *  4. 根据增益模式选择固定电压或 GPIO 触发扫描；
 *  5. 启动网络监听（实时或缓存模式）；
 *  6. 启动 FPGA 采集使能；
 *  7. 启动 DDS 扫频信号输出；
 *  8. 扫频完成后自动触发 DAC 增益波形输出；
 *  9. 等待接收过程完成后，关闭采集；
 * 10. 停止网络监听、释放相关硬件资源。
 * 
 * 本函数适用于一次性单波束的完整收发流程控制，常用于自动化测试或信号链验证。
 * 
 * @param[in] cfg
 *      DDS 配置参数结构体，包含起始频率、步进频率、增量数、MCLK 倍频、波形类型等。
 * @param[in] start_gain
 *      接收增益起始值（单位：dB）。  
 *      若与 end_gain 相同，则启用固定增益模式。
 *      不能大于结束增益。
 * @param[in] end_gain
 *      接收增益结束值（单位：dB）。  
 *      若与 start_gain 不同，则启用锯齿波扫描模式。
 * @param[in] gain_duration_us
 *      增益扫描总时长（单位：微秒），用于 DAC 输出波形持续时间控制。范围在 1ms - 250ms 
 * @param[in] packet_cb(可以为空)
 *      网络包实时回调函数指针，用于在接收 FPGA 数据包时进行处理。
 *      若使用缓存模式，该回调依然会被调用，但缓存优先。
 * @param[in] cache_cb
 *      网络缓存回调函数指针，用于缓存模式下的数据统一处理。
 *      若为 NULL，则仅使用实时包回调。
 * @param[in] cache_size（最大为 50 * 1024 * 1024 = 50Mb）
 *      缓存区大小（字节），仅在 cache_cb 不为空时生效。
 * 
 * @return
 *  返回执行状态码：
 *  - `0`：收发流程执行成功；
 *  - `-1`：任意初始化、配置或启动阶段失败。
 * 
 * @note
 * - 调用前需确保 `i2c_dev`、`eth_ifname`、`udp_header_params` 已正确初始化；
 * - 若启用缓存模式，需保证系统内存充足；
 * - 函数执行过程为阻塞式，直到扫频和接收流程完全结束；
 * - 在错误退出时会自动关闭 DAC 和网络监听。
 * 
 * @see
 *  - `ad5932_*()` 系列函数：用于配置扫频信号；
 *  - `dac63001_*()` 系列函数：用于设置接收增益；
 *  - `fpga_*()` 系列函数：用于控制数据采集；
 *  - `net_listener_start_with_cache()`：网络缓存监听启动接口。
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