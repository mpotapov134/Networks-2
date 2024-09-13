#ifndef MAIN_H
#define MAIN_H

#include <arpa/inet.h>
#include <sys/types.h>

#define RECV_PORT 12345
#define BUF_SIZE 5
#define CHECK_INTERVAL 1000
#define OFFLINE_TIMEOUT 1000


void clonefinder(int protocol, const void *multicast_addr);
void sender_function(int protocol, const void *multicast_addr, pid_t parent_pid);
void receiver_function(int protocol, int receiver_socket);

#endif /* MAIN_H */
