#include "mock_spi.h"
#include <stdio.h>
#include <string.h>

// 全局变量用于记录函数调用和参数
int spi_hal_init_called = 0;
int spi_hal_write_called = 0;
int spi_hal_close_called = 0;
int spi_hal_write_len = 0;
uint8_t spi_hal_last_tx_buf[10];

int spi_hal_init(void) {
    printf("Mock: spi_hal_init() called.\n");
    spi_hal_init_called = 1;
    // 模拟成功返回
    return 0;
}

int spi_hal_write(const uint8_t *tx_buf, int len) {
    printf("Mock: spi_hal_write() called with len=%d.\n", len);
    spi_hal_write_called = 1;
    spi_hal_write_len = len;
    // 将传入的数据复制到全局变量中，以便测试函数检查
    if (len <= 10) {
        memcpy(spi_hal_last_tx_buf, tx_buf, len);
    }
    // 模拟成功返回
    return len;
}

void spi_hal_close(void) {
    printf("Mock: spi_hal_close() called.\n");
    spi_hal_close_called = 1;
}

// 供测试使用的重置函数
void mock_spi_reset(void) {
    spi_hal_init_called = 0;
    spi_hal_write_called = 0;
    spi_hal_close_called = 0;
    spi_hal_write_len = 0;
    memset(spi_hal_last_tx_buf, 0, sizeof(spi_hal_last_tx_buf));
}