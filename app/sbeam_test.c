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
#include "../utils/log.h"

// 函数声明
void test_packet_callback(const uint8_t *buffer, int length);
void test_cache_callback(const uint8_t *cache_data, uint32_t total_packets, 
                        uint64_t total_bytes, const uint32_t *packet_lengths);
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

// 测试配置参数
typedef struct {
  // DDS 配置参数
  uint32_t start_freq;
  uint32_t delta_freq;
  uint16_t num_incr;
  uint8_t wave_type;
  uint8_t mclk_mult;
  uint16_t interval_val;
  bool positive_incr;
  
  // 增益控制参数
  uint16_t start_gain;
  uint16_t end_gain;
  uint32_t gain_duration_us;
  
  // 测试控制参数
  bool test_generate_only;
  bool test_receive_only;
  uint32_t test_duration_sec;

  bool use_transceive_func; // 是否使用收发一体函数
  bool test_with_cache;
} TestConfig;

// 全局统计变量
static volatile bool keep_running = true;
static int packet_count = 0;
static size_t total_data_received = 0;
static int sweep_completed = 0;


// 全局统计（需要在文件开头定义）
static unsigned long total_packets = 0;
static unsigned long udp_packets = 0;
static unsigned long fpga_packets = 0;
static unsigned long errors = 0;
static unsigned long total_payload_bytes = 0;

/**
 * @brief 信号处理函数 - 优雅退出
 */
void signal_handler(int sig) {
  printf("\n📡 收到停止信号 (SIG%d)，正在停止测试...\n", sig);
  keep_running = false;
}


// 缓存回调函数
void test_cache_callback(const uint8_t *cache_data, uint32_t total_packets, 
            uint64_t total_bytes, const uint32_t *packet_lengths) {
  printf("\n=== 缓存数据回调 ===\n");
  printf("总包数: %u\n", total_packets);
  printf("总字节数: %lu\n", total_bytes);
  printf("平均包大小: %.2f 字节\n", total_packets > 0 ? (float)total_bytes / total_packets : 0);
  
  // 详细解析前5个FPGA包
  uint32_t fpga_packets = 0;
  uint32_t offset = 0;
  
  for (uint32_t i = 0; i < total_packets && fpga_packets < 5; i++) {
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
      fpga_packets++;
      
      int udp_len = ntohs(udp->len);
      int payload_len = udp_len - sizeof(struct udphdr);
      
      printf("\n=== FPGA 缓存包 #%u ===\n", fpga_packets);
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
    
    offset += packet_len;
  }
  
  printf("\n缓存数据分析完成，共找到 %u 个FPGA包\n", fpga_packets);
}

/**
 * @brief 测试带缓存的收发一体函数
 */
int test_transceive_function_with_cache(const TestConfig *config) {
  printf("\n🔄 开始测试带缓存的收发一体函数...\n");
  
  // 准备 DDS 配置
  DDSConfig dds_config = {
    .start_freq = config->start_freq,
    .delta_freq = config->delta_freq,
    .num_incr = config->num_incr,
    .wave_type = config->wave_type,
    .mclk_mult = config->mclk_mult,
    .interval_val = config->interval_val,
    .positive_incr = config->positive_incr
  };
  
  printf("📊 DDS配置: %u Hz起始, %u Hz步长, %u次递增\n", 
       dds_config.start_freq, dds_config.delta_freq, dds_config.num_incr);
  printf("📊 增益配置: %d dB -> %d dB, 持续时间: %.3f秒\n",
       config->start_gain, config->end_gain, 
       config->gain_duration_us / 1000000.0f);
  printf("💾 缓存模式: 启用, 缓存大小: %u MB\n", DEFAULT_CACHE_SIZE / (1024 * 1024));
  
  // 调用带缓存的收发一体函数
  printf("🎛️  调用 transmit_and_receive_single_beam_with_cache()...\n");
  
  int result = transmit_and_receive_single_beam_with_cache(
    &dds_config,
    config->start_gain,
    config->end_gain,
    config->gain_duration_us,
    NULL,  // 实时包回调（可选）
    test_cache_callback,   // 缓存回调
    DEFAULT_CACHE_SIZE
  );
  
  if (result == 0) {
    printf("✅ 带缓存的收发一体函数执行成功\n");
    
    // 显示缓存统计
    sbeam_cache_stats_t stats = sbeam_get_cache_stats();
    printf("📊 缓存统计: 包数=%u, 字节=%lu, 使用率=%.1f%%, 丢弃=%u\n",
         stats.total_packets, stats.total_bytes,
         stats.cache_size > 0 ? (float)stats.cache_used / stats.cache_size * 100 : 0,
         stats.dropped_packets);
  } else {
    printf("❌ 带缓存的收发一体函数执行失败，错误码: %d\n", result);
  }
  
  return result;
}

