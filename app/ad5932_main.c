#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdint.h>
#include <fcntl.h>


#include "../driver/ad5932_driver.h"
#include "../api/ad5932_api.h"

void print_usage(const char *program_name) {
  fprintf(stderr, "\n用法: %s [选项]\n", program_name);
  fprintf(stderr, "用于配置 AD5932 DDS 芯片进行内部自动频率扫描。\n\n");
  fprintf(stderr, "必需选项:\n");
  fprintf(stderr, "  -s, --start-freq <Hz>    起始频率 (Fstart)\n");
  fprintf(stderr, "\n可选选项:\n");
  fprintf(stderr, "  -d, --delta-freq <Hz>    频率递增步长 (Delta F, 默认为 0 时，输出固定频率波形)\n");
  fprintf(stderr, "  -n, --num-incr <次数>    递增次数 (N_incr, 范围 2~4095, 默认: 2)\n");
  fprintf(stderr, "  -w, --waveform <类型>    输出波形类型 (0=正弦波, 1=三角波, 2=方波, 默认: 0)\n");
  fprintf(stderr, "  -m, --interval-mult <M>  MCLK乘数 (0=1x, 1=5x, 2=100x, 3=500x, 默认: 0)\n");
  fprintf(stderr, "  -i, --interval-val <T>   递增间隔值 T (2-2047, 默认: 2)\n");
  fprintf(stderr, "  -g, --negative           使用负向频率步长 (扫频方向向下)\n");
  fprintf(stderr, "  -h, --help               显示此帮助信息\n");
  fprintf(stderr, "\n示例:\n");
  fprintf(stderr, "  %s -s 10000 -d 500 -n 20 -w 0 -i 5\n\n", program_name);
}

int main(int argc, char *argv[]) {
  uint32_t start_freq = 0;
  uint32_t delta_freq = 0;
  uint16_t num_incr = 0;
  int wave_type = 0;
  int mclk_mult = 0;
  uint16_t interval_val = 2;
  bool positive_incr = true;

  int opt;
  static struct option long_options[] = {
    {"start-freq",    required_argument, 0, 's'},
    {"delta-freq",    required_argument, 0, 'd'},
    {"num-incr",      required_argument, 0, 'n'},
    {"waveform",      optional_argument, 0, 'w'},
    {"interval-mult", optional_argument, 0, 'm'},
    {"interval-val",  optional_argument, 0, 'i'},
    {"negative",      no_argument,       0, 'g'},
    {"help",          no_argument,       0, 'h'},
    {0, 0, 0, 0}
  };

  while ((opt = getopt_long(argc, argv, "s:d:n:w:m:i:gh", long_options, NULL)) != -1) {
    switch (opt) {
      case 's': start_freq = (uint32_t)atol(optarg); break;
      case 'd': delta_freq = (uint32_t)atol(optarg); break;
      case 'n': num_incr   = (uint16_t)atoi(optarg); break;
      case 'w': wave_type  = atoi(optarg);           break;
      case 'm': mclk_mult  = atoi(optarg);           break;
      case 'i': interval_val  = (uint16_t)atoi(optarg); break;
      case 'g': positive_incr = false; break;
      case 'h': print_usage(argv[0]); return EXIT_SUCCESS;
      default:  print_usage(argv[0]); return EXIT_FAILURE;
    }
  }

  if (start_freq == 0) {
    fprintf(stderr, "错误: 必须指定起始频率(-s)。\n");
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  if (num_incr > 4095) {
    fprintf(stderr, "错误: 递增次数不能超过 4095。\n");
    return EXIT_FAILURE;
  }

  uint32_t final_freq = start_freq + (positive_incr ? 1 : -1) * delta_freq * num_incr;

  printf("\n==============================\n");
  printf(" AD5932 自动扫频参数\n");
  printf("==============================\n");
  printf("起始频率: %u Hz\n", start_freq);
  printf("频率步长: %s%u Hz\n", positive_incr ? "+" : "-", delta_freq);
  printf("递增次数: %u\n", num_incr);
  printf("最终频率: %u Hz\n", final_freq);
  printf("波形类型: %d\n", wave_type);
  printf("MCLK倍数: %d\n", mclk_mult);
  printf("间隔参数: %u\n", interval_val);
  if (delta_freq == 0) {
    printf("注意: 频率步长为 0，芯片将输出固定频率波形。\n");
  }
  printf("==============================\n\n");

  ad5932_init();

  ad5932_reset();
  printf("AD5932 已复位，进入内部扫频模式。\n");

  ad5932_set_start_frequency(start_freq);
  ad5932_set_delta_frequency(delta_freq, positive_incr);
  ad5932_set_number_of_increments(num_incr);
  ad5932_set_increment_interval(0, mclk_mult, interval_val);
  ad5932_set_waveform(wave_type);

  printf("AD5932 参数设置完成，开始扫频...\n");
  ad5932_start_sweep();

  while (!ad5932_is_sweep_done()) {
    usleep(1000); // 1ms 轮询等待
  }

  printf("扫频结束，准备复位。\n");
  ad5932_reset();
  ad5932_set_standby(false); // 退出待机，恢复初始状态

  printf("测试完成，SPI 已关闭。\n");

  return EXIT_SUCCESS;
}
