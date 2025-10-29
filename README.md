# transmit_and_receive_single_beam_with_cache 函数文档

## 函数概述

`transmit_and_receive_single_beam_with_cache` 是 sBeam 库的核心函数，负责执行完整的单波束信号发射与同步接收数据采集流程。该函数集成了 DDS 信号生成、DAC 增益控制、FPGA 数据采集和网络数据传输功能，支持实时回调和缓存回调两种数据处理模式。

## 函数原型

```c
int transmit_and_receive_single_beam_with_cache(
    const DDSConfig *cfg,
    uint16_t start_gain,
    uint16_t end_gain,
    uint32_t gain_duration_us,
    NetPacketCallback packet_cb,
    NetCacheCallback cache_cb,
    uint32_t cache_size
);
```

## 参数详解

### 输入参数

#### `cfg` - DDS 扫频配置
**类型**: `const DDSConfig *` 

**描述**: 指向 DDS 配置结构体的指针，控制信号生成的各项参数。

**DDSConfig 结构体成员**:
```c
typedef struct {
    uint32_t start_freq;     // 起始频率 (Hz)
    uint32_t delta_freq;     // 频率递增步长 (Hz)，0 表示固定频率
    uint16_t num_incr;       // 频率递增次数 (2~4095)
    uint8_t  wave_type;      // 波形类型 (0=正弦波, 1=三角波, 2=方波)
    uint8_t  mclk_mult;      // MCLK 乘数 (0=1x, 1=5x, 2=100x, 3=500x)
    uint16_t interval_val;   // 每个频率的持续周期 T (2~2047)
    bool     positive_incr;  // 扫频方向 (true=正向, false=负向)
} DDSConfig;
```

#### `start_gain` - 起始增益值
**类型**: `uint16_t`  

**单位**: dB  

**范围**: 0-80 dB  

**约束**: 不能大于结束增益值  

**说明**: 若与 `end_gain` 相同，则启用固定增益模式

#### `end_gain` - 结束增益值
**类型**: `uint16_t`

**单位**: dB  

**范围**: 0-80 dB  

**说明**: 若与 `start_gain` 不同，则启用锯齿波扫描模式

#### `gain_duration_us` - 增益扫描持续时间
**类型**: `uint32_t`  

**单位**: 微秒 (μs)  

**范围**: 1000 ~ 250000 μs (1ms ~ 250ms)

#### `packet_cb` - 实时数据包回调函数
**类型**: `NetPacketCallback`  

**定义**: 

```c
typedef void (*NetPacketCallback)(const uint8_t *data, int length);
```
**参数**:
- `data`: 指向接收到的数据包缓冲区的指针
- `length`: 数据包长度（字节）

**说明**: 
- 可选参数，可以为 `NULL`
- 每接收到一个网络包立即触发
- 在缓存模式下，此回调依然会被调用，但缓存数据优先

#### `cache_cb` - 缓存数据回调函数
**类型**: `NetCacheCallback`  

**定义**:

```c
typedef void (*NetCacheCallback)(
    const uint8_t *cache_data, 
    uint32_t total_packets, 
    uint64_t total_bytes, 
    const uint32_t *packet_lengths
);
```
**参数**:
- `cache_data`: 指向缓存数据的指针
- `total_packets`: 总包数
- `total_bytes`: 总字节数
- `packet_lengths`: 指向各包长度数组的指针

**说明**: 
- 必须参数，不能为 `NULL`
- 在测试结束后统一调用，处理所有缓存数据

#### `cache_size` - 缓存区大小
**类型**: `uint32_t`  

**单位**: 字节  

**最大值**: 50 × 1024 × 1024 (50 MB)  

**说明**: 仅在 `cache_cb` 不为空时生效

## 返回值

**类型**: `int`  

**返回值说明**:

- `0`: 收发流程执行成功
- `-1`: 任意初始化、配置或启动阶段失败

## 函数执行流程

### 硬件控制流程

#### 1. 初始化阶段
- **FPGA 初始化**: 配置 UDP/IP 头部信息，设置数据包格式
- **DDS 配置**: 初始化 AD5932 芯片，设置扫频参数
- **DAC 配置**: 初始化 DAC63001 芯片，设置外部参考模式

#### 2. 增益模式选择
- **固定增益模式** (`start_gain == end_gain`):
  - 直接设置固定电压进行信号接收
  - 不启用 FPGA 的触发锯齿波增益波形
- **扫描增益模式** (`start_gain != end_gain`):
  - 配置锯齿波增益扫描参数
  - 启用 GPIO 触发同步增益扫描开始

#### 3. 数据采集启动
- **网络监听**: 启动实时或缓存模式网络监听
- **FPGA 使能**: 启动 FPGA 数据采集和网络包发送
- **DDS 启动**: 开始扫频信号生成

#### 4. 同步控制
- **扫频等待**: 轮询等待 DDS 扫频完成
- **增益触发**: 扫频完成后**自动触发** DAC 增益波形输出
- **时间同步**: 精确控制增益扫描持续时间

#### 5. 收尾工作
- **停止采集**: 关闭 FPGA 数据采集
- **触发回调**: 停止网络监听，触发缓存数据回调
- **资源清理**: 重置硬件状态，释放资源

### 数据处理流程

```
网络数据包 → 实时回调 (可选) → 缓存区 → 缓存回调 (必须)
```

## 使用示例

### 基本用法（仅缓存模式）