/**
 * @brief 测试带缓存的信号接收功能
 */
int test_signal_reception_with_cache(const TestConfig *config) {
  printf("\n📡 开始测试带缓存的信号接收功能...\n");
  
  printf("🎛️  调用 receive_single_beam_response_with_cache()...\n");
  printf("📊 增益扫描: %d dB -> %d dB, 持续时间: %.3f秒\n",
    config->start_gain, config->end_gain, 
    config->gain_duration_us / 1000000.0f);
  printf("💾 缓存模式: 启用, 缓存大小: %u MB\n", DEFAULT_CACHE_SIZE / (1024 * 1024));
  
  receive_single_beam_response_with_cache(
    config->start_gain,
    config->end_gain,
    config->gain_duration_us,
    test_packet_callback,  // 实时包回调（可选）
    test_cache_callback,   // 缓存回调
    DEFAULT_CACHE_SIZE
  );
  
  printf("✅ 带缓存的信号接收命令已发送\n");
  return 0;
}

/**
 * @brief 网络数据包详细分析回调函数
 */
void test_packet_callback(const uint8_t *buffer, int length) {
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

    // 前几个包详细打印
    if (fpga_packets <= 5) {
      printf("\n");
      printf("╔══════════════════════════════════════════════════╗\n");
      printf("║                FPGA 数据包 #%-4lu                 ║\n", fpga_packets);
      printf("╠══════════════════════════════════════════════════╣\n");
      
      // MAC 地址信息
      printf("║ MAC 层信息:                                      ║\n");
      printf("║   源 MAC     : %02X:%02X:%02X:%02X:%02X:%02X                 ║\n",
             eth->h_source[0], eth->h_source[1], eth->h_source[2],
             eth->h_source[3], eth->h_source[4], eth->h_source[5]);
      printf("║   目标 MAC   : %02X:%02X:%02X:%02X:%02X:%02X                 ║\n",
             eth->h_dest[0], eth->h_dest[1], eth->h_dest[2],
             eth->h_dest[3], eth->h_dest[4], eth->h_dest[5]);
      printf("║   协议类型   : 0x%04X (IP)                       ║\n", ntohs(eth->h_proto));
      
      // IP 层信息
      printf("║ IP 层信息:                                       ║\n");
      struct in_addr src_ip, dst_ip;
      src_ip.s_addr = ip->saddr;
      dst_ip.s_addr = ip->daddr;
      printf("║   源 IP      : %-15s                   ║\n", inet_ntoa(src_ip));
      printf("║   目标 IP    : %-15s                   ║\n", inet_ntoa(dst_ip));
      printf("║   协议       : %d (UDP)                          ║\n", ip->protocol);
      printf("║   IP 头长度  : %-2d 字节 (%-2d ×32位字)              ║\n", ip->ihl * 4, ip->ihl);
      printf("║   总长度     : %-5d 字节                        ║\n", ntohs(ip->tot_len));
      
      // UDP 层信息
      printf("║ UDP 层信息:                                      ║\n");
      printf("║   源端口     : %-5d                             ║\n", ntohs(udp->source));
      printf("║   目标端口   : %-5d (FPGA)                      ║\n", dest_port);
      printf("║   UDP 长度   : %-5d 字节                        ║\n", udp_len);
      printf("║   校验和     : 0x%04X                            ║\n", ntohs(udp->check));
      
      // 载荷信息
      printf("║ 载荷信息:                                        ║\n");
      printf("║   有效载荷   : %-4d 字节                         ║\n", payload_len);
      printf("║   包总长度   : %-4d 字节                         ║\n", length);
      
      // 打印前 32 字节载荷
      int payload_offset = sizeof(struct ethhdr) + ip->ihl * 4 + sizeof(struct udphdr);
      if (length > payload_offset) {
          printf("║   载荷前32字节:                                  ║\n");
          printf("║     ");
          for (int i = 0; i < 32 && (payload_offset + i) < length; i++) {
              printf("%02X ", buffer[payload_offset + i]);
              if ((i + 1) % 16 == 0 && i < 31 && (payload_offset + i + 1) < length) {
                  printf("║\n║     ");
              }
          }
          printf("║\n");
          
          // 尝试解析为ADC数据（如果适用）
          if (payload_len >= 4) {
              uint32_t sample_data = *(uint32_t*)(buffer + payload_offset);
              printf("║   首样本数据 : 0x%08X                        ║\n", sample_data);
          }
      }
        
      printf("╚══════════════════════════════════════════════════╝\n");
    } 
    else if (fpga_packets % 100 == 0) {
      // 每100个FPGA包输出统计信息
      printf("\n📊 数据接收统计: 总包=%lu, UDP包=%lu, FPGA包=%lu, 数据量=%.2f KB\n",
            total_packets, udp_packets, fpga_packets, total_payload_bytes / 1024.0);
    }
    else if (fpga_packets % 10 == 0) {
      // 每10个包输出一个点表示进度
      printf(".");
      fflush(stdout);
    }
    
    // 更新全局统计（如果需要在其他地方访问）
    packet_count         = fpga_packets;
    total_data_received  = total_payload_bytes;
  }
  else {
    // 非FPGA包的可选处理
    if (total_packets <= 10) {
      printf("⚠️  非FPGA UDP包: 目标端口=%d\n", dest_port);
    }
  }
}




