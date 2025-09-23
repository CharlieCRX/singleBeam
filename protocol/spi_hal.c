#include "spi_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <string.h>

#define SPI_DEVICE "/dev/spidev3.0"

static int spi_fd = -1;

int spi_hal_init(void) {
  uint8_t mode = SPI_MODE_2;
  uint8_t bits = 8;
  uint32_t speed = 1000000;

  spi_fd = open(SPI_DEVICE, O_RDWR);
  if (spi_fd < 0) {
    perror("无法打开SPI设备");
    return -1;
  }

  if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0) {
    perror("无法设置SPI模式");
    close(spi_fd);
    return -1;
  }

  if (ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
    perror("无法设置SPI字长");
    close(spi_fd);
    return -1;
  }

  if (ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
    perror("无法设置SPI速度");
    close(spi_fd);
    return -1;
  }

  return 0;
}

int spi_hal_write(const uint8_t *tx_buf, int len) {
  struct spi_ioc_transfer tr;
  uint8_t rx_buf[len];
  memset(&tr, 0, sizeof(tr));
  tr.tx_buf = (unsigned long)tx_buf;
  tr.rx_buf = (unsigned long)rx_buf;
  tr.len = len;
  tr.delay_usecs = 0;
  tr.speed_hz = 1000000;
  tr.bits_per_word = 8;
  tr.cs_change = 0;

  if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
    perror("SPI传输失败");
    return -1;
  }
  usleep(10);
  return len;
}

void spi_hal_close(void) {
  if (spi_fd >= 0) {
    close(spi_fd);
    spi_fd = -1;
  }
}