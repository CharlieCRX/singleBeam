#ifndef DEV_FPGA_H
#define DEV_FPGA_H
#include <stdint.h>
#include <stdbool.h>
/* FPGA Register Addresses
 * 基于 RK3568 平台的 FPGA 寄存器地址定义
*/
typedef enum {
  // 基础控制寄存器
  REG_ACQ_EN          = 0x0000,   // 采集使能
  REG_FPGA_SRST       = 0x0006,   // FPGA软复位

  REG_FPGA_DATE       = 0x000b,   // FPGA日期
  REG_FPGA_VER        = 0x000c,   // FPGA版本

  // UDP包头寄存器
  REG_UDP_HDR_0       = 0x0040,   // UDP包头: 源MAC byte5-byte2
  REG_UDP_HDR_1       = 0x0041,   // UDP包头: 源MAC byte1-byte0, 目的MAC byte5-byte4
  REG_UDP_HDR_2       = 0x0042,   // UDP包头: 源MAC byte3-byte0, 协议类型/IP版本/服务类型
  REG_UDP_HDR_3       = 0x0043,   // UDP包头: 总长度/标识
  REG_UDP_HDR_4       = 0x0044,   // UDP包头: 标志位/生存时间/协议
  REG_UDP_HDR_5       = 0x0045,   // UDP包头: IP部首校验和/源IP byte3-byte2
  REG_UDP_HDR_6       = 0x0046,   // UDP包头: 源IP byte1-byte0/目的IP byte3-byte2
  REG_UDP_HDR_7       = 0x0047,   // UDP包头: 目的IP byte1-byte0/源端口
  REG_UDP_HDR_8       = 0x0048,   // UDP包头: 目的端口/UDP报文长度
  REG_UDP_HDR_9       = 0x0050,   // UDP包头: UDP报文校验和/保留

  // DDS 控制寄存器
  REG_DDS_CTRL        = 0x0010,   // DDS控制线，0→1跳变触发DDS发送信号，写1后自动回0
  REG_DDS_INT         = 0x0011,   // DDS中断输入，默认为0
  REG_DDS_STB         = 0x0012,   // DDS待机状态，0:正常工作，1:待机
  REG_DDS_SYNCOUT     = 0x0013,   // DDS同步输出信号，0:未完成信号发送，1:已完成发送

} fpga_reg_addr_t;


/**
* @brief UDP头部配置参数结构体
* @details 包含了构建UDP/IP/ETH头部所需的所有可配置字段
*/
typedef struct {
  // MAC地址配置 (高16位 + 低32位，用于字节序处理)
  uint16_t dst_mac_high;
  uint32_t dst_mac_low;
  uint16_t src_mac_high;
  uint32_t src_mac_low;
  
  // IP地址 (主机字节序)
  uint32_t src_ip;
  uint32_t dst_ip;
  
  // 端口 (主机字节序)
  uint16_t src_port;
  uint16_t dst_port;

  // IP/UDP总长度 (主机字节序)
  uint16_t ip_total_len;   // 对应原代码中的 0x041c
  uint16_t udp_data_len;   // 对应原代码中的 0x408 (UDP数据长度+8)
} S_udp_header_params;

/*
 * @brief 初始化 FPGA 设备
 */
int fpga_init(const char* i2c_bus);

/**
 * @brief FPGA 采集使能控制
 */
void fpga_set_acq_enable(bool enable);

/**
 * @brief 读取FPGA采集使能状态
 */
bool fpga_get_acq_enable(void);

/**
 * @brief 触发FPGA软复位
 * @details 向FPGA的软复位寄存器写入复位命令, 一般需要 1 s后才能恢复正常工作
 */
void fpga_trigger_soft_reset(void);


/**
 * @brief 解除FPGA软复位
 */
void fpga_release_soft_reset(void);


/**
 * @brief 配置FPGA的UDP包头信息
 */
int fpga_initialize_udp_header(S_udp_header_params *params);

// DDS 控制函数声明
bool fpga_is_dds_standby_up(void);
void fpga_set_dds_ctrl_pulse(bool enable);
void fpga_set_dds_standby(bool enable);

#endif // DEV_FPGA_H