// this should be run on the FPGA.
// when recv pkt with eth_proto == 0x0900, it will send the pkt back to the sender.

#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        fprintf(stderr, "raw socket create error %s", strerror(errno));
        return -1;
    }

    // the default interface is eth1
    // append the interface name to the end of the command if you want to use another one

    char interface[128] = "eth1";

    if (argc > 1) {
        strcpy(interface, argv[1]);
    }

    // bind the socket to the interface eth1
    struct sockaddr_ll addr;
    memset(&addr, 0, sizeof(addr));
    addr.sll_family = AF_PACKET;
    addr.sll_ifindex = if_nametoindex(interface);

    if (addr.sll_ifindex == ENXIO) {
        fprintf(stderr, "interface %s not found", interface);
        return -1;
    }
    printf("watching interface %s\n", interface);

    addr.sll_protocol = htons(ETH_P_ALL);
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "bind error %s", strerror(errno));
        return -1;
    }

    char buffer[8192];


    // get the mac address of the open interface
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, interface);
    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) < 0) {
        fprintf(stderr, "ioctl error %s", strerror(errno));
        return -1;
    }

    // get env variable ONLY_OUR=0 or 1

    int only_our = 0;
    if (getenv("ONLY_OUR") != NULL) {
        only_our = atoi(getenv("ONLY_OUR"));
    }

    while (1) {
        // get an intact frame
        int n = recv(sockfd, buffer, sizeof(buffer), 0);

        if (n < 0) {
            fprintf(stderr, "cannot receive data: %s\n", strerror(errno));
            close(sockfd);
            return -1;
        }

        if (n > 4096) {
            fprintf(stderr, "frame too large: len = %d\n", n);
            return -1;
        }
        struct ethhdr *eth = (struct ethhdr *)buffer;
        
        if (ntohs(eth->h_proto) != 0x0900 && only_our) {
            // we skip the non-custom pkt
            continue;
        }

        // check the src mac to determine whether send or recv

        // compare the src mac address
        if (memcmp(eth->h_source, ifr.ifr_hwaddr.sa_data, 6) == 0) {
            // send
            printf("[send] ");
        } else {
            printf("[recv] ");
        }

        if (ntohs(eth->h_proto) == 0x0800) {
            printf(" IPv4  ");
            // get the IP header
            struct iphdr *ip = (struct iphdr *)(buffer + sizeof(struct ethhdr));
            if (ip->protocol == IPPROTO_TCP) {
                printf(" TCP ");

                char srcip[16], dstip[16];
                strcpy(srcip, inet_ntoa(*(struct in_addr *)&ip->saddr));
                strcpy(dstip, inet_ntoa(*(struct in_addr *)&ip->daddr));

                printf(" (srcip, srcport) = (%s, %d), (dstip, dstport) = (%s, %d) ",
                        srcip,
                        ntohs(*(unsigned short *)(buffer + sizeof(struct ethhdr) + sizeof(struct iphdr))),
                        dstip,
                        ntohs(*(unsigned short *)(buffer + sizeof(struct ethhdr) + sizeof(struct iphdr) + 2)));
            }
        } else 
        if (ntohs(eth->h_proto) == 0x0900) {
            printf(" CUSTOM  ");
            // get the IP header
            struct iphdr *ip = (struct iphdr *)(buffer + sizeof(struct ethhdr));
            if (ip->protocol == IPPROTO_TCP) {
                printf(" TCP ");

                char srcip[16], dstip[16];
                strcpy(srcip, inet_ntoa(*(struct in_addr *)&ip->saddr));
                strcpy(dstip, inet_ntoa(*(struct in_addr *)&ip->daddr));

                printf(" (srcip, srcport) = (%s, %d), (dstip, dstport) = (%s, %d) ",
                        srcip,
                        ntohs(*(unsigned short *)(buffer + sizeof(struct ethhdr) + sizeof(struct iphdr))),
                        dstip,
                        ntohs(*(unsigned short *)(buffer + sizeof(struct ethhdr) + sizeof(struct iphdr) + 2)));
            
                // create a dummy echo pkt
                // swap: src mac, dst mac, src ip, dst ip, src port, dst port

                // swap src mac and dst mac
                unsigned char tmp[6];
                memcpy(tmp, eth->h_source, 6);
                memcpy(eth->h_source, eth->h_dest, 6);
                memcpy(eth->h_dest, tmp, 6);

                // swap src ip and dst ip
                unsigned int tmpip = ip->saddr;
                ip->saddr = ip->daddr;
                ip->daddr = tmpip;

                // swap src port and dst port
                unsigned short tmpport = *(unsigned short *)(buffer + sizeof(struct ethhdr) + sizeof(struct iphdr));
                *(unsigned short *)(buffer + sizeof(struct ethhdr) + sizeof(struct iphdr)) = *(unsigned short *)(buffer + sizeof(struct ethhdr) + sizeof(struct iphdr) + 2);
                *(unsigned short *)(buffer + sizeof(struct ethhdr) + sizeof(struct iphdr) + 2) = tmpport;

                // send the pkt back
                if (send(sockfd, buffer, n, 0) < 0) {
                    fprintf(stderr, "send error %s", strerror(errno));
                    return -1;
                } else {
                    // make sure protocol is 0x0900
                    assert(ntohs(((struct ethhdr *)buffer)->h_proto) == 0x0900);

                    // hexdump the pkt
                    printf("send back %d bytes\n", n);
                    for (int i = 0; i < n; i++) {
                        printf("%02x ", (unsigned char)buffer[i]);
                        if (i % 16 == 15) {
                            printf("\n");
                        }
                    }
                }

            } else {
                assert(0);
            }
        }

        printf("len = %d\n", n);
    }

    close(sockfd);
    return 0;
}
