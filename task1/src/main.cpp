#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "colors.h"

#define RECV_PORT 12345

void clonefinder(int protocol, const void *multicast_addr);
void sender_function(int protocol, const void *multicast_addr, pid_t parent_pid);


void handle_error(const char *msg) {
    printf("%s\n", msg);
    exit(EXIT_FAILURE);
}


void handle_perror(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}


int main(int argc, char **argv) {
    if (argc != 2) {
        handle_error("Usage: clonefinder <multicast address>.");
    }

    struct in_addr multicast_addr_ip;
    struct in6_addr multicast_addr_ipv6;

    if (inet_pton(AF_INET, argv[1], &multicast_addr_ip) == 1) {
        printf("%sIPv4 selected%s\n", GRN, CRESET);
        clonefinder(AF_INET, &multicast_addr_ip);
    } else if (inet_pton(AF_INET6, argv[1], &multicast_addr_ipv6) == 1) {
        printf("%sIPv6 selected%s\n", GRN, CRESET);
        clonefinder(AF_INET6, &multicast_addr_ipv6);
    } else {
        handle_error("Invalid multicast ip address. Supported protocols are ipv4 and ipv6.");
    }
}


void clonefinder(int protocol, const void *multicast_addr) {
    if (protocol != AF_INET && protocol != AF_INET6) {
        handle_error("clonefinder: invalid protocol");
    }

    int receiver_socket = socket(protocol, SOCK_DGRAM, 0);
    if (receiver_socket == -1) {
        handle_perror("socket");
    }

    /* allow for multiple sockets on the same port */
    int optval = 1;
    if (setsockopt(receiver_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        handle_perror("setsockopt");
    }

    /* set up receiving socket: bind and subscribe to multicast group */
    if (protocol == AF_INET) {
        /* bind socket to multicast address */
        struct in_addr *multicast_addr_ip = (in_addr *) multicast_addr;

        struct sockaddr_in receiver_addr;
        receiver_addr.sin_family = AF_INET;
        receiver_addr.sin_addr = *multicast_addr_ip;
        receiver_addr.sin_port = htons(RECV_PORT);

        if (bind(receiver_socket, (sockaddr *) &receiver_addr, sizeof(receiver_addr)) == -1) {
           handle_perror("bind");
        }

        /* subscribe to multicast group */
        struct ip_mreqn multicast_group;
        multicast_group.imr_multiaddr = *multicast_addr_ip;
        multicast_group.imr_address.s_addr = INADDR_ANY;
        multicast_group.imr_ifindex = 0;

        if (setsockopt(receiver_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                &multicast_group, sizeof(multicast_group)) == -1) {
            handle_perror("setsockopt");
        }
    } else if (protocol == AF_INET6) {
        /* bind socket to multicast address */
        struct in6_addr *multicast_addr_ipv6 = (in6_addr *) multicast_addr;

        struct sockaddr_in6 receiver_addr;
        receiver_addr.sin6_family = AF_INET6;
        receiver_addr.sin6_addr = in6addr_any;
        receiver_addr.sin6_port = htons(RECV_PORT);

        if (bind(receiver_socket, (sockaddr *) &receiver_addr, sizeof(receiver_addr)) == -1) {
            handle_perror("bind");
        }

        /* subscribe to multicast group */
        struct ipv6_mreq multicast_group;
        multicast_group.ipv6mr_multiaddr = *multicast_addr_ipv6;
        multicast_group.ipv6mr_interface = 0;

        if (setsockopt(receiver_socket, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP,
                &multicast_group, sizeof(multicast_group)) == -1) {
            handle_perror("setsockopt");
        }
    }

    /* start sender in a new process */
    pid_t parent_pid = getpid();
    pid_t fork_pid = fork();
    if (fork_pid == -1) {
        handle_perror("fork");
    } else if (fork_pid == 0) {
        sender_function(protocol, multicast_addr, parent_pid);
    } else {
        while (true);
    }
}


void sender_function(int protocol, const void *multicast_addr, pid_t parent_pid) {
    printf("sender start\n");
    int sender_socket = socket(protocol, SOCK_DGRAM, 0);
    if (sender_socket == -1) {
        handle_perror("socket");
    }

    struct sockaddr *dst_sockaddr;
    socklen_t dst_sockaddr_len;

    if (protocol == AF_INET) {
        struct sockaddr_in dst_sockaddr_in;
        dst_sockaddr_in.sin_family = AF_INET;
        dst_sockaddr_in.sin_addr = *(in_addr *) multicast_addr;
        dst_sockaddr_in.sin_port = htons(RECV_PORT);

        dst_sockaddr = (sockaddr *) &dst_sockaddr_in;
        dst_sockaddr_len = sizeof(dst_sockaddr_in);
    } else if (protocol == AF_INET6) {
        sockaddr_in6 dst_sockaddr_in6;
        dst_sockaddr_in6.sin6_family = AF_INET6;
        dst_sockaddr_in6.sin6_addr = *(in6_addr *) multicast_addr;
        dst_sockaddr_in6.sin6_port = htons(RECV_PORT);

        dst_sockaddr = (sockaddr *) &dst_sockaddr_in6;
        dst_sockaddr_len = sizeof(dst_sockaddr_in6);
    }

    uint8_t buffer[] = {1};
    while (true) {
        sendto(sender_socket, buffer, sizeof(buffer), 0, dst_sockaddr, dst_sockaddr_len);
        sleep(1);

        pid_t real_parent_pid = getppid();
        if (real_parent_pid != parent_pid) {
            exit(EXIT_SUCCESS);
        }
    }
}
