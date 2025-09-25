#include "mock_ad5932_driver.h"
#include <stdio.h>
#include <string.h>

// 全局变量定义
bool ad5932_write_called = false;
bool ad5932_init_called = false;
uint16_t ad5932_last_written_data = 0;
uint16_t ad5932_write_call_count = 0;
uint16_t ad5932_write_call_history[20] = {0}; // 假设最多记录20次写入


// 模拟函数实现
void ad5932_write(uint16_t data) {
    printf("Mock: ad5932_write() called with data=0x%04X.\n", data);
    ad5932_write_called = true;
    ad5932_last_written_data = data;
    ad5932_write_call_history[ad5932_write_call_count++] = data;
}

void ad5932_init(void) {
    printf("Mock: ad5932_init() called.\n");
    ad5932_init_called = true;
}

// 重置函数实现
void mock_ad5932_driver_reset(void) {
    ad5932_write_called = false;
    ad5932_init_called = false;
    ad5932_last_written_data = 0;
    ad5932_write_call_count = 0;
    memset(ad5932_write_call_history, 0, sizeof(ad5932_write_call_history));
}