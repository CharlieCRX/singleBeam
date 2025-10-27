#include "fpga.h"
#include "../protocol/i2c_hal.h"
#include "../utils/log.h"
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#pragma pack(1)
#define FPGA_I2C_SLAVE  (0x55)

// --- 结构体定义 (Packet Header Structures) ---

/**
 * @brief 以太网头部结构体
 */
typedef struct _eth_hdr{
  uint8_t dst_mac[6]; // 目的MAC地址
  uint8_t src_mac[6]; // 源MAC地址
  uint16_t eth_type;  // 以太网类型 (如 0x0800 for IP)
} eth_hdr;

/**
* @brief IP头部结构体
*/
typedef struct _ip_hdr{
  uint8_t ver_hl;     // 版本(4位) + 头部长度(4位)
  uint8_t serv_type;  // 服务类型
  uint16_t pkt_len;   // 总长度
  uint16_t re_mark;   // 标识
  uint16_t flag_seg;  // 标志 + 片偏移
  uint8_t surv_tm;    // 生存时间 (TTL)
  uint8_t protocol;   // 协议 (如 0x11 for UDP)
  uint16_t h_check;   // 头部校验和
  uint32_t src_ip;    // 源IP地址
  uint32_t dst_ip;    // 目的IP地址
} ip_hdr;

/**
* @brief UDP头部结构体
*/
typedef struct _udp_hdr{
  uint16_t sport;     // 源端口
  uint16_t dport;     // 目的端口
  uint16_t pktlen;    // 长度
  uint16_t check_sum; // 校验和 (UDP校验和，通常为0)
} udp_hdr;

/**
* @brief 封装完整数据包头部信息的联合体
*/
typedef union pkg_hdr{
  struct{
      eth_hdr eth;
      ip_hdr ip;
      udp_hdr udp;
      uint16_t pad; // 填充/对齐
  };
  // 确保数据部分的大小精确匹配结构体总和
  uint8_t data[sizeof(eth_hdr) + sizeof(ip_hdr) + sizeof(udp_hdr) + 2]; 
} U_pkg_hdr;

// 辅助函数
/* Checksum a block of data */
static uint16_t csum (uint16_t *packet, int packlen) {
	register unsigned long sum = 0;

	while (packlen > 1) {
		sum+= *(packet++);
		packlen-=2;
	}

	if (packlen > 0)
		sum += *(unsigned char *)packet;

	/* TODO: this depends on byte order */

	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	return (uint16_t) ~sum;
}
static void ipcsum(ip_hdr *ip) {
	ip->h_check=0;
	ip->h_check=csum((uint16_t *)ip, sizeof(ip_hdr));
}



/**
 * @brief 初始化 FPGA 设备
 *
 * @param  i2c_bus I2C总线设备路径
 * @return 成功返回0，失败返回-1
 */
int fpga_init(const char* i2c_bus) {
  return i2c_hal_init(i2c_bus);
}


/**
 * @brief FPGA 采集使能控制
 *
 * @param enable true使能采集，false禁用采集
 */
void fpga_set_acq_enable(bool enable) {
  uint16_t val = enable ? 0x0001 : 0x0000;
  LOG_INFO("FPGA set acquisition enable: %s\n", enable ? "ENABLED" : "DISABLED");
  i2c_hal_fpga_write(FPGA_I2C_SLAVE, REG_ACQ_EN, val);
}

/**
 * @brief 读取FPGA采集使能状态
 *
 * @return 采集使能状态，true表示使能，false表示禁用
 */
bool fpga_get_acq_enable(void) {
  uint32_t val = 0;
  if (i2c_hal_fpga_read(FPGA_I2C_SLAVE, REG_ACQ_EN, &val) < 0) {
    return false;
  }
  return (val & 0x1) != 0;
}

/**
 * @brief 触发FPGA软复位
 * @details 向FPGA的软复位寄存器写入复位命令, 一般需要 1 s后才能解除
 */
void fpga_trigger_soft_reset(void) {
  i2c_hal_fpga_write(FPGA_I2C_SLAVE, REG_FPGA_SRST, 0x0000);
}

/**
 * @brief 解除FPGA软复位
 */
void fpga_release_soft_reset(void) {
  i2c_hal_fpga_write(FPGA_I2C_SLAVE, REG_FPGA_SRST, 0x0001);
}


/**
 * @brief 初始化并写入UDP/IP/ETH头部到FPGA寄存器
 * @details 根据提供的参数构建UDP/IP/以太网帧头，计算IP校验和，并将整个头部数据以4字节为单位写入FPGA的指定寄存器区域。
 * @param  指向UDP头部配置参数结构体的指针。
 * @return 0表示成功，-1表示失败。
 */
