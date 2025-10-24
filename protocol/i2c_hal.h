#ifndef I2C_HAL_H
#define I2C_HAL_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 初始化I2C接口。
 * @param i2c_bus I2C总线设备路径，如"/dev/i2c-2"
 * @return 成功返回0，失败返回-1。
 */
int i2c_hal_init(const char* i2c_bus);

/**
 * @brief 向I2C设备写入16位寄存器数据。
 * @param dev_addr 设备地址
 * @param reg_addr 寄存器地址
 * @param value 16位数据值
 * @return 成功返回0，失败返回-1。
 */
int i2c_hal_write_reg16(uint8_t dev_addr, uint8_t reg_addr, uint16_t value);

/**
 * @brief 从I2C设备读取16位寄存器数据。
 * @param dev_addr 设备地址
 * @param reg_addr 寄存器地址
 * @param value 指向存储读取数据的指针
 * @return 成功返回0，失败返回-1。
 */
int i2c_hal_read_reg16(uint8_t dev_addr, uint8_t reg_addr, uint16_t *value);



/**
 * @brief 向FPGA设备写32位寄存器（FPGA专用）
 * @param fpga_addr FPGA设备地址
 * @param reg_addr 16位寄存器地址
 * @param val 32位数据
 * @return 成功返回0，失败返回-1
 */
int i2c_hal_fpga_write(uint8_t fpga_addr, uint16_t reg_addr, uint32_t val);

/**
 * @brief 从FPGA设备读取32位寄存器（FPGA专用）
 * @param fpga_addr FPGA设备地址
 * @param reg_addr 16位寄存器地址
 * @param val 指向存储32位数据的指针
 * @return 成功返回0，失败返回-1
 */
int i2c_hal_fpga_read(uint8_t fpga_addr, uint16_t reg_addr, uint32_t *val);


/**
 * @brief 关闭I2C接口。
 */
void i2c_hal_close(void);

#endif // I2C_HAL_H