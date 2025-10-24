#ifndef DEV_NET_LISTENER_H
#define DEV_NET_LISTENER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NET_BUFFER_SIZE 2048

// 数据包回调类型
typedef void (*NetPacketCallback)(const uint8_t *data, int length);

// 启动监听（非阻塞或独立线程模式）
int net_listener_start(const char *ifname, NetPacketCallback cb);

// 停止监听
void net_listener_stop(const char *ifname);

// 判断是否在运行
int net_listener_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // DEV_NET_LISTENER_H
