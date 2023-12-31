// this should be run on the FPGA.

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

int main() {
    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        
        fprintf(stderr, "raw socket create error %s", strerror(errno));
        return -1;
    }

    // bind the socket to the interface eth1
    struct sockaddr_ll addr;
    memset(&addr, 0, sizeof(addr));
    addr.sll_family = AF_PACKET;
    addr.sll_ifindex = if_nametoindex("eth1");
    addr.sll_protocol = htons(ETH_P_ALL);
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "bind error %s", strerror(errno));
        return -1;
    }

    char buffer[8192];

    while (1) {
        // get an intact frame
        int n = recv(sockfd, buffer, sizeof(buffer), 0);

        if (n < 0) {
            fprintf(stderr, "cannot receive data: %s\n", strerror(errno));
            close(sockfd);
            return -1;
        }

        if (n > 4096) {
            fprintf(stderr, "frame too large: %d\n", n);
            return -1;
        }
        
        // determine if it's a TCP packet
        struct ethhdr *eth = (struct ethhdr *)buffer;
        if (ntohs(eth->h_proto) != ETH_P_IP) {
            continue;
        }

        // get the IP header
        struct iphdr *ip = (struct iphdr *)(buffer + sizeof(struct ethhdr));
        if (ip->protocol != IPPROTO_TCP) {
            continue;
        }

        printf("TCP: len=%d, (srcip, srcport) = (%s, %d), (dstip, dstport) = (%s, %d)\n",
                ntohs(ip->tot_len),
                inet_ntoa(*(struct in_addr *)&ip->saddr),
                ntohs(*(unsigned short *)(buffer + sizeof(struct ethhdr) + sizeof(struct iphdr))),
                inet_ntoa(*(struct in_addr *)&ip->daddr),
                ntohs(*(unsigned short *)(buffer + sizeof(struct ethhdr) + sizeof(struct iphdr) + 2)));
    }

    close(sockfd);
    return 0;
}
