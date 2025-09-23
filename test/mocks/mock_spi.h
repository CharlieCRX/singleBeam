#ifndef MOCK_SPI_H
#define MOCK_SPI_H

#include <stdint.h>
#include <string.h>

// 声明 mock 函数
// 这些函数的签名与 spi_hal.h 中的真实函数完全相同
int spi_hal_init(void);
int spi_hal_write(const uint8_t *tx_buf, int len);
void spi_hal_close(void);

// 声明用于跟踪调用的全局变量
// 这些变量暴露给测试用例，以便它们可以检查函数是否被正确调用
extern int spi_hal_init_called;
extern int spi_hal_write_called;
extern int spi_hal_close_called;
extern int spi_hal_write_len;
extern uint8_t spi_hal_last_tx_buf[10];

// 声明一个重置函数，方便在每个测试用例开始时清除状态
void mock_spi_reset(void);

#endif // MOCK_SPI_H