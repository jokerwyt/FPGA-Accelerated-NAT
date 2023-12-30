// this should be run on the FPGA.

#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <sys/ioctl.h>
#include <net/if.h>

int main() {
    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        std::cerr << "raw socket create error " << strerror(errno) << std::endl;
        return -1;
    }

    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    std::snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "eth1");
    if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, (void *)&ifr, sizeof(ifr)) < 0) {
        std::cerr << "cannot bind to adapter: " << strerror(errno) << std::endl;
        close(sockfd);
        return -1;
    }

    char buffer[8192];

    while (true) {
        // get an intact frame
        int n = recv(sockfd, buffer, sizeof(buffer), 0);

        if (n < 0) {
            std::cerr << "cannot receive data: " << strerror(errno) << std::endl;
            close(sockfd);
            return -1;
        }

        if (n > 4096) {
            std::cerr << "frame too large: " << n << std::endl;
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