/**
 * @brief 扫频完成回调（模拟）
 */
void sweep_completion_callback() {
  sweep_completed = 1;
  printf("🎯 扫频完成回调触发\n");
}

/**
 * @brief 打印测试配置信息
 */
void print_test_config(const TestConfig *config) {
  printf("\n");
  printf("╔══════════════════════════════════════════════════╗\n");
  printf("║        单波束信号测试配置                        ║\n");
  printf("╠══════════════════════════════════════════════════╣\n");
  printf("║ DDS 信号生成配置:                                ║\n");
  printf("║  起始频率: %10u Hz                         ║\n", config->start_freq);
  printf("║  频率步长: %10u Hz                         ║\n", config->delta_freq);
  printf("║  递增次数: %10u                            ║\n", config->num_incr);
  printf("║  波形类型: %10d (0=正弦,1=三角,2=方波)     ║\n", config->wave_type);
  printf("║  MCLK倍数: %10d (0=1x,1=5x,2=100x,3=500x)  ║\n", config->mclk_mult);
  printf("║  间隔参数: %10u                            ║\n", config->interval_val);
  printf("║  扫频方向: %10s                              ║\n", 
       config->positive_incr ? "正向" : "负向");
  printf("║                                                  ║\n");
  printf("║ 增益控制配置:                                    ║\n");
  printf("║  起始增益: %10u dB                         ║\n", config->start_gain);
  printf("║  结束增益: %10u dB                         ║\n", config->end_gain);
  printf("║  持续时间: %10u us (%.3f秒)               ║\n", 
       config->gain_duration_us, config->gain_duration_us / 1000000.0f);
  printf("║                                                  ║\n");
  printf("║ 测试控制配置:                                    ║\n");
  printf("║  仅生成测试: %7s                              ║\n", 
       config->test_generate_only ? "是" : "否");
  printf("║  仅接收测试: %7s                              ║\n", 
       config->test_receive_only ? "是" : "否");
  printf("║  测试时长: %10u 秒                         ║\n", config->test_duration_sec);
  printf("╚══════════════════════════════════════════════════╝\n");
  printf("\n");
}

