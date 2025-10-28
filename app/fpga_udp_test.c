#include "../dev/fpga.h"
#include "../dev/net_listener.h"
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/ether.h>
#include <signal.h>
#include <linux/ip.h>
#include <linux/udp.h>
static const char *i2c_dev = "/dev/i2c-2";
static const char *eth_interface = "eth0";

// 可在文件顶部声明
typedef struct {
  unsigned long total_packets;
  unsigned long udp_packets;
  unsigned long fpga_packets;
  unsigned long errors;
  unsigned long total_payload_bytes;
} packet_stats_t;

packet_stats_t stats; // 在你的 dev_network.c 或主程序里定义

// --- 辅助打印函数 ---
static void print_mac(const char *label, const uint8_t *mac) {
  printf("%s: %02X:%02X:%02X:%02X:%02X:%02X\n",
     label, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void print_ip(const char *label, uint32_t ip_addr) {
  struct in_addr addr;
  addr.s_addr = ip_addr;
  printf("%s: %s\n", label, inet_ntoa(addr));
}

// 缓存回调函数 - 用户获取缓存数据的接口
void user_cache_callback(const uint8_t *cache_data, uint32_t total_packets, 
  uint64_t total_bytes, const uint32_t *packet_lengths) {
  printf("\n=== 缓存数据统计 ===\n");
  printf("总包数: %u\n", total_packets);
  printf("总字节数: %lu\n", total_bytes);
  printf("平均包大小: %.2f 字节\n", total_packets > 0 ? (float)total_bytes / total_packets : 0);
  
  // 统计信息
  uint32_t fpga_packets = 0;
  uint32_t udp_packets = 0;
  uint32_t ip_packets = 0;
  uint64_t fpga_payload_bytes = 0;
  
  // 遍历缓存中的所有包，查找FPGA包
  uint32_t offset = 0;
  for (uint32_t i = 0; i < total_packets; i++) {
  const uint8_t *packet_data = cache_data + offset;
  int packet_len = packet_lengths[i];
  
  // 解析以太网头
  struct ethhdr *eth = (struct ethhdr*)packet_data;
  
  // 仅处理 IP 包
  if (ntohs(eth->h_proto) != ETH_P_IP) {
    offset += packet_len;
    continue;
  }
  
  ip_packets++;
  
  // 解析 IP 头
  struct iphdr *ip = (struct iphdr*)(packet_data + sizeof(struct ethhdr));
  
  // 仅处理 UDP 协议
  if (ip->protocol != IPPROTO_UDP) {
    offset += packet_len;
    continue;
  }
  
  udp_packets++;
  
  // 解析 UDP 头
  struct udphdr *udp = (struct udphdr*)(packet_data + sizeof(struct ethhdr) + ip->ihl * 4);
  uint16_t dest_port = ntohs(udp->dest);
  
  // 检查是否为 FPGA 包
  if (dest_port == 5030) {
    fpga_packets++;
    
    int udp_len = ntohs(udp->len);
    int payload_len = udp_len - sizeof(struct udphdr);
    if (payload_len > 0)
    fpga_payload_bytes += payload_len;
    
    // 前五个包详细打印
    if (fpga_packets <= 5) {
      printf("\n=== FPGA 数据包 #%u (缓存索引: %u) ===\n", fpga_packets, i + 1);
      print_mac("源MAC", eth->h_source);
      print_mac("目标MAC", eth->h_dest);
      print_ip("源IP", ip->saddr);
      print_ip("目标IP", ip->daddr);
      printf("源端口: %d\n", ntohs(udp->source));
      printf("目标端口: %d\n", dest_port);
      printf("UDP长度: %d\n", udp_len);
      printf("有效载荷: %d 字节\n", payload_len);
      printf("包总长度: %d 字节\n", packet_len);
      
      // 打印前 16 字节载荷
      int payload_offset = sizeof(struct ethhdr) + ip->ihl * 4 + sizeof(struct udphdr);
      if (packet_len > payload_offset) {
        printf("载荷前16字节: ");
        for (int j = 0; j < 16 && (payload_offset + j) < packet_len; j++) {
          printf("%02X ", packet_data[payload_offset + j]);
        }
        printf("\n");
      }
      printf("========================\n");
    }
  }

  offset += packet_len;
}

  printf("\n=== 详细统计 ===\n");
  printf("IP包数: %u\n", ip_packets);
  printf("UDP包数: %u\n", udp_packets);
  printf("FPGA包数: %u\n", fpga_packets);
  printf("FPGA载荷总字节: %lu\n", fpga_payload_bytes);
  printf("===================\n");
}



// --- 主回调函数 ---
void user_packet_printer(const uint8_t *buffer, int length) {
  struct ethhdr *eth = (struct ethhdr*)buffer;

  stats.total_packets++;

  // 仅处理 IP 包
  if (ntohs(eth->h_proto) != ETH_P_IP) {
    return;
  }

  struct iphdr *ip = (struct iphdr*)(buffer + sizeof(struct ethhdr));

  // 仅处理 UDP 协议
  if (ip->protocol != IPPROTO_UDP) {
    return;
  }

  stats.udp_packets++;

  struct udphdr *udp = (struct udphdr*)(buffer + sizeof(struct ethhdr) + ip->ihl * 4);
  uint16_t dest_port = ntohs(udp->dest);

  // 检查是否为 FPGA 包
  if (dest_port == 5030) {
    stats.fpga_packets++;

    int udp_len = ntohs(udp->len);
    int payload_len = udp_len - sizeof(struct udphdr);
    if (payload_len > 0)
      stats.total_payload_bytes += payload_len;

    // 前几个包详细打印
    if (stats.fpga_packets <= 5) {
      printf("\n=== FPGA 数据包 #%lu ===\n", stats.fpga_packets);
      print_mac("源MAC", eth->h_source);
      print_mac("目标MAC", eth->h_dest);
      print_ip("源IP", ip->saddr);
      print_ip("目标IP", ip->daddr);
      printf("源端口: %d\n", ntohs(udp->source));
      printf("目标端口: %d\n", dest_port);
      printf("UDP长度: %d\n", udp_len);
      printf("有效载荷: %d 字节\n", payload_len);
      printf("包总长度: %d 字节\n", length);

      // 打印前 16 字节载荷
      int payload_offset = sizeof(struct ethhdr) + ip->ihl * 4 + sizeof(struct udphdr);
      if (length > payload_offset) {
        printf("载荷前16字节: ");
        for (int i = 0; i < 16 && (payload_offset + i) < length; i++) {
          printf("%02X ", buffer[payload_offset + i]);
        }
        printf("\n");
      }
      printf("========================\n");
    } 
    else if (stats.fpga_packets % 100 == 0) {
      // 每100个FPGA包输出一个点表示进度
      printf(".");
      fflush(stdout);
    }
  }
}

static void signal_handler(int sig) {
  printf("\n收到停止信号，关闭监听...\n");
  net_listener_stop(eth_interface);

  printf("停止FPGA数据采集...\n");
  fpga_set_acq_enable(false);
  printf("监听已停止，程序退出。\n");
}

int main() {
  S_udp_header_params params = {
    .dst_mac_high = 0xb07b,
    .dst_mac_low  = 0x2500c019,
    .src_mac_high = 0x0043,
    .src_mac_low  = 0x4d494e40,
    .src_ip       = 0xa9fe972f,
    .dst_ip       = 0xc0a80105,
    .src_port     = 5000,
    .dst_port     = 5030,
    .ip_total_len = 0x041c,
    .udp_data_len = 0x408
  };

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  fpga_init(i2c_dev);
  fpga_set_acq_enable(false);
  fpga_initialize_udp_header(&params);
  printf("[主程序] FPGA UDP头初始化完成。\n");

  // 使用带缓存的监听
  if (net_listener_start_with_cache("eth0", user_packet_printer, 
                                  user_cache_callback, DEFAULT_CACHE_SIZE) < 0) {
      printf("[主程序] 网络监听启动失败。\n");
      return -1;
  }
  printf("[主程序] 网络监听已启动（带缓存），按 Ctrl+C 停止。\n");

  fpga_set_acq_enable(true);
  printf("[主程序] 数据采集已启用。\n");
  printf("[主程序] 此时应该有包上来...\n");

  // 主循环 - 可以定期打印缓存统计
  while (net_listener_is_running()) {
      sleep(5);
      cache_stats_t stats = net_listener_get_cache_stats();
      printf("[缓存统计] 包数: %u, 字节: %lu, 使用率: %.1f%%, 丢弃: %u\n",
             stats.total_packets, stats.total_bytes,
             stats.cache_size > 0 ? (float)stats.cache_used / stats.cache_size * 100 : 0,
             stats.dropped_packets);
  }

  printf("[主程序] 已退出监听。\n");
  return 0;
}