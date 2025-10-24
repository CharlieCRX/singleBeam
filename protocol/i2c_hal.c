#include "i2c_hal.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <string.h>
#include <arpa/inet.h>

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


/**
 * @brief 向I2C设备写32位寄存器（FPGA专用）
 * @param dev_addr I2C设备地址
 * @param reg_addr 16位寄存器地址
 * @param val 32位数据
 * @return 成功返回0，失败返回-1
 */
int i2c_hal_fpga_write(uint8_t dev_addr, uint16_t reg_addr, uint32_t val)
{
  uint8_t buf[6];
  struct i2c_msg msg;
  struct i2c_rdwr_ioctl_data data;

  buf[0] = (uint8_t)((reg_addr >> 8) & 0xFF);
  buf[1] = (uint8_t)(reg_addr & 0xFF);
  buf[2] = (uint8_t)((val >> 24) & 0xFF);
  buf[3] = (uint8_t)((val >> 16) & 0xFF);
  buf[4] = (uint8_t)((val >> 8) & 0xFF);
  buf[5] = (uint8_t)(val & 0xFF);

  msg.addr  = dev_addr;
  msg.flags = 0;
  msg.len   = sizeof(buf);
  msg.buf   = buf;

  data.msgs  = &msg;
  data.nmsgs = 1;

  return ioctl(i2c_fd, I2C_RDWR, &data) < 0 ? -1 : 0;
}


/**
 * @brief 从I2C设备读取32位寄存器（FPGA专用）
 * @param dev_addr I2C设备地址
 * @param reg_addr 16位寄存器地址
 * @param val 指向存储32位数据的指针
 * @return 成功返回0，失败返回-1
 */
int i2c_hal_fpga_read(uint8_t dev_addr, uint16_t reg_addr, uint32_t *val)
{
  uint8_t buf[2];
  struct i2c_msg msgs[2];
  struct i2c_rdwr_ioctl_data data;

  buf[0] = (uint8_t)((reg_addr >> 8) & 0xFF) | 0x80; // 高位标记读
  buf[1] = (uint8_t)(reg_addr & 0xFF);

  msgs[0].addr  = dev_addr;
  msgs[0].flags = 0;
  msgs[0].len   = sizeof(buf);
  msgs[0].buf   = buf;

  msgs[1].addr  = dev_addr;
  msgs[1].flags = I2C_M_RD;
  msgs[1].len   = sizeof(uint32_t);
  msgs[1].buf   = (uint8_t*)val;

  data.msgs  = msgs;
  data.nmsgs = 2;

  if(ioctl(i2c_fd, I2C_RDWR, &data) < 0)
      return -1;

  *val = ntohl(*val); // 转为主机字节序
  return 0;
}

void i2c_hal_close(void) {
  if (i2c_fd >= 0) {
    close(i2c_fd);
    i2c_fd = -1;
  }
}