/**
 * @brief 打印测试结果统计
 */
void print_test_results1(void) {
  printf("\n");
  printf("╔══════════════════════════════════════════════════╗\n");
  printf("║          测试结果统计           ║\n");
  printf("╠══════════════════════════════════════════════════╣\n");
  printf("║ 数据包接收统计:                 ║\n");
  printf("║  总数据包数: %8d 个             ║\n", packet_count);
  printf("║  总数据量: %10lu 字节          ║\n", total_data_received);
  printf("║  平均包大小: %8.1f 字节/包         ║\n", 
       packet_count > 0 ? (float)total_data_received / packet_count : 0);
  printf("║                        ║\n");
  printf("║ 扫频状态:                     ║\n");
  printf("║  扫频完成: %8s                ║\n", 
       sweep_completed ? "是" : "否");
  printf("╚══════════════════════════════════════════════════╝\n");
  printf("\n");
}

/**
 * @brief 打印最终统计信息（修改版）
 */
void print_test_results(void) {
  
  // 这里应该从实际的统计变量获取数据
  fpga_packets = packet_count;
  total_payload_bytes = total_data_received;
  
  printf("\n");
  printf("╔══════════════════════════════════════════════════╗\n");
  printf("║                  网络包统计详情                  ║\n");
  printf("╠══════════════════════════════════════════════════╣\n");
  printf("║ 总接收包数: %8lu 个                          ║\n", total_packets);
  printf("║ UDP包数量:  %8lu 个                          ║\n", udp_packets);
  printf("║ FPGA包数量: %8lu 个                          ║\n", fpga_packets);
  printf("║ 总数据量:   %8lu 字节 (%.2f KB)           ║\n", 
         total_payload_bytes, total_payload_bytes / 1024.0);
  if (fpga_packets > 0) {
    printf("║ 平均包大小: %8.1f 字节/包                     ║\n", 
           (float)total_payload_bytes / fpga_packets);
    printf("║ 数据速率:  %8.2f KB/s                         ║\n", 
             (total_payload_bytes / 1024.0) / (packet_count > 0 ? packet_count / 100.0 : 1.0));
  }
  printf("╚══════════════════════════════════════════════════╝\n");
  printf("\n");
  
  // 性能分析
  if (fpga_packets > 0) {
    printf("📈 性能分析:\n");
    printf("   包处理速率: %.1f 包/秒\n", 
      (float)fpga_packets / (packet_count > 0 ? packet_count / 100.0 : 1.0));
    printf("   数据吞吐量: %.2f MB/s\n", 
      (total_payload_bytes / (1024.0 * 1024.0)) / (packet_count > 0 ? packet_count / 100.0 : 1.0));
    
    if (fpga_packets < 10) {
      printf("   ⚠️  接收包数较少，请检查硬件连接和网络配置\n");
    } else if (total_payload_bytes / fpga_packets < 100) {
      printf("   ⚠️  平均包大小较小，可能存在数据不完整\n");
    } else {
      printf("   ✅ 数据接收正常\n");
    }
  }
}

/**
 * @brief 测试信号生成功能
 */