int fpga_initialize_udp_header(S_udp_header_params *params) {
  LOG_INFO("Initializing FPGA UDP header...\n");
  if (params == NULL) {
    LOG_ERROR("Input parameters pointer is NULL.\n");
    return -1;
  }

  U_pkg_hdr pkg_hdr;
  memset(&pkg_hdr, 0, sizeof(U_pkg_hdr));

  // --- 1. 填充MAC地址 (处理字节序并拼接) ---
  
  // 目标MAC地址 (Dst MAC)
  // 注意：原代码使用htons/htonl进行字节序处理，我们保持原逻辑。
  uint16_t tmp_dst_h = htons(params->dst_mac_high);
  uint32_t tmp_dst_l = htonl(params->dst_mac_low);

  // 低4字节 (MAC[2]到MAC[5])
  memcpy(pkg_hdr.eth.dst_mac + 2, (unsigned char*)&tmp_dst_l, 4);
  // 高2字节 (MAC[0]到MAC[1])
  memcpy(pkg_hdr.eth.dst_mac, (unsigned char*)&tmp_dst_h, 2);

  // 源MAC地址 (Src MAC)
  uint16_t tmp_src_h = htons(params->src_mac_high);
  uint32_t tmp_src_l = htonl(params->src_mac_low);
  
  // 低4字节
  memcpy(pkg_hdr.eth.src_mac + 2, (unsigned char*)&tmp_src_l, 4);
  // 高2字节
  memcpy(pkg_hdr.eth.src_mac, (unsigned char*)&tmp_src_h, 2);

  LOG_INFO("Configured MAC addresses - DST: %02X:%02X:%02X:%02X:%02X:%02X, SRC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           pkg_hdr.eth.dst_mac[0], pkg_hdr.eth.dst_mac[1], pkg_hdr.eth.dst_mac[2],
           pkg_hdr.eth.dst_mac[3], pkg_hdr.eth.dst_mac[4], pkg_hdr.eth.dst_mac[5],
           pkg_hdr.eth.src_mac[0], pkg_hdr.eth.src_mac[1], pkg_hdr.eth.src_mac[2],
           pkg_hdr.eth.src_mac[3], pkg_hdr.eth.src_mac[4], pkg_hdr.eth.src_mac[5]);


  // --- 2. 填充以太网头部 (Ethernet Header) ---
  pkg_hdr.eth.eth_type = htons((uint16_t)0x0800); // 0x0800 for IPV4


  // --- 3. 填充IP头部 (IP Header) ---
  pkg_hdr.ip.ver_hl    = 0x45; // Version 4, Header Length 5 (20 bytes)
  pkg_hdr.ip.serv_type = 0;
  pkg_hdr.ip.pkt_len   = htons((uint16_t)0x041c); // 固定总长度 (IP头+UDP头+数据)
  pkg_hdr.ip.re_mark   = 0;
  pkg_hdr.ip.flag_seg  = 0;
  pkg_hdr.ip.surv_tm   = 0x80; // TTL
  pkg_hdr.ip.protocol  = 0x11; // 0x11 for UDP
  pkg_hdr.ip.h_check   = 0; // 校验和先清零，再计算
  
  // 源IP：使用传入参数并转换为网络字节序
  pkg_hdr.ip.src_ip = htonl(params->src_ip);
  pkg_hdr.ip.dst_ip = htonl(params->dst_ip); 

  // 计算IP校验和
  ipcsum(&pkg_hdr.ip);


  // --- 4. 填充UDP头部 (UDP Header) ---
  // 端口号：转换为网络字节序
  pkg_hdr.udp.sport = htons(params->src_port);
  pkg_hdr.udp.dport = htons(params->dst_port);
  pkg_hdr.udp.pktlen = htons((uint16_t)0x408); // 固定UDP总长度 (UDP头+数据)
  pkg_hdr.udp.check_sum = 0; // 简化：UDP校验和设置为0


  // --- 5. 打印配置信息 ---
  char src_ip_str[INET_ADDRSTRLEN];
  char dst_ip_str[INET_ADDRSTRLEN];
  
  inet_ntop(AF_INET, &pkg_hdr.ip.src_ip, src_ip_str, sizeof(src_ip_str));
  inet_ntop(AF_INET, &pkg_hdr.ip.dst_ip, dst_ip_str, sizeof(dst_ip_str));
  
  LOG_INFO("Configured IP addresses - SRC: %s (0x%08X), DST: %s (0x%08X)\n",
           src_ip_str, ntohl(pkg_hdr.ip.src_ip),
           dst_ip_str, ntohl(pkg_hdr.ip.dst_ip));
  LOG_INFO("Configured ports - SRC: %u, DST: %u\n", 
           ntohs(pkg_hdr.udp.sport), ntohs(pkg_hdr.udp.dport));
  LOG_INFO("IP packet length: %u, UDP total length: %u\n", 
           ntohs(pkg_hdr.ip.pkt_len), ntohs(pkg_hdr.udp.pktlen));

  // 打印结构体大小和偏移量 (用于验证对齐)
  LOG_INFO(" &pkg_hdr.pad - &pkg_hdr.eth = %zu bytes\n", 
         (size_t)((uint8_t*)&pkg_hdr.pad - (uint8_t*)&pkg_hdr.eth));
  LOG_INFO(" Total header size = %zu bytes\n", sizeof(U_pkg_hdr));
  ipcsum(&pkg_hdr.ip);


  // --- 6. 写入FPGA寄存器 ---
  LOG_INFO("Writing UDP header to FPGA registers (base address: 0x%04X)\n", REG_UDP_HDR_0);
  
  // 头部总长度以4字节对齐写入
  size_t header_dword_count = sizeof(U_pkg_hdr) / 4;
  LOG_INFO("Total header dwords to write: %zu\n", header_dword_count);
  for(size_t i = 0; i < header_dword_count; i++) {
    uint32_t write_data = *(uint32_t*)&pkg_hdr.data[i * 4];
    uint32_t read_back = 0;
    uint16_t addr = REG_UDP_HDR_0 + (uint16_t)i;

    fpga_reg_write_4Bytes(FPGA_I2C_SLAVE, addr, &pkg_hdr.data[i * 4]);
    i2c_hal_fpga_read(FPGA_I2C_SLAVE, addr, &read_back);
    
    LOG_INFO("index=%zu write=0x%08x readbk=0x%08x addr = 0x%02x\n", i, write_data, read_back, (REG_UDP_HDR_0 + i));
  }
  
  LOG_INFO("FPGA UDP header initialization completed successfully\n");
  return 0;
}