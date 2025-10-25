#include "i2c_hal.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>

static int i2c_fd = -1;

int i2c_hal_init(const char* i2c_bus) {
  if (i2c_fd >= 0) {
    // 已初始化
    return 0;
  }
  
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
 * @brief 向FPGA设备写4字节寄存器（FPGA专用）
 *
 * @param fpga_addr I2C设备地址
 * @param reg_addr 16位寄存器地址
 * @param val 指向4字节数据的指针
 * @return 成功返回0，失败返回-1
 */
int fpga_reg_write_4Bytes(uint8_t fpga_addr, int16_t reg_addr, uint8_t* val) {
  uint8_t buf[6] = {0};
	struct i2c_rdwr_ioctl_data data;
	struct i2c_msg msg;
  struct timeval _time_start_;
  struct timeval _time_end_; 
	int64_t timespace = 0;

  gettimeofday(&_time_start_,NULL);
  
	buf[0] = (uint8_t)((reg_addr>>8)&0x00FF); 
	buf[1] = (uint8_t)((reg_addr)&0x00FF);
	
	memcpy(buf+2, val, 4);
	
	msg.addr = (uint8_t)fpga_addr;
	msg.flags = 0; 
	msg.len = (uint8_t)sizeof(buf);
	msg.buf = (char *)buf;
	
	data.msgs = &msg;
	data.nmsgs = 1;

	if(ioctl(i2c_fd, I2C_RDWR, &data)<0){
		perror("ioctl i2c-w:");
    printf("I2C ioctl write failed\n");
		return -1;
	}

  gettimeofday(&_time_end_,NULL);
  timespace = (_time_end_.tv_sec - _time_start_.tv_sec)*1000000 + _time_end_.tv_usec - _time_start_.tv_usec;
  if(timespace>(int64_t)100000){
    printf("reg(0x%x) write slowly! timeuse=%lld us", reg_addr, timespace);
  }

	return 0;
}

/**
 * @brief 向FPGA设备写32位寄存器（FPGA专用）
 * @param fpga_addr I2C设备地址
 * @param reg_addr 16位寄存器地址
 * @param val 32位数据
 * @return 成功返回0，失败返回-1
 */
int i2c_hal_fpga_write(uint8_t fpga_addr, uint16_t reg_addr, uint32_t val)
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

  msg.addr  = fpga_addr;
  msg.flags = 0;
  msg.len   = sizeof(buf);
  msg.buf   = buf;

  data.msgs  = &msg;
  data.nmsgs = 1;

  return ioctl(i2c_fd, I2C_RDWR, &data) < 0 ? -1 : 0;
}


/**
 * @brief 从FPGA设备读取32位寄存器（FPGA专用）
 * @param fpga_addr I2C设备地址
 * @param reg_addr 16位寄存器地址
 * @param val 指向存储32位数据的指针
 * @return 成功返回0，失败返回-1
 */
int i2c_hal_fpga_read(uint8_t fpga_addr, uint16_t reg_addr, uint32_t *val)
{
  uint8_t buf[2];
  struct i2c_msg msgs[2];
  struct i2c_rdwr_ioctl_data data;

  buf[0] = (uint8_t)((reg_addr >> 8) & 0xFF) | 0x80; // 高位标记读
  buf[1] = (uint8_t)(reg_addr & 0xFF);

  msgs[0].addr  = fpga_addr;
  msgs[0].flags = 0;
  msgs[0].len   = sizeof(buf);
  msgs[0].buf   = buf;

  msgs[1].addr  = fpga_addr;
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