int test_signal_generation(const TestConfig *config) {
  printf("\n🚀 开始测试信号生成功能...\n");
  
  // 准备 DDS 配置
  DDSConfig dds_config = {
    .start_freq = config->start_freq,
    .delta_freq = config->delta_freq,
    .num_incr = config->num_incr,
    .wave_type = config->wave_type,
    .mclk_mult = config->mclk_mult,
    .interval_val = config->interval_val,
    .positive_incr = config->positive_incr
  };
  
  // 计算预期参数
  uint32_t final_freq = dds_config.start_freq + 
             (dds_config.positive_incr ? 1 : -1) * 
             dds_config.delta_freq * dds_config.num_incr;
  
  printf("📊 预期扫频范围: %u Hz -> %u Hz\n", 
       dds_config.start_freq, final_freq);
  printf("📊 频率点数: %d\n", (dds_config.num_incr + 1) * dds_config.interval_val);
  
  // 生成信号
  printf("🎛️  调用 generate_single_beam_signal()...\n");
  generate_single_beam_signal(&dds_config);
  
  printf("✅ 信号生成命令已发送（后台线程执行）\n");
  return 0;
}

/**
 * @brief 测试信号接收功能
 */
int test_signal_reception(const TestConfig *config) {
  printf("\n📡 开始测试信号接收功能...\n");
  
  printf("🎛️  调用 receive_single_beam_response()...\n");
  printf("📊 增益扫描: %d dB -> %d dB, 持续时间: %.3f秒\n",
    config->start_gain, config->end_gain, 
    config->gain_duration_us / 1000000.0f);
  
  receive_single_beam_response(
    config->start_gain,
    config->end_gain,
    config->gain_duration_us,
    test_packet_callback
  );
  
  printf("✅ 信号接收命令已发送\n");
  return 0;
}

/**
 * @brief 综合测试：生成和接收同时进行
 */
int test_integrated_operation(const TestConfig *config) {
  printf("\n🔗 开始综合测试：信号生成 + 接收...\n");
  
  // 先启动信号生成
  if (test_signal_generation(config) != 0) {
    return -1;
  }
  
  // 然后启动信号接收
  if (test_signal_reception(config) != 0) {
    return -1;
  }
  
  printf("✅ 综合测试已启动\n");
  return 0;
}


/**
 * @brief 测试收发一体函数
 */
int test_transceive_function(const TestConfig *config) {
  printf("\n🔄 开始测试收发一体函数...\n");
  
  // 准备 DDS 配置
  DDSConfig dds_config = {
      .start_freq = config->start_freq,
      .delta_freq = config->delta_freq,
      .num_incr = config->num_incr,
      .wave_type = config->wave_type,
      .mclk_mult = config->mclk_mult,
      .interval_val = config->interval_val,
      .positive_incr = config->positive_incr
  };
  
  printf("📊 DDS配置: %u Hz起始, %u Hz步长, %u次递增\n", 
         dds_config.start_freq, dds_config.delta_freq, dds_config.num_incr);
  printf("📊 增益配置: %d dB -> %d dB, 持续时间: %.3f秒\n",
         config->start_gain, config->end_gain, 
         config->gain_duration_us / 1000000.0f);
  
  // 计算预期扫频范围
  uint32_t final_freq = dds_config.start_freq + 
             (dds_config.positive_incr ? 1 : -1) * 
             dds_config.delta_freq * dds_config.num_incr;
  
  printf("🎯 预期扫频范围: %u Hz -> %u Hz\n", 
         dds_config.start_freq, final_freq);
  printf("🎯 频率点数: %d\n", dds_config.num_incr + 1);
  
  // 调用收发一体函数
  printf("🎛️  调用 transmit_and_receive_single_beam()...\n");
  
  int result = transmit_and_receive_single_beam(
      &dds_config,
      config->start_gain,
      config->end_gain,
      config->gain_duration_us,
      test_packet_callback
  );
  
  if (result == 0) {
      printf("✅ 收发一体函数执行成功\n");
  } else {
      printf("❌ 收发一体函数执行失败，错误码: %d\n", result);
  }
  
  return result;
}

/**
 * @brief 打印使用说明
 */
