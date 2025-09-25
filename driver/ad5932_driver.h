#ifndef AD5932_DRIVER_H
#define AD5932_DRIVER_H

#include <stdint.h>

/**
 * @brief 初始化AD5932驱动
 */
void ad5932_init(void);

/**
 * @brief 将16位数据写入AD5932寄存器。
 * @param data 要写入的16位数据。
 */
void ad5932_write(uint16_t data);

#endif // AD5932_DRIVER_H