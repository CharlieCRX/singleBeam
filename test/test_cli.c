#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include "../api/ad5932_api.h"

void print_usage(const char *prog_name) {
  printf("用法: %s [选项]\n", prog_name);
  printf("选项:\n");
  printf("  -f, --frequency <频率>  设置输出频率 (Hz), 默认: 1000.0\n");
  printf("  -w, --waveform <波形>   设置输出波形, 可选: sine, triangle, square, 默认: sine\n");
  printf("  -s, --sweep             配置LFM扫频模式\n");
  printf("  --start-freq <频率>     扫频起始频率 (Hz)\n");
  printf("  --end-freq <频率>       扫频结束频率 (Hz)\n");
  printf("  --steps <数量>          扫频步进数量\n");
  printf("  --dwell-time <时间>     每个频率驻留时间 (秒)\n");
  printf("  --use-cycles            使用输出周期数而不是时钟周期\n");
  printf("  --syncout <模式>        设置SYNCOUT输出(0=禁用, 1=脉冲模式, 2=扫描结束模式)\n");
  printf("  -h, --help              显示此帮助信息\n");
}

int main(int argc, char *argv[]) {
  double output_freq = 1000.0;
  char *waveform_str = "sine";
  int waveform_mode = 0; // 0=sine, 1=triangle, 2=square
  int setup_sweep = 0;
  double start_freq = 1000.0;
  double end_freq = 10000.0;
  uint16_t num_steps = 100;
  double dwell_time = 0.001; // 1ms
  int use_cycles = 0;
  int syncout_mode = 0; // 0=禁用, 1=脉冲模式, 2=扫描结束模式
  int c;

  // 定义长选项
  static struct option long_options[] = {
    {"frequency",   required_argument, 0, 'f'},
    {"waveform",    required_argument, 0, 'w'},
    {"sweep",       no_argument,       0, 's'},
    {"start-freq",  required_argument, 0, 0},
    {"end-freq",    required_argument, 0, 0},
    {"steps",       required_argument, 0, 0},
    {"dwell-time",  required_argument, 0, 0},
    {"use-cycles",  no_argument,       0, 0},
    {"syncout",     required_argument, 0, 0},
    {"help",        no_argument,       0, 'h'},
    {0, 0, 0, 0}
  };

  // 解析命令行参数
  int option_index = 0;
  while ((c = getopt_long(argc, argv, "f:w:sh", long_options, &option_index)) != -1) {
    switch (c) {
      case 0:
        if (strcmp(long_options[option_index].name, "start-freq") == 0) {
          start_freq = atof(optarg);
          setup_sweep = 1;
        } else if (strcmp(long_options[option_index].name, "end-freq") == 0) {
          end_freq = atof(optarg);
          setup_sweep = 1;
        } else if (strcmp(long_options[option_index].name, "steps") == 0) {
          num_steps = atoi(optarg);
          setup_sweep = 1;
        } else if (strcmp(long_options[option_index].name, "dwell-time") == 0) {
          dwell_time = atof(optarg);
          setup_sweep = 1;
        } else if (strcmp(long_options[option_index].name, "use-cycles") == 0) {
          use_cycles = 1;
          setup_sweep = 1;
        } else if (strcmp(long_options[option_index].name, "syncout") == 0) {
          syncout_mode = atoi(optarg);
        }
        break;
      case 'f':
        output_freq = atof(optarg);
        break;
      case 'w':
        waveform_str = optarg;
        if (strcmp(waveform_str, "sine") == 0) {
          waveform_mode = 0;
        } else if (strcmp(waveform_str, "triangle") == 0) {
          waveform_mode = 1;
        } else if (strcmp(waveform_str, "square") == 0) {
          waveform_mode = 2;
        } else {
          fprintf(stderr, "错误: 无效的波形类型 '%s'. 默认使用正弦波.\n", waveform_str);
          waveform_mode = 0;
        }
        break;
      case 's':
        setup_sweep = 1;
        break;
      case 'h':
        print_usage(argv[0]);
        return EXIT_SUCCESS;
      case '?':
        // getopt_long 已经打印了错误信息
        print_usage(argv[0]);
        return EXIT_FAILURE;
      default:
        break;
    }
  }

  printf("AD5932 可编程频率扫描波形发生器\n");
  
  ad5932_reset();
  printf("AD5932复位完成\n");
  
  // 设置SYNCOUT输出
  if (syncout_mode > 0) {
    ad5932_set_syncout(1, syncout_mode == 2);
    printf("设置SYNCOUT输出: %s\n", syncout_mode == 2 ? "扫描结束模式" : "脉冲模式");
  }
  
  if (setup_sweep) {
    printf("配置LFM扫频: 起始频率=%.2f Hz, 结束频率=%.2f Hz, 步数=%d, 驻留时间=%.4f秒\n", 
         start_freq, end_freq, num_steps, dwell_time);
    ad5932_setup_lfm_sweep(start_freq, end_freq, num_steps, dwell_time, use_cycles);
    printf("LFM扫频配置完成\n");
  } else {
    ad5932_set_start_frequency(output_freq);
    printf("设置频率: %.2f Hz\n", output_freq);
  }
  
  ad5932_set_waveform(waveform_mode);
  printf("设置为 %s 输出模式\n", waveform_str);
  
  printf("按 Enter 键启动扫描...\n");
  getchar();
  
  ad5932_start_sweep();
  printf("扫描已启动\n");
  
  printf("按 Enter 键退出...\n");
  getchar();
  
  close(spi_fd);
  printf("程序结束\n");
  
  return EXIT_SUCCESS;
}