void print_usage(const char *program_name) {
  fprintf(stderr, "\n用法: %s [选项]\n", program_name);
  fprintf(stderr, "单波束信号生成与接收综合测试程序\n\n");
  
  fprintf(stderr, "必需选项:\n");
  fprintf(stderr, "  无 (默认执行完整综合测试)\n\n");
  
  fprintf(stderr, "测试模式选项:\n");
  fprintf(stderr, "  -c, --with-cache        启用数据包缓存模式 (默认500MB)\n");
  fprintf(stderr, "  -t, --time SECONDS      测试持续时间 (默认: 10秒)\n");
  fprintf(stderr, "  -h, --help              显示此帮助信息\n\n");
  
  fprintf(stderr, "DDS 信号生成参数 (AD5932):\n");
  fprintf(stderr, "  --start-freq HZ         起始频率Hz (默认: 100000 = 100kHz)\n");
  fprintf(stderr, "  --delta-freq HZ         频率递增步长 (0表示固定频率)\n");
  fprintf(stderr, "  --num-incr COUNT        频率递增次数 (范围 2~4095, 默认: 2)\n");
  fprintf(stderr, "  --wave-type TYPE        输出波形类型 (0=正弦波, 1=三角波, 2=方波, 默认: 2)\n");
  fprintf(stderr, "  --mclk-mult MULT        MCLK倍频系数 (0=1x, 1=5x, 2=100x, 3=500x, 默认: 0)\n");
  fprintf(stderr, "  --interval-val VAL      每个频率的持续周期 (范围 2~2047, 默认: 2)\n");
  fprintf(stderr, "  --negative-sweep        使用负向扫频 (默认: 正向扫频)\n\n");
  
  fprintf(stderr, "增益控制参数 (DAC63001 + AD8338):\n");
  fprintf(stderr, "  --start-gain DB         起始增益值 (范围 0-80 dB, 默认: 0dB)\n");
  fprintf(stderr, "  --end-gain DB           结束增益值 (范围 0-80 dB, 默认: 80dB)\n");
  fprintf(stderr, "  --duration-us US        增益扫描持续时间us (范围 1000 ~ 250000 us)\n\n");
  
  fprintf(stderr, "扫频参数说明:\n");
  fprintf(stderr, "  - 扫频范围: 起始频率 -> 起始频率 + (递增次数 × 频率步长)\n");
  fprintf(stderr, "  - 频率点数: 递增次数 + 1\n");
  fprintf(stderr, "  - 总波形数量: (递增次数 + 1) × 持续周期\n\n");

  fprintf(stderr, "网络数据包处理模式:\n");
  fprintf(stderr, "  - 实时回调模式: 每个数据包接收后立即处理\n");
  fprintf(stderr, "  - 缓存模式: 数据包先存入内存，测试结束后统一处理\n");
  fprintf(stderr, "  - 混合模式: 同时支持实时处理和缓存 (-c 与 -i 或 -r 联用)\n\n");

  fprintf(stderr, "使用示例:\n\n");
  
  fprintf(stderr, "带缓存的收发测试:\n");
  fprintf(stderr, "  %s -c --start-freq 300 --delta-freq 0 \\\n", program_name);
  fprintf(stderr, "     --num-incr 2 --interval-val 2 --start-gain 0 --end-gain 80 --duration-us 60000\n");
  fprintf(stderr, "  - 固定扫频范围: 300Hz，共输出6个波形\n");
  fprintf(stderr, "  - 增益扫描: 00dB → 60dB，持续60ms\n");
  fprintf(stderr, "  - 启用数据包缓存(500MB)，测试结束后详细分析\n\n");
  
  // fprintf(stderr, "3. 仅信号生成测试:\n");
  // fprintf(stderr, "  %s -g --start-freq 500000 --delta-freq 50000 \\\n", program_name);
  // fprintf(stderr, "     --num-incr 5 --interval-val 2 --wave-type 2\n");
  // fprintf(stderr, "  - DDS扫频: 500kHz → 750kHz，共6个频率点\n");
  // fprintf(stderr, "  - 方波输出，共12个波形\n\n");
  
  // fprintf(stderr, "4. 带缓存的信号接收测试:\n");
  // fprintf(stderr, "  %s -r -c --start-gain 0 --end-gain 60 --duration-us 600000\n", program_name);
  // fprintf(stderr, "  - 增益扫描: 0dB → 60dB，持续0.6秒\n");
  // fprintf(stderr, "  - AD8338增益控制电压: 1.1V → 0.5V\n");
  // fprintf(stderr, "  - 所有网络数据包缓存供后续分析\n\n");
  
  // fprintf(stderr, "5. 固定频率输出测试:\n");
  // fprintf(stderr, "  %s --start-freq 2000000 --delta-freq 0 --num-incr 1 --interval-val 50\n", program_name);
  // fprintf(stderr, "  - 固定2MHz频率输出 (频率步长=0)\n");
  // fprintf(stderr, "  - 共100个相同频率的波形\n\n");
  
  // fprintf(stderr, "6. 高增益灵敏度测试:\n");
  // fprintf(stderr, "  %s --start-gain 20 --end-gain 80 --duration-us 3000000\n", program_name);
  // fprintf(stderr, "  - 高增益范围: 20dB → 80dB\n");
  // fprintf(stderr, "  - 3秒缓慢增益变化\n");
  // fprintf(stderr, "  - 适用于弱信号检测和动态范围测试\n\n");
  
  fprintf(stderr, "MCLK倍频系数影响:\n");
  fprintf(stderr, "  0 (1倍):   每个频率点标准持续时间\n");
  fprintf(stderr, "  1 (5倍):   每个频率点持续时间延长5倍\n");
  fprintf(stderr, "  2 (100倍): 每个频率点持续时间延长100倍 (慢速扫频)\n");
  fprintf(stderr, "  3 (500倍): 每个频率点持续时间延长500倍 (极慢速扫频)\n\n");
  
  fprintf(stderr, "硬件依赖说明:\n");
  fprintf(stderr, "  - DDS信号生成: AD5932芯片 (SPI控制)\n");
  fprintf(stderr, "  - 增益控制: DAC63001 + AD8338 VGA (I2C控制)\n");
  fprintf(stderr, "  - 数据采集: FPGA网络数据流 (UDP端口5030)\n");
  fprintf(stderr, "  - 网络接口: eth0 (默认)\n");
  
  fprintf(stderr, "\n性能说明:\n");
  fprintf(stderr, "  - 缓存模式适用于高数据率场景 (>10 MB/s)\n");
  fprintf(stderr, "  - 实时模式适用于需要即时数据处理的场景\n");
  fprintf(stderr, "  - 默认缓存大小: 500MB (支持约20秒的20 MB/s数据流)\n");
  fprintf(stderr, "  - 混合模式同时提供实时监控和事后分析能力\n");
}

