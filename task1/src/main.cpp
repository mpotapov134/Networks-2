#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include "colors.h"

void clonefinder(int protocol, void *multicast_addr);


void error(const char *msg) {
    printf("%s\n", msg);
    exit(EXIT_FAILURE);
}


int main(int argc, char **argv) {
    if (argc != 2) {
        error("Usage: clonefinder <multicast address>.");
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
        error("Invalid multicast ip address. Supported protocols are ipv4 and ipv6.");
    }
}


void clonefinder(int protocol, void *multicast_addr) {}
