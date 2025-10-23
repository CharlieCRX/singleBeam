#include "i2c_hal.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <string.h>

static int i2c_fd = -1;

int i2c_hal_init(const char* i2c_bus) {
  i2c_fd = open(i2c_bus, O_RDWR);
  if (i2c_fd < 0) {
    perror("无法打开I2C设备");
    return -1;
  }
  return 0;
}

int i2c_hal_write_reg16(uint8_t dev_addr, uint8_t reg_addr, uint16_t value) {
  uint8_t buf[3];
  buf[0] = reg_addr;
  buf[1] = (uint8_t)(value >> 8);
  buf[2] = (uint8_t)(value & 0xFF);

  struct i2c_msg msg = {
    .addr = dev_addr,
    .flags = 0,
    .len = 3,
    .buf = buf
  };

  struct i2c_rdwr_ioctl_data ioctl_data = {
    .msgs = &msg,
    .nmsgs = 1
  };

  if (ioctl(i2c_fd, I2C_RDWR, &ioctl_data) < 0) {
    perror("I2C写入失败");
    return -1;
  }

  return 0;
}

int i2c_hal_read_reg16(uint8_t dev_addr, uint8_t reg_addr, uint16_t *value) {
  uint8_t buf[2];
  struct i2c_msg msgs[2];

  // 写寄存器地址
  msgs[0].addr  = dev_addr;
  msgs[0].flags = 0;
  msgs[0].len   = 1;
  msgs[0].buf   = &reg_addr;

  // 读两个字节
  msgs[1].addr  = dev_addr;
  msgs[1].flags = I2C_M_RD;
  msgs[1].len   = 2;
  msgs[1].buf   = buf;

  struct i2c_rdwr_ioctl_data ioctl_data = {
    .msgs = msgs,
    .nmsgs = 2
  };

  if (ioctl(i2c_fd, I2C_RDWR, &ioctl_data) < 0) {
    perror("I2C读取失败");
    return -1;
  }

  *value = ((uint16_t)buf[0] << 8) | buf[1];
  return 0;
}

void i2c_hal_close(void) {
  if (i2c_fd >= 0) {
    close(i2c_fd);
    i2c_fd = -1;
  }
}