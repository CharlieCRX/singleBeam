#ifndef MOCK_AD5932_DRIVER_H
#define MOCK_AD5932_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

// 声明模拟函数
// 签名与 ad5932_driver.h 中的真实函数完全相同
void ad5932_init(void);
void ad5932_write(uint16_t data);

// 声明用于跟踪调用的全局变量
// 这些变量暴露给测试用例，以便它们可以检查函数是否被正确调用
extern bool     ad5932_write_called;
extern bool     ad5932_init_called;
extern uint16_t ad5932_last_written_data;
extern uint16_t ad5932_write_call_count;
extern uint16_t ad5932_write_call_history[20]; // 假设最多跟踪20次调用

// 声明一个重置函数，方便在每个测试用例开始时清除状态
void mock_ad5932_driver_reset(void);

#endif // MOCK_AD5932_DRIVER_H