#include "net_listener.h"
#include "../utils/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <arpa/inet.h>

static volatile int running = 0;
static pthread_t listener_thread;
static int sockfd = -1;
static NetPacketCallback user_cb = NULL;
static NetCacheCallback user_cache_cb = NULL;

// 缓存相关变量
static uint8_t *packet_cache = NULL;
static uint32_t *packet_lengths = NULL;
static uint32_t cache_size = 0;
static uint32_t cache_used = 0;
static uint32_t packet_count = 0;
static uint32_t max_packets = 0;
static uint64_t total_bytes = 0;
static uint32_t dropped_packets = 0;
static pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;

// 缓存管理函数
static int init_cache(uint32_t size) {
  pthread_mutex_lock(&cache_mutex);
  
  // 计算最大包数（假设平均包大小为1KB）
  max_packets = size / 1024;
  if (max_packets < 1000) max_packets = 1000; // 最小1000个包
  
  // 分配包数据缓存
  cache_size = size;
  packet_cache = malloc(cache_size);
  if (!packet_cache) {
    LOG_ERROR("[net_listener] Failed to allocate packet cache\n");
    pthread_mutex_unlock(&cache_mutex);
    return -1;
  }
  
  // 分配包长度数组
  packet_lengths = malloc(max_packets * sizeof(uint32_t));
  if (!packet_lengths) {
    LOG_ERROR("[net_listener] Failed to allocate packet lengths array\n");
    free(packet_cache);
    packet_cache = NULL;
    pthread_mutex_unlock(&cache_mutex);
    return -1;
  }
  
  cache_used = 0;
  packet_count = 0;
  total_bytes = 0;
  dropped_packets = 0;
  
  LOG_INFO("[net_listener] Cache initialized: %u MB, max packets: %u\n", 
       size / (1024 * 1024), max_packets);
  pthread_mutex_unlock(&cache_mutex);
  return 0;
}

static void cleanup_cache(void) {
  pthread_mutex_lock(&cache_mutex);
  if (packet_cache) {
    free(packet_cache);
    packet_cache = NULL;
  }
  if (packet_lengths) {
    free(packet_lengths);
    packet_lengths = NULL;
  }
  cache_size = 0;
  cache_used = 0;
  packet_count = 0;
  total_bytes = 0;
  dropped_packets = 0;
  pthread_mutex_unlock(&cache_mutex);
}

static int add_packet_to_cache(const uint8_t *data, int length) {
  pthread_mutex_lock(&cache_mutex);
  
  // 检查缓存空间
  if (packet_count >= max_packets) {
    dropped_packets++;
    pthread_mutex_unlock(&cache_mutex);
    return -1; // 包数达到上限
  }
  
  if (cache_used + length > cache_size) {
    dropped_packets++;
    pthread_mutex_unlock(&cache_mutex);
    return -1; // 缓存空间不足
  }
  
  // 存储包数据
  memcpy(packet_cache + cache_used, data, length);
  packet_lengths[packet_count] = length;
  
  cache_used += length;
  total_bytes += length;
  packet_count++;
  
  pthread_mutex_unlock(&cache_mutex);
  return 0;
}

// 原有函数保持不变...
static int set_promisc_mode(const char *ifname, int sockfd, int enable) {
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

  if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
    perror("ioctl(SIOCGIFFLAGS)");
    return -1;
  }
  if (enable)
    ifr.ifr_flags |= IFF_PROMISC;
  else
    ifr.ifr_flags &= ~IFF_PROMISC;

  if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
    perror("ioctl(SIOCSIFFLAGS)");
    return -1;
  }
  return 0;
}

