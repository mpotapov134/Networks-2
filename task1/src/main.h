#ifndef MAIN_H
#define MAIN_H

#include <arpa/inet.h>
#include <sys/types.h>

#define RECV_PORT 12345
#define BUF_SIZE 5
#define CHECK_INTERVAL 1000
#define OFFLINE_TIMEOUT 1000

int create_socket_with_reusable_port();
int create_rcv_socket_ipv4(const in_addr *multicast_addr);
int create_rcv_socket_ipv6(const in6_addr *multicast_addr);

struct sockaddr_in create_dst_addr_ipv4(const in_addr *multicast_addr);
struct sockaddr_in6 create_dst_addr_ipv6(const in6_addr *multicast_addr);

void sender_function(int sender_socket, const sockaddr *dst_addr, socklen_t addr_len, pid_t parent_pid);
void receiver_function(int protocol, int receiver_socket);

#endif /* MAIN_H */
