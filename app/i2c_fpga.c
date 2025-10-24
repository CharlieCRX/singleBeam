#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>
#include "../protocol/i2c_hal.h"

static const char *i2c_dev = "/dev/i2c-2";
#define FPGA_I2C_SLAVE  (0x55)

static void usage(FILE *fp, int argc, char **argv)
{
  fprintf(fp,
    "Usage: %s [options]\n\n"
    "Options:\n"
    " -h | --help         Print this message\n"
    " -r | --read <reg>       Read FPGA register\n"
    " -w | --write <reg> <val>  Write FPGA register\n"
    "\nExample:\n"
    "  %s -r 0x0010\n"
    "  %s -w 0x0010 0x12345678\n",
    argv[0], argv[0], argv[0]);
  exit(1);
}

int main(int argc, char **argv)
{
  int opt;
  uint16_t reg = 0;
  uint32_t val = 0;
  int read_flag = 0, write_flag = 0;

  static struct option lopts[] = {
    {"help",  0, 0, 'h'},
    {"read",  1, 0, 'r'},
    {"write", 1, 0, 'w'},
    {NULL, 0, 0, 0}
  };

  if (argc < 2)
    usage(stdout, argc, argv);

  while ((opt = getopt_long(argc, argv, "hr:w:", lopts, NULL)) != -1) {
    switch (opt) {
    case 'h':
      usage(stdout, argc, argv);
      break;
    case 'r':
      read_flag = 1;
      reg = (uint16_t)strtol(optarg, NULL, 0);
      break;
    case 'w':
      write_flag = 1;
      if (optind + 1 > argc) {
        fprintf(stderr, "Missing value after write register.\n");
        usage(stderr, argc, argv);
      }
      reg = (uint16_t)strtol(optarg, NULL, 0);
      val = (uint32_t)strtol(argv[optind], NULL, 0);
      break;
    default:
      usage(stderr, argc, argv);
    }
  }

  if (i2c_hal_init(i2c_dev) < 0) {
    fprintf(stderr, "Failed to open I2C device: %s\n", i2c_dev);
    return -1;
  }

  if (read_flag) {
    uint32_t read_val = 0;
    if (i2c_hal_fpga_read(FPGA_I2C_SLAVE, reg, &read_val) == 0) {
      printf("FPGA [0x%04X] = 0x%08X\n", reg, read_val);
    } else {
      fprintf(stderr, "I2C read failed.\n");
    }
  }

  if (write_flag) {
    if (i2c_hal_fpga_write(FPGA_I2C_SLAVE, reg, val) == 0) {
      printf("FPGA [0x%04X] <= 0x%08X (write OK)\n", reg, val);
    } else {
      fprintf(stderr, "I2C write failed.\n");
    }
  }

  i2c_hal_close();
  return 0;
}