static int create_raw_socket(const char *ifname) {
  int sock;
  struct sockaddr_ll sll;
  struct ifreq ifr;

  sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  if (sock < 0) {
    perror("socket");
    return -1;
  }

  if (set_promisc_mode(ifname, sock, 1) < 0) {
    close(sock);
    return -1;
  }

  memset(&sll, 0, sizeof(sll));
  sll.sll_family = AF_PACKET;
  sll.sll_protocol = htons(ETH_P_ALL);
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

  if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
    perror("ioctl(SIOCGIFINDEX)");
    close(sock);
    return -1;
  }

  sll.sll_ifindex = ifr.ifr_ifindex;

  if (bind(sock, (struct sockaddr *)&sll, sizeof(sll)) < 0) {
    perror("bind");
    close(sock);
    return -1;
  }

  return sock;
}

static void *listener_loop(void *arg) {
  unsigned char buffer[NET_BUFFER_SIZE];

  while (running) {
    int length = recv(sockfd, buffer, sizeof(buffer), 0);
    if (length > 0) {
      // 调用用户回调
      if (user_cb)
        user_cb(buffer, length);
      
      // 缓存数据包
      if (user_cache_cb)
        add_packet_to_cache(buffer, length);
    } else {
      usleep(1000);
    }
  }
  return NULL;
}

int net_listener_start(const char *ifname, NetPacketCallback cb) {
  return net_listener_start_with_cache(ifname, cb, NULL, 0);
}

int net_listener_start_with_cache(const char *ifname, NetPacketCallback cb, 
                 NetCacheCallback cache_cb, uint32_t cache_size) {
  LOG_INFO("[net_listener] Starting on %s...\n", ifname);
  if (running)
    return 0;

  // 初始化缓存（如果启用了缓存功能）
  user_cache_cb = cache_cb;
  if (cache_cb && cache_size > 0) {
    if (init_cache(cache_size) < 0) {
      LOG_ERROR("[net_listener] Cache initialization failed\n");
      return -1;
    }
  }

  // 创建原始套接字
  sockfd = create_raw_socket(ifname);
  if (sockfd < 0) {
    cleanup_cache();
    return -1;
  }

  user_cb = cb;
  running = 1;

  // 创建监听线程
  if (pthread_create(&listener_thread, NULL, listener_loop, NULL) != 0) {
    perror("pthread_create");
    running = 0;
    cleanup_cache();
    close(sockfd);
    return -1;
  }

  LOG_INFO("[net_listener] Started on %s\n", ifname);
  return 0;
}

void net_listener_stop(const char *ifname) {
  net_listener_stop_with_cache(ifname);
}

void net_listener_stop_with_cache(const char *ifname) {
  if (!running)
    return;
  
  running = 0;
  pthread_join(listener_thread, NULL);
  set_promisc_mode(ifname, sockfd, 0);
  close(sockfd);
  sockfd = -1;
  
  // 如果有缓存回调，传递缓存数据
  if (user_cache_cb && packet_cache && packet_count > 0) {
    LOG_INFO("[net_listener] Delivering cached data: %u packets, %lu bytes\n", 
         packet_count, total_bytes);
    user_cache_cb(packet_cache, packet_count, total_bytes, packet_lengths);
  }
  
  cleanup_cache();
  user_cache_cb = NULL;
  LOG_INFO("[net_listener] Stopped\n");
}

int net_listener_is_running(void) {
  return running;
}

cache_stats_t net_listener_get_cache_stats(void) {
  cache_stats_t stats;
  pthread_mutex_lock(&cache_mutex);
  stats.total_packets = packet_count;
  stats.total_bytes = total_bytes;
  stats.cache_size = cache_size;
  stats.cache_used = cache_used;
  stats.dropped_packets = dropped_packets;
  pthread_mutex_unlock(&cache_mutex);
  return stats;
}

void net_listener_clear_cache(void) {
  pthread_mutex_lock(&cache_mutex);
  cache_used = 0;
  packet_count = 0;
  total_bytes = 0;
  dropped_packets = 0;
  pthread_mutex_unlock(&cache_mutex);
  LOG_INFO("[net_listener] Cache cleared\n");
}