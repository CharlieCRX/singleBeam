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

/**
 * @brief 设置网卡混杂模式
 *
 * @param ifname 网卡接口名称
 * @param sockfd 套接字描述符
 * @param enable 1启用混杂模式，0禁用
 * @return 0成功，-1失败
 */
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

/**
 * @brief 创建原始套接字并绑定到指定网卡
 *
 * @param ifname 网卡接口名称
 * @return 套接字描述符，失败返回-1
 */
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


/**
 * @brief 监听线程主循环
 *
 * @param arg 未使用参数
 * @return 
 */
static void *listener_loop(void *arg) {
  unsigned char buffer[NET_BUFFER_SIZE];

  while (running) {
    int length = recv(sockfd, buffer, sizeof(buffer), 0);
    if (length > 0 && user_cb)
      user_cb(buffer, length);
    else
      usleep(1000);
  }
  return NULL;
}

/**
 * @brief 启动网络监听
 *
 * @param ifname 网卡接口名称
 * @param cb 数据包回调函数
 * @return 0成功，-1失败
 */
int net_listener_start(const char *ifname, NetPacketCallback cb) {
  if (running)
    return 0;

  // 创建原始套接字
  sockfd = create_raw_socket(ifname);
  if (sockfd < 0)
    return -1;

  user_cb = cb;
  running = 1;

  // 创建监听线程
  if (pthread_create(&listener_thread, NULL, listener_loop, NULL) != 0) {
    perror("pthread_create");
    running = 0;
    close(sockfd);
    return -1;
  }

  LOG_INFO("[net_listener] Started on %s\n", ifname);
  return 0;
}

void net_listener_stop(const char *ifname) {
  if (!running)
    return;
  running = 0;
  pthread_join(listener_thread, NULL);
  set_promisc_mode(ifname, sockfd, 0);
  close(sockfd);
  sockfd = -1;
  LOG_INFO("[net_listener] Stopped\n");
}

int net_listener_is_running(void) {
  return running;
}
