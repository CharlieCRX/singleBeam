#include "ad5932_driver.h"
#include "../protocol/spi_hal.h"
#include <stdint.h>

/**
 * @brief 初始化AD5932驱动
 */
void ad5932_init(void) {
  spi_hal_init();
}

/**
 * @brief 将16位数据写入AD5932寄存器。
 * @param data 要写入的16位数据。
 */
void ad5932_write(uint16_t data) {
  uint8_t tx_buf[2];

  // 将16位数据拆分为两个8位字节（高位在前）
  tx_buf[0] = (data >> 8) & 0xFF; // MSB
  tx_buf[1] = data & 0xFF;        // LSB

  // 调用协议层的SPI写入函数
  spi_hal_write(tx_buf, 2);
}