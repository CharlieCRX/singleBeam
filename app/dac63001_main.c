#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "../dev/dac63001.h"

// 全局变量存储命令行参数
static struct {
  dac63001_waveform_t mode;
  float voltage;
  float min_voltage;
  float max_voltage;
  dac63001_code_step_t code_step;
  dac63001_slew_rate_t slew_rate;
} g_args = {
  .mode = DAC63001_WAVEFORM_NONE,
  .voltage = 0.0f,
  .min_voltage = 0.1f,
  .max_voltage = 1.0f,
  .code_step = DAC63001_STEP_32LSB,
  .slew_rate = DAC63001_SLEW_12US
};

void print_usage(const char *program_name)
{
  printf("Usage: %s [OPTIONS]\n\n", program_name);
  printf("DAC63001 控制程序 - 支持固定电压输出和波形生成\n\n");
  printf("Options:\n");
  printf("  -v, --voltage VOLTAGE    设置固定输出电压 (0-%.2fV)\n", DAC63001_EXT_REF_VOLTAGE);
  printf("  -w, --waveform           启用反向锯齿波生成\n");
  printf("  -x, --stop               停止波形生成\n");
  printf("  --min MIN_VOLTAGE        锯齿波最小电压 (默认: 0.1V)\n");
  printf("  --max MAX_VOLTAGE        锯齿波最大电压 (默认: 1.0V)\n");
  printf("  --step CODE_STEP         代码步长 0-7 (默认: 7 - 32 LSB)\n");
  printf("                           0:1LSB, 1:2LSB, 2:3LSB, 3:4LSB\n");
  printf("                           4:6LSB, 5:8LSB, 6:16LSB, 7:32LSB\n");
  printf("  --slew SLEW_RATE         Slew rate 0-15 (默认: 3 - 12µs)\n");
  printf("                           0:立即变化, 1:4µs, 2:8µs, 3:12µs, 4:18µs, 5:27µs\n");
  printf("                           6:40µs, 7:61µs, 8:91µs, 9:137µs, 10:239µs, 11:419µs\n");
  printf("                           12:733µs, 13:1282µs, 14:2564µs, 15:5128µs\n");
  printf("  -h, --help               显示此帮助信息\n\n");
  printf("Examples:\n");
  printf("  %s -v 0.5              # 输出0.5V固定电压\n", program_name);
  printf("  %s -w                  # 启用反向锯齿波\n", program_name);
  printf("  %s -w --min 0.2 --max 0.8 --step 5 --slew 7\n", program_name);
  printf("  %s -x                  # 停止波形生成\n", program_name);
}

int parse_arguments(int argc, char *argv[]) {
  static struct option long_options[] = {
    {"voltage", required_argument, 0, 'v'},
    {"waveform", no_argument, 0, 'w'},
    {"stop", no_argument, 0, 'x'},
    {"min", required_argument, 0, 0},
    {"max", required_argument, 0, 0},
    {"step", required_argument, 0, 0},
    {"slew", required_argument, 0, 0},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
  };

  int c;
  int option_index = 0;
  int temp_step, temp_slew;

  while ((c = getopt_long(argc, argv, "v:wxh", long_options, &option_index)) != -1) {
    switch (c) {
      case 'v':
        g_args.mode = DAC63001_WAVEFORM_FIXED;
        g_args.voltage = atof(optarg);
        if (g_args.voltage < 0 || g_args.voltage > DAC63001_EXT_REF_VOLTAGE) {
          fprintf(stderr, "错误: 电压必须在 0-%.2fV 范围内\n", DAC63001_EXT_REF_VOLTAGE);
          return -1;
        }
        break;

      case 'w':
        g_args.mode = DAC63001_WAVEFORM_SAWTOOTH;
        break;

      case 'x':
        g_args.mode = DAC63001_WAVEFORM_NONE;
        break;

      case 0:
        if (strcmp(long_options[option_index].name, "min") == 0) {
          g_args.min_voltage = atof(optarg);
        } else if (strcmp(long_options[option_index].name, "max") == 0) {
          g_args.max_voltage = atof(optarg);
        } else if (strcmp(long_options[option_index].name, "step") == 0) {
          temp_step = atoi(optarg);
          if (temp_step < 0 || temp_step > 7) {
            fprintf(stderr, "错误: 代码步长必须在 0-7 范围内\n");
            return -1;
          }
          g_args.code_step = (dac63001_code_step_t)temp_step;
        } else if (strcmp(long_options[option_index].name, "slew") == 0) {
          temp_slew = atoi(optarg);
          if (temp_slew < 0 || temp_slew > 15) {
            fprintf(stderr, "错误: Slew rate 必须在 0-15 范围内\n");
            return -1;
          }
          g_args.slew_rate = (dac63001_slew_rate_t)temp_slew;
        }
        break;

      case 'h':
        print_usage(argv[0]);
        exit(0);

      case '?':
        return -1;

      default:
        break;
    }
  }

  if (g_args.mode == DAC63001_WAVEFORM_NONE && optind == 1) {
    fprintf(stderr, "错误: 必须指定操作模式\n");
    return -1;
  }

  if (g_args.mode == DAC63001_WAVEFORM_SAWTOOTH) {
    if (g_args.min_voltage >= g_args.max_voltage) {
      fprintf(stderr, "错误: 最小电压必须小于最大电压\n");
      return -1;
    }
    if (g_args.max_voltage > DAC63001_EXT_REF_VOLTAGE) {
      fprintf(stderr, "错误: 最大电压不能超过参考电压 %.2fV\n", DAC63001_EXT_REF_VOLTAGE);
      return -1;
    }
  }

  return 0;
}

int main(int argc, char *argv[]) {
  int ret;
  
  printf("DAC63001 控制程序 - 外部参考电压: %.2fV\n", DAC63001_EXT_REF_VOLTAGE);
  
  // 解析命令行参数
  if (parse_arguments(argc, argv) < 0) {
    print_usage(argv[0]);
    return -1;
  }
  
  // 初始化DAC63001
  dac63001_init("/dev/i2c-2");
  
  // 配置外部参考模式
  ret = dac63001_setup_external_ref();
  if (ret < 0) {
    printf("DAC配置失败\n");
    dac63001_close();
    return -1;
  }
  
  // 根据模式执行相应操作
  switch (g_args.mode) {
    case DAC63001_WAVEFORM_FIXED:
      ret = dac63001_set_fixed_voltage(g_args.voltage);
      break;
      
    case DAC63001_WAVEFORM_SAWTOOTH:
      printf("锯齿波参数:\n");
      printf("  最小电压: %.3fV\n", g_args.min_voltage);
      printf("  最大电压: %.3fV\n", g_args.max_voltage);
      printf("  代码步长: %d\n", g_args.code_step);
      printf("  Slew rate: %d\n", g_args.slew_rate);
      
      ret = dac63001_setup_sawtooth_wave(g_args.min_voltage, g_args.max_voltage,
                       g_args.code_step, g_args.slew_rate);
      break;
      
    case DAC63001_WAVEFORM_NONE:
      ret = dac63001_stop_waveform();
      break;
      
    default:
      fprintf(stderr, "错误: 未知的操作模式\n");
      ret = -1;
      break;
  }
  
  if (ret < 0) {
    printf("操作执行失败\n");
  } else {
    printf("操作执行成功\n");
  }
  
  dac63001_close();
  return ret;
}