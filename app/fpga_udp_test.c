#include "../dev/fpga.h"
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define SPI_DEVICE "/dev/spidev3.0"
static const char *i2c_dev = "/dev/i2c-2";
int main() {
  S_udp_header_params params = {
    .dst_mac_high = 0xb07b,
    .dst_mac_low  = 0x2500c019, // b0:7b:25:00:c0:19
    .src_mac_high = 0x0043,
    .src_mac_low  = 0x4d494e40, // 00:43:4D:49:4E:40
    .src_ip       = 0xa9fe972f, // 169.254.151.47
    .dst_ip       = 0xc0a80105, // 192.168.1.5
    .src_port     = 5000,
    .dst_port     = 5030,
    .ip_total_len = 0x041c, // 不生效
    .udp_data_len = 0x408
  };

  fpga_init(i2c_dev);

  fpga_set_acq_enable(false);

  fpga_initialize_udp_header(&params);

  printf("FPGA UDP头初始化完成。\n");

}