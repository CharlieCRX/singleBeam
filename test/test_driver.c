#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// 引入要测试的驱动层头文件
#include "../driver/ad5932_driver.h"

// 引入模拟实现的头文件，以便访问全局状态
#include "mocks/mock_spi.h"


void test_ad5932_init(void) {
  mock_spi_reset(); // 重置 SPI 模拟状态
  ad5932_init();

  assert(spi_hal_write_called == 0); // 初始化不会调用 SPI 写操作
  assert(spi_hal_init_called == 1); // 应该调用 SPI 初始化
  printf("PASS: %s\n", __FUNCTION__);
}

// 测试用例：验证 ad5932_driver_write 函数
void test_ad5932_driver_write(void) {
  mock_spi_reset(); // 重置 SPI 模拟状态  
  uint16_t test_data = 0x1234;

  ad5932_write(test_data);  
  // 验证 spi_hal_write 是否被调用
  assert(spi_hal_write_called == 1);

  // 验证传递的字节数是否正确
  assert(spi_hal_write_len == 2);

  // 验证传递的数据是否正确
  assert(spi_hal_last_tx_buf[0] == 0x12);
  assert(spi_hal_last_tx_buf[1] == 0x34);
  printf("PASS: %s\n", __FUNCTION__);
}

// 主测试运行器
int main(void) {
  printf("--- Running Driver Layer Tests ---\n");
  test_ad5932_driver_write();
  printf("\nAll driver tests passed successfully!\n");
  return EXIT_SUCCESS;
}