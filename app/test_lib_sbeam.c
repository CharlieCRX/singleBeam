#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include "sbeam.h"

// 全局控制变量
static volatile bool keep_running = true;

// 统计变量
static unsigned long total_packets = 0;
static unsigned long udp_packets = 0;
static unsigned long fpga_packets = 0;
static unsigned long total_payload_bytes = 0;

/**
 * @brief 信号处理函数 - 优雅退出
 */
void signal_handler(int sig) {
  printf("\n📡 收到停止信号 (SIG%d)，正在停止测试...\n", sig);
  keep_running = false;
}

/**
 * @brief 实时数据包回调函数
 */
void lib_packet_callback(const uint8_t *buffer, int length) {
  struct ethhdr *eth = (struct ethhdr*)buffer;
  total_packets++;

  // 仅处理 IP 包
  if (ntohs(eth->h_proto) != ETH_P_IP) {
    return;
  }

  struct iphdr *ip = (struct iphdr*)(buffer + sizeof(struct ethhdr));

  // 仅处理 UDP 协议
  if (ip->protocol != IPPROTO_UDP) {
    return;
  }

  udp_packets++;

  struct udphdr *udp = (struct udphdr*)(buffer + sizeof(struct ethhdr) + ip->ihl * 4);
  uint16_t dest_port = ntohs(udp->dest);

  // 检查是否为 FPGA 包 (目标端口5030)
  if (dest_port == 5030) {
    fpga_packets++;

    int udp_len = ntohs(udp->len);
    int payload_len = udp_len - sizeof(struct udphdr);
    if (payload_len > 0)
      total_payload_bytes += payload_len;

    // 每100个包输出统计信息
    if (fpga_packets % 100 == 0) {
      printf("📊 实时数据: FPGA包=%lu, 数据量=%.2f KB\n", 
           fpga_packets, total_payload_bytes / 1024.0);
    }
  }
}

/**
 * @brief 缓存数据回调函数
 */
void lib_cache_callback(const uint8_t *cache_data, uint32_t total_packets, 
             uint64_t total_bytes, const uint32_t *packet_lengths) {
  printf("\n=== 缓存数据分析回调 ===\n");
  printf("总包数: %u\n", total_packets);
  printf("总字节数: %lu\n", total_bytes);
  printf("平均包大小: %.2f 字节\n", total_packets > 0 ? (float)total_bytes / total_packets : 0);
  
  // 解析前3个FPGA包
  uint32_t fpga_packets_found = 0;
  uint32_t offset = 0;
  
  for (uint32_t i = 0; i < total_packets && fpga_packets_found < 3; i++) {
    const uint8_t *packet_data = cache_data + offset;
    int packet_len = packet_lengths[i];
    
    // 解析以太网头
    struct ethhdr *eth = (struct ethhdr*)packet_data;
    
    // 仅处理 IP 包
    if (ntohs(eth->h_proto) != ETH_P_IP) {
      offset += packet_len;
      continue;
    }
    
    // 解析 IP 头
    struct iphdr *ip = (struct iphdr*)(packet_data + sizeof(struct ethhdr));
    
    // 仅处理 UDP 协议
    if (ip->protocol != IPPROTO_UDP) {
      offset += packet_len;
      continue;
    }
    
    // 解析 UDP 头
    struct udphdr *udp = (struct udphdr*)(packet_data + sizeof(struct ethhdr) + ip->ihl * 4);
    uint16_t dest_port = ntohs(udp->dest);
    
    // 检查是否为 FPGA 包
    if (dest_port == 5030) {
      fpga_packets_found++;
      
      int udp_len = ntohs(udp->len);
      int payload_len = udp_len - sizeof(struct udphdr);
      
      printf("\n--- FPGA缓存包 #%u ---\n", fpga_packets_found);
      printf("源端口: %d, 目标端口: %d\n", ntohs(udp->source), dest_port);
      printf("UDP长度: %d, 有效载荷: %d 字节\n", udp_len, payload_len);
      
      // 打印前 8 字节载荷
      int payload_offset = sizeof(struct ethhdr) + ip->ihl * 4 + sizeof(struct udphdr);
      if (packet_len > payload_offset) {
        printf("载荷前8字节: ");
        for (int j = 0; j < 8 && (payload_offset + j) < packet_len; j++) {
          printf("%02X ", packet_data[payload_offset + j]);
        }
        printf("\n");
      }
    }
    
    offset += packet_len;
  }
  
  printf("\n缓存分析完成，共找到 %u 个FPGA包\n", fpga_packets_found);
}

/**
 * @brief 打印测试配置
 */
