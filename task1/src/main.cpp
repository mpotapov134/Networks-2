#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <map>
#include <string>

#include "colors.h"
#include "main.h"

using namespace std;


void handle_error(const char *msg) {
    cout << BRED << msg << "\n" << CRESET;
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

    in_addr multicast_addr_ipv4;
    in6_addr multicast_addr_ipv6;

    int protocol;
    int receiver_socket, sender_socket;

    sockaddr *dst_sockaddr;
    socklen_t addr_len;

    if (inet_pton(AF_INET, argv[1], &multicast_addr_ipv4) == 1) {
        protocol = AF_INET;
        std::cout << BGRN << "IPv4 selected\n" << CRESET;
        receiver_socket = create_rcv_socket_ipv4(&multicast_addr_ipv4);

        sockaddr_in dst_address_ipv4 = create_dst_addr_ipv4(&multicast_addr_ipv4);
        dst_sockaddr = (sockaddr *) &dst_address_ipv4;
        addr_len = sizeof(dst_address_ipv4);
    }

    else if (inet_pton(AF_INET6, argv[1], &multicast_addr_ipv6) == 1) {
        protocol = AF_INET6;
        std::cout << BGRN << "IPv6 selected\n" << CRESET;
        receiver_socket = create_rcv_socket_ipv6(&multicast_addr_ipv6);

        sockaddr_in6 dst_address_ipv6 = create_dst_addr_ipv6(&multicast_addr_ipv6);
        dst_sockaddr = (sockaddr *) &dst_address_ipv6;
        addr_len = sizeof(dst_address_ipv6);
    }

    else {
        handle_error("Invalid multicast ip address. Supported protocols are ipv4 and ipv6.");
    }

    /* start sender in a new process, receiver - in current process */
    pid_t pid = getpid();
    pid_t fork_pid = fork();

    if (fork_pid == -1) {
        handle_perror("fork");
    } else if (fork_pid == 0) {
        sender_socket = socket(protocol, SOCK_DGRAM, 0);
        if (sender_socket == -1) {
            handle_perror("socket");
        }

        sender_function(sender_socket, dst_sockaddr, addr_len, pid);
    } else {
        receiver_function(protocol, receiver_socket);
    }
}


int create_socket_with_reusable_port() {
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd == -1) {
        handle_perror("socket");
    }

    /* allow for multiple sockets on the same port */
    int optval = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        handle_perror("setsockopt");
    }

    return socket_fd;
}


int create_rcv_socket_ipv4(const in_addr *multicast_addr) {
    int receiver_socket = create_socket_with_reusable_port();

    /* bind socket to multicast address */
    struct sockaddr_in receiver_addr;
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_addr = *multicast_addr;
    receiver_addr.sin_port = htons(RECV_PORT);

    if (bind(receiver_socket, (sockaddr *) &receiver_addr, sizeof(receiver_addr)) == -1) {
        handle_perror("bind");
    }

    /* subscribe to multicast group */
    struct ip_mreqn multicast_group;
    multicast_group.imr_multiaddr = *multicast_addr;
    multicast_group.imr_address.s_addr = INADDR_ANY;
    multicast_group.imr_ifindex = 0;

    if (setsockopt(receiver_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
            &multicast_group, sizeof(multicast_group)) == -1) {
        handle_perror("setsockopt");
    }

    return receiver_socket;
}


int create_rcv_socket_ipv6(const in6_addr *multicast_addr) {
    int receiver_socket = create_socket_with_reusable_port();

    /* bind to all interfaces; for some reason binding only to multicast addr does not work */
    struct sockaddr_in6 receiver_addr;
    receiver_addr.sin6_family = AF_INET6;
    receiver_addr.sin6_addr = in6addr_any;
    receiver_addr.sin6_port = htons(RECV_PORT);

    if (bind(receiver_socket, (sockaddr *) &receiver_addr, sizeof(receiver_addr)) == -1) {
        handle_perror("bind");
    }

    /* subscribe to multicast group */
    struct ipv6_mreq multicast_group;
    multicast_group.ipv6mr_multiaddr = *multicast_addr;
    multicast_group.ipv6mr_interface = 0;

    if (setsockopt(receiver_socket, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP,
            &multicast_group, sizeof(multicast_group)) == -1) {
        handle_perror("setsockopt");
    }

    return receiver_socket;
}


struct sockaddr_in create_dst_addr_ipv4(const in_addr *multicast_addr) {
    struct sockaddr_in dst_sockaddr;
    dst_sockaddr.sin_family = AF_INET;
    dst_sockaddr.sin_addr = *multicast_addr;
    dst_sockaddr.sin_port = htons(RECV_PORT);
    return dst_sockaddr;
}


struct sockaddr_in6 create_dst_addr_ipv6(const in6_addr *multicast_addr) {
    struct sockaddr_in6 dst_sockaddr;
    dst_sockaddr.sin6_family = AF_INET6;
    dst_sockaddr.sin6_addr = *multicast_addr;
    dst_sockaddr.sin6_port = htons(RECV_PORT);
    return dst_sockaddr;
}


void sender_function(int sender_socket, const sockaddr *dst_addr, socklen_t addr_len, pid_t parent_pid) {
    uint8_t buffer[BUF_SIZE];
    while (true) {
        sendto(sender_socket, buffer, sizeof(buffer), 0, dst_addr, addr_len);
        sleep(1);

        pid_t real_parent_pid = getppid();
        if (real_parent_pid != parent_pid) {
            exit(EXIT_SUCCESS);
        }
    }
}


void receiver_function(int protocol, int receiver_socket) {
    chrono::high_resolution_clock clock;
    auto start = clock.now();

    map<string, int64_t> copies;

    uint8_t buffer[BUF_SIZE];

    sockaddr_in src_sockaddr_in;
    sockaddr_in6 src_sockaddr_in6;
    sockaddr *src_sockaddr;
    socklen_t src_sockaddr_len;
    void *ip_address;

    if (protocol == AF_INET) {
        src_sockaddr = (sockaddr *) &src_sockaddr_in;
        src_sockaddr_len = sizeof(src_sockaddr_in);
        ip_address = &src_sockaddr_in.sin_addr;
    } else if (protocol == AF_INET6) {
        src_sockaddr = (sockaddr *) &src_sockaddr_in6;
        src_sockaddr_len = sizeof(src_sockaddr_in6);
        ip_address = &src_sockaddr_in6.sin6_addr;
    }

    int64_t last_check_time = 0;

    while (true) {
        recvfrom(receiver_socket, buffer, BUF_SIZE, 0, src_sockaddr, &src_sockaddr_len);
        auto cur_time = clock.now();
        int64_t time_from_start = chrono::duration_cast<chrono::milliseconds> (cur_time - start).count();

        char ip_str[INET6_ADDRSTRLEN];
        inet_ntop(protocol, ip_address, ip_str, sizeof(ip_str));
        string socket_str = string(ip_str) + ':' + to_string(src_sockaddr_in.sin_port);


        if (copies.find(socket_str) == copies.end()) {
            cout << "Обнаружена новая копия по адресу " << socket_str << "\n";
        }
        copies[socket_str] = time_from_start;


        if (time_from_start - last_check_time >= CHECK_INTERVAL) {
            /* check map */
            for (auto iter = copies.begin(); iter != copies.end();) {
                int64_t last_response_time = iter->second;
                if (time_from_start - last_response_time >= OFFLINE_TIMEOUT) {
                    std::cout << "Копия " << iter->first << " вышла из сети\n";
                    iter = copies.erase(iter);
                } else {
                    iter++;
                }
            }
            last_check_time = time_from_start;
        }
    }
}
