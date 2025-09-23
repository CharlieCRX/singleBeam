#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h> // 引入 assert 头文件

// 引入需要测试的接口头文件
#include "../protocol/spi_hal.h"

// 引入模拟实现的头文件，以便访问全局状态
#include "mocks/mock_spi.h"

// 测试用例：验证 SPI 初始化函数
int test_spi_init(void) {
    mock_spi_reset();
    spi_hal_init();
    assert(spi_hal_init_called == 1);
    printf("PASS: %s\n", __FUNCTION__);
    return 0;
}

// 测试用例：验证 SPI 写入函数
int test_spi_write(void) {
    mock_spi_reset();
    uint8_t test_data[] = {0x12, 0x34};
    spi_hal_write(test_data, sizeof(test_data));

    assert(spi_hal_write_called == 1);
    assert(spi_hal_write_len == 2);
    assert(memcmp(spi_hal_last_tx_buf, test_data, 2) == 0);
    printf("PASS: %s\n", __FUNCTION__);
    return 0;
}


// 主测试运行器
int main(void) {
    printf("--- Running Protocol Layer Tests ---\n");
    test_spi_init();
    test_spi_write();
    printf("\nAll tests passed successfully!\n");
    return EXIT_SUCCESS;
}