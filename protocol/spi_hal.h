#ifndef SPI_HAL_H
#define SPI_HAL_H

#include <stdint.h>

/**
 * @brief 初始化SPI接口。
 * @return 成功返回0，失败返回-1。
 */
int spi_hal_init(void);

/**
 * @brief 向SPI总线写入数据。
 * @param tx_buf 待发送的数据缓冲区。
 * @param len 待发送的数据长度。
 * @return 成功返回写入的字节数，失败返回-1。
 */
int spi_hal_write(const uint8_t *tx_buf, int len);

/**
 * @brief 关闭SPI接口。
 */
void spi_hal_close(void);

#endif // SPI_HAL_H