/**
 * @brief 解析命令行参数
 */
int parse_arguments(int argc, char *argv[], TestConfig *config) {
  // 设置默认值
  *config = (TestConfig){
    .start_freq = 500000,
    .delta_freq = 50000,
    .num_incr = 2,
    .wave_type = 2,
    .mclk_mult = 0,
    .interval_val = 2,
    .positive_incr = true,
    .start_gain = 0,
    .end_gain = 80,
    .gain_duration_us = 1000,
    .test_generate_only = false,
    .test_receive_only = false,
    .test_duration_sec = 10
  };
  
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "--generate-only") == 0) {
      config->test_generate_only = true;
    } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--receive-only") == 0) {
      config->test_receive_only = true;
    } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--time") == 0) {
      if (i + 1 < argc) {
        config->test_duration_sec = atoi(argv[++i]);
      }
    } else if (strcmp(argv[i], "--start-freq") == 0 && i + 1 < argc) {
      config->start_freq = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--delta-freq") == 0 && i + 1 < argc) {
      config->delta_freq = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--num-incr") == 0 && i + 1 < argc) {
      config->num_incr = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--wave-type") == 0 && i + 1 < argc) {
      config->wave_type = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--mclk-mult") == 0 && i + 1 < argc) {
      config->mclk_mult = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--interval-val") == 0 && i + 1 < argc) {
      config->interval_val = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--negative-sweep") == 0) {
      config->positive_incr = false;
    } else if (strcmp(argv[i], "--start-gain") == 0 && i + 1 < argc) {
      config->start_gain = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--end-gain") == 0 && i + 1 < argc) {
      config->end_gain = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--duration-us") == 0 && i + 1 < argc) {
      config->gain_duration_us = atoi(argv[++i]);
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      print_usage(argv[0]);
      return 1;
    } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--integrated") == 0) {
      config->use_transceive_func = true;
    } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--with-cache") == 0) {
      config->test_with_cache = true;
    }
    else {
      fprintf(stderr, "错误: 未知参数 '%s'\n", argv[i]);
      print_usage(argv[0]);
      return -1;
    }
  }
  
  // 参数验证
  if (config->test_generate_only && config->test_receive_only) {
    fprintf(stderr, "错误: 不能同时指定 --generate-only 和 --receive-only\n");
    return -1;
  }
  
  return 0;
}