void print_lib_test_config(const DDSConfig *dds_cfg, 
              uint16_t start_gain, uint16_t end_gain, 
              uint32_t gain_duration_us, uint32_t cache_size) {
  printf("\n");
  printf("╔══════════════════════════════════════════════════╗\n");
  printf("║       库调用测试配置            ║\n");
  printf("╠══════════════════════════════════════════════════╣\n");
  printf("║ DDS 配置:                    ║\n");
  printf("║  起始频率: %10u Hz             ║\n", dds_cfg->start_freq);
  printf("║  频率步长: %10u Hz             ║\n", dds_cfg->delta_freq);
  printf("║  递增次数: %10u              ║\n", dds_cfg->num_incr);
  printf("║  波形类型: %10d              ║\n", dds_cfg->wave_type);
  printf("║  MCLK倍数: %10d              ║\n", dds_cfg->mclk_mult);
  printf("║  间隔参数: %10u              ║\n", dds_cfg->interval_val);
  printf("║  扫频方向: %10s                ║\n", 
       dds_cfg->positive_incr ? "正向" : "负向");
  printf("║                          ║\n");
  printf("║ 增益配置:                    ║\n");
  printf("║  起始增益: %10u dB             ║\n", start_gain);
  printf("║  结束增益: %10u dB             ║\n", end_gain);
  printf("║  持续时间: %10u us (%.3f秒)         ║\n", 
       gain_duration_us, gain_duration_us / 1000000.0f);
  printf("║                          ║\n");
  printf("║ 缓存配置:                    ║\n");
  printf("║  缓存大小: %10u MB             ║\n", cache_size / (1024 * 1024));
  printf("╚══════════════════════════════════════════════════╝\n");
  printf("\n");
}

/**
 * @brief 打印测试结果
 */
void print_lib_test_results(void) {
  printf("\n");
  printf("╔══════════════════════════════════════════════════╗\n");
  printf("║         库调用测试结果          ║\n");
  printf("╠══════════════════════════════════════════════════╣\n");
  printf("║ 实时数据统计:                   ║\n");
  printf("║  总接收包数: %8lu 个              ║\n", total_packets);
  printf("║  UDP包数量:  %8lu 个              ║\n", udp_packets);
  printf("║  FPGA包数量: %8lu 个              ║\n", fpga_packets);
  printf("║  总数据量:   %8lu 字节 (%.2f KB)       ║\n", 
       total_payload_bytes, total_payload_bytes / 1024.0);
  if (fpga_packets > 0) {
    printf("║  平均包大小: %8.1f 字节/包           ║\n", 
         (float)total_payload_bytes / fpga_packets);
  }
  
  // 获取缓存统计
  sbeam_cache_stats_t stats = sbeam_get_cache_stats();
  printf("║                          ║\n");
  printf("║ 缓存统计:                    ║\n");
  printf("║  总包数:   %8u 个              ║\n", stats.total_packets);
  printf("║  总字节数:   %8lu 字节            ║\n", stats.total_bytes);
  printf("║  缓存使用率: %7.1f%%              ║\n", 
       stats.cache_size > 0 ? (float)stats.cache_used / stats.cache_size * 100 : 0);
  printf("║  丢弃包数:   %8u 个              ║\n", stats.dropped_packets);
  printf("╚══════════════════════════════════════════════════╝\n");
  printf("\n");
}

/**
 * @brief 主测试函数
 */
int main(int argc, char *argv[]) {
  printf("🎛️  ====================================\n");
  printf("🎛️    sBeam库调用测试程序       \n");
  printf("🎛️  ====================================\n");
  
  // 注册信号处理
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  
  // 配置DDS参数
  DDSConfig dds_config = {
    .start_freq = 300,    // 300 Hz
    .delta_freq = 0,     // 50 kHz 步长
    .num_incr = 2,       // 5次递增
    .wave_type = 2,      // 方波
    .mclk_mult = 0,      // 1倍MCLK
    .interval_val = 2,     // 每个频率2个周期
    .positive_incr = true    // 正向扫频
  };
  
  // 增益参数
  uint16_t start_gain = 0;
  uint16_t end_gain = 80;
  uint32_t gain_duration_us = 60000;  // 60ms
  
  // 缓存配置
  uint32_t cache_size = 10 * 1024 * 1024;  // 10MB缓存
  
  // 打印测试配置
  print_lib_test_config(&dds_config, start_gain, end_gain, gain_duration_us, cache_size);
  
  printf("🚀 开始调用库函数...\n");
  
  // 调用库函数 - 这是主要测试点
  int result = transmit_and_receive_single_beam_with_cache(
    &dds_config,
    start_gain,
    end_gain,
    gain_duration_us,
    lib_packet_callback,  // 实时包回调
    lib_cache_callback,   // 缓存回调
    cache_size        // 缓存大小
  );
  
  if (result == 0) {
    printf("✅ 库函数调用成功\n");
  } else {
    printf("❌ 库函数调用失败，错误码: %d\n", result);
    return -1;
  }
  
  // 等待一段时间让测试运行
  printf("⏳ 测试运行中（等待5秒）...\n");
  for (int i = 0; i < 5 && keep_running; i++) {
    sleep(1);
    printf("⏱️  运行中... %d/5 秒\n", i + 1);
  }
  
  // 打印测试结果
  print_lib_test_results();
  
  // 清理工作
  printf("🧹 执行清理工作...\n");
  sbeam_clear_cache();  // 清理缓存
  
  printf("🎉 库调用测试完成\n");
  return 0;
}