```c
#include "sbeam.h"

// 缓存回调函数
void my_cache_callback(const uint8_t *cache_data, uint32_t total_packets, 
                      uint64_t total_bytes, const uint32_t *packet_lengths) {
    printf("收到 %u 个数据包，总数据量 %lu 字节\n", total_packets, total_bytes);
    // 处理缓存数据...
}

int main() {
    // 配置 DDS 参数
    DDSConfig cfg = {
        .start_freq = 100000,     // 100 kHz
        .delta_freq = 50000,      // 50 kHz 步长
        .num_incr = 5,            // 5次递增
        .wave_type = 2,           // 方波
        .mclk_mult = 0,           // 1倍 MCLK
        .interval_val = 2,        // 每个频率2个周期
        .positive_incr = true     // 正向扫频
    };
    
    // 调用收发函数
    int result = transmit_and_receive_single_beam_with_cache(
        &cfg,
        0,                        // 起始增益 0 dB
        60,                       // 结束增益 60 dB
        50000,                    // 增益扫描 50ms
        NULL,                     // 无实时回调
        my_cache_callback,        // 缓存回调
        10 * 1024 * 1024         // 10MB 缓存
    );
    
    if (result == 0) {
        printf("测试成功完成\n");
    } else {
        printf("测试失败\n");
    }
    
    return 0;
}
```

### 混合模式（实时 + 缓存）

```c
// 实时回调函数
void my_packet_callback(const uint8_t *data, int length) {
    printf("实时数据包: %d 字节\n", length);
    // 实时处理数据...
}

// 调用时同时启用实时和缓存
int result = transmit_and_receive_single_beam_with_cache(
    &cfg, 0, 60, 50000,
    my_packet_callback,      // 实时回调
    my_cache_callback,       // 缓存回调
    10 * 1024 * 1024
);
```

### 固定增益模式

```c
// 固定增益，不进行增益扫描
int result = transmit_and_receive_single_beam_with_cache(
    &cfg,
    30, 30, 0,              // 起始和结束增益相同，启用固定模式
    NULL, my_cache_callback, 10 * 1024 * 1024
);
```

## 硬件依赖

### 必需硬件组件
- **DDS 芯片**: AD5932 (SPI 控制) - 信号生成
- **DAC 芯片**: DAC63001 (I2C 控制) - 增益控制
- **VGA 芯片**: AD8338 - 可变增益放大器
- **FPGA**: 数据采集和网络传输控制
- **网络接口**: eth0 (默认) - 数据传输

### 硬件初始化要求
调用前需确保以下资源已正确配置：
- `i2c_dev` - I2C 设备路径 (`/dev/i2c-2`)
- `eth_ifname` - 网络接口名 (`"eth0"`)
- `udp_header_params` - UDP/IP 头部参数

## 性能特性

### 数据率支持
- **实时模式**: 适用于需要即时数据处理的场景
- **缓存模式**: 适用于高数据率场景 (>10 MB/s)
- **混合模式**: 同时提供实时监控和事后分析能力

### 时序精度
- **扫频同步**: 微秒级精度控制
- **增益触发**: 硬件 GPIO 同步，无软件延迟
- **数据采集**: 与信号生成严格同步

### 资源消耗
- **内存**: 缓存大小可配置，最大 50MB
- **CPU**: 阻塞式执行，占用单个线程
- **网络**: 监听 UDP 端口 5030

## 错误处理

### 常见错误情况
1. **硬件初始化失败**
   - I2C/SPI 设备无法访问
   - 网络接口不可用
   - FPGA 初始化失败

2. **参数验证失败**
   - 增益值超出范围 (0-80 dB)
   - 缓存大小超限 (>50MB)
   - DDS 参数无效

3. **资源不足**
   - 内存分配失败
   - 缓存区满导致丢包

### 错误恢复策略
- 函数在错误退出时会自动关闭 DAC 和网络监听
- 建议在调用后进行返回值检查
- 可使用 `sbeam_get_cache_stats()` 获取详细状态信息

## 最佳实践

### 性能优化
1. **缓存大小设置**: 根据预期数据量合理设置缓存大小
2. **实时回调简化**: 实时回调中避免复杂计算，防止丢包
3. **参数预验证**: 调用前验证所有参数的有效性

### 错误预防
1. **硬件状态检查**: 调用前确认硬件连接正常
2. **资源预留**: 确保系统有足够内存用于缓存
3. **超时处理**: 考虑添加外部超时机制

### 调试技巧
1. **统计信息**: 使用 `sbeam_get_cache_stats()` 监控性能
2. **分段测试**: 先测试单独功能，再测试完整流程
3. **日志分析**: 关注函数内部的日志输出

## 相关函数

### 配套使用函数
```c
// 获取缓存统计信息
sbeam_cache_stats_t sbeam_get_cache_stats(void);

// 手动清空缓存
void sbeam_clear_cache(void);

// 停止网络监听
void sbeam_stop_listener_with_cache(const char *ifname);
```

### 简化版本函数
```c
// 不带缓存的版本
int transmit_and_receive_single_beam(
    const DDSConfig *cfg,
    uint16_t start_gain,
    uint16_t end_gain,
    uint32_t gain_duration_us,
    NetPacketCallback callback
);

// 仅信号生成
void generate_single_beam_signal(const DDSConfig *cfg);

// 仅信号接收
void receive_single_beam_response_with_cache(
    uint16_t start_gain,
    uint16_t end_gain,
    uint32_t gain_duration_us,
    NetPacketCallback packet_cb,
    NetCacheCallback cache_cb,
    uint32_t cache_size
);
```

## 版本兼容性

- **v1.0**: 基础功能，支持实时回调
- **v1.1**: 添加缓存功能，支持大数据量处理
- **v2.0**: 优化同步精度，改进错误处理

该函数提供了高度集成的单波束收发解决方案，特别适合需要精确同步和数据完整性的应用场景。