/**
 * @brief 主测试函数
 */
int main(int argc, char *argv[]) {
  TestConfig config;
  int ret;
  
  printf("🎛️  ====================================\n");
  printf("🎛️  单波束信号生成与接收测试程序   \n");
  printf("🎛️  ====================================\n");
  
  // 解析命令行参数
  ret = parse_arguments(argc, argv, &config);
  if (ret != 0) {
    return ret == 1 ? 0 : -1;
  }
  
  // 打印测试配置
  print_test_config(&config);
  
  // 注册信号处理
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  
  // 执行测试
  if (config.test_generate_only) {
    // 仅测试信号生成
    ret = test_signal_generation(&config);
    if (ret == 0) {
      printf("⏳ 等待扫频完成（最长 %d 秒）...\n", config.test_duration_sec);
    }
  } else if (config.test_receive_only) {
    // 仅测试信号接收
    ret = test_signal_reception(&config);
    if (ret == 0) {
      printf("⏳ 数据接收中（%d 秒）...\n", config.test_duration_sec);
    }
  } else if (config.use_transceive_func) {
    // 使用收发一体函数测试
    ret = test_transceive_function(&config);
    if (ret == 0) {
        printf("⏳ 收发一体测试运行中（%d 秒）...\n", config.test_duration_sec);
    }
  } else if (config.test_with_cache) {
    // 带缓存的综合测试
    ret = test_transceive_function_with_cache(&config);
    if (ret == 0) {
      printf("⏳ 带缓存的综合测试运行中（%d 秒）...\n", config.test_duration_sec);
    }
  } else {
    // 综合测试
    ret = test_integrated_operation(&config);
    if (ret == 0) {
      printf("⏳ 综合测试运行中（%d 秒）...\n", config.test_duration_sec);
    }
  }
  
  if (ret != 0) {
    fprintf(stderr, "❌ 测试启动失败\n");
    return -1;
  }
  
  // 主等待循环
  time_t start_time = time(NULL);
  while (keep_running && (time(NULL) - start_time) < config.test_duration_sec) {
    sleep(1);
    
    // 每5秒显示一次状态
    if ((time(NULL) - start_time) % 5 == 0) {
      printf("⏱️  测试运行中: %ld/%d 秒, 已接收 %d 个数据包\n", 
           time(NULL) - start_time, config.test_duration_sec, packet_count);
    }
  }
  
  // 测试完成
  printf("\n✅ 测试完成\n");
  print_test_results();
  
  // 清理工作
  printf("🧹 执行清理工作...\n");
  // 注意：实际的硬件清理应该在各个驱动模块中完成
  
  printf("🎉 测试程序正常退出\n");
  return 0;
}