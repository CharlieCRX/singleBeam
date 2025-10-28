#ifndef DEV_NET_LISTENER_H
#define DEV_NET_LISTENER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NET_BUFFER_SIZE 2048
#define DEFAULT_CACHE_SIZE (500 * 1024 * 1024) // 500MB 默认缓存大小

// 数据包回调类型
typedef void (*NetPacketCallback)(const uint8_t *data, int length);

// 缓存完成回调类型 - 用户获取缓存数据的接口
typedef void (*NetCacheCallback)(const uint8_t *cache_data, uint32_t total_packets, 
                                uint64_t total_bytes, const uint32_t *packet_lengths);

// 缓存统计信息结构体
typedef struct {
    uint32_t total_packets;      // 总包数
    uint64_t total_bytes;        // 总字节数
    uint32_t cache_size;         // 缓存大小
    uint32_t cache_used;         // 已用缓存
    uint32_t dropped_packets;    // 丢弃的包数
} cache_stats_t;

// 启动监听（非阻塞或独立线程模式）
int net_listener_start(const char *ifname, NetPacketCallback cb);

// 启动带缓存的监听
int net_listener_start_with_cache(const char *ifname, NetPacketCallback cb, 
                                 NetCacheCallback cache_cb, uint32_t cache_size);

// 停止监听并获取缓存数据
void net_listener_stop_with_cache(const char *ifname);

// 停止监听
void net_listener_stop(const char *ifname);

// 判断是否在运行
int net_listener_is_running(void);

// 获取缓存统计信息
cache_stats_t net_listener_get_cache_stats(void);

// 手动清空缓存
void net_listener_clear_cache(void);

#ifdef __cplusplus
}
#endif

#endif // DEV_NET_LISTENER_H