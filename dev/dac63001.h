#ifndef DAC63001_H
#define DAC63001_H

#include <stdint.h>
#include <stdbool.h>

// DAC63001 I2C地址
#define DAC63001_I2C_ADDR   0x48

// DAC 寄存器地址
#define COMMON_CONFIG_REG       0x1F
#define DAC_0_VOUT_CMP_CONFIG   0x15
#define DAC_0_DATA_REG          0x1C
#define COMMON_TRIGGER_REG      0x20
#define DAC_0_MARGIN_HIGH       0x13
#define DAC_0_MARGIN_LOW        0x14
#define DAC_0_FUNC_CONFIG       0x18
#define COMMON_DAC_TRIG         0x21

// 外部参考电压值（单位：伏）
#define DAC63001_EXT_REF_VOLTAGE         1.25f
#define DAC63001_CODE_MAX                4096  // 12位DAC最大代码值

// 波形类型
typedef enum {
    DAC63001_WAVEFORM_NONE = 0,
    DAC63001_WAVEFORM_FIXED,
    DAC63001_WAVEFORM_SAWTOOTH,
} dac63001_waveform_t;

// Slew Rate 配置
typedef enum {
    DAC63001_SLEW_IMMEDIATE = 0,
    DAC63001_SLEW_4US,
    DAC63001_SLEW_8US,
    DAC63001_SLEW_12US,
    DAC63001_SLEW_18US,
    DAC63001_SLEW_27US,
    DAC63001_SLEW_40US,
    DAC63001_SLEW_61US,
    DAC63001_SLEW_91US,
    DAC63001_SLEW_137US,
    DAC63001_SLEW_239US,
    DAC63001_SLEW_419US,
    DAC63001_SLEW_733US,
    DAC63001_SLEW_1282US,
    DAC63001_SLEW_2564US,
    DAC63001_SLEW_5128US
} dac63001_slew_rate_t;

// Code Step 配置
typedef enum {
    DAC63001_STEP_1LSB = 0,
    DAC63001_STEP_2LSB,
    DAC63001_STEP_3LSB,
    DAC63001_STEP_4LSB,
    DAC63001_STEP_6LSB,
    DAC63001_STEP_8LSB,
    DAC63001_STEP_16LSB,
    DAC63001_STEP_32LSB
} dac63001_code_step_t;

/**
 * @brief 初始化DAC63001驱动
 * @param i2c_bus I2C总线设备路径
 */
void dac63001_init(const char* i2c_bus);

/**
 * @brief 配置DAC63001为外部参考电压模式
 */
int dac63001_setup_external_ref(void);

/**
 * @brief 设置固定电压输出
 * @param voltage 输出电压值 (0-1.25V)
 */
int dac63001_set_fixed_voltage(float voltage);

/**
 * @brief 配置反向锯齿波生成
 * @param min_voltage 最小电压
 * @param max_voltage 最大电压
 * @param code_step 代码步长
 * @param slew_rate Slew rate配置
 */
int dac63001_setup_sawtooth_wave(float min_voltage, float max_voltage, 
                                dac63001_code_step_t code_step, 
                                dac63001_slew_rate_t slew_rate);
/**
 * @brief 开始DAC函数生成
 */
int dac63001_start_waveform(void);

/**
 * @brief 停止DAC函数生成
 */
int dac63001_stop_waveform(void);

/**
 * @brief 关闭DAC63001驱动
 */
void dac63001_close(void);

// ===== AD8338 增益控制相关 =====
// AD8338 增益控制参数
#define AD8338_GAIN_MIN_DB      0      // 最小增益 0dB
#define AD8338_GAIN_MAX_DB      80     // 最大增益 80dB

#define AD8338_VGAIN_MIN_V      1.1f   // 最小增益对应的电压（0dB -> 1.1V）
#define AD8338_VGAIN_MAX_V      0.1f   // 最大增益对应的电压（80dB -> 0.1V）

// 根据具体电阻值计算的增益偏移
#define AD8338_R_FEEDBACK       9500.0f  // 反馈电阻 9500Ω
#define AD8338_R_N              500.0f   // 输入电阻 500Ω

/**
 * @brief 设置AD8338增益扫描
 * @param start_gain 起始增益 (0-80 dB)
 * @param end_gain 结束增益 (0-80 dB)  
 * @param gain_duration_us 增益变化持续时间 (微秒，最大5秒)
 */
int dac63001_set_gain_sweep(uint16_t start_gain, uint16_t end_gain, uint32_t gain_duration_us);

/**
 * @brief 增益值转换为电压值（使用具体电阻值）
 * @param gain_dB 增益值 (dB)
 * @return 对应的控制电压 (V)
 */
float ad8338_gain_to_voltage(uint16_t gain_dB);

/**
 * @brief 电压值转换为增益值（使用具体电阻值）
 * @param voltage 控制电压 (V)
 * @return 对应的增益值 (dB)
 */
uint16_t ad8338_voltage_to_gain(float voltage);

/**
 * @brief 计算电阻比值对应的增益偏移
 * @return 增益偏移值 (dB)
 */
float calculate_gain_offset(void);

#endif // DAC63001_H