// this should be run on soarx.

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
#include <errno.h>
#include <vector>
#include <tuple5.h>
#include <cassert>

char interface[128] = "enp2s0d1";
int tuple5_cnt = 1;
int payload_len = 0;
char target_mac_str[128] = "00:0a:35:00:00:01";
char target_mac[6]; // this field will be copied into the ethernet header directly

int main(int argc, char *argv[]) {
    std::cout << "usage: ./perf_client [-i <interface>] [-c <tuple5_cnt>] [-l <payload_len>] [-t <target_mac>]\n";
    std::cout << "default interface: enp2s0d1\n";
    std::cout << "default tuple5_cnt: 1\n";
    std::cout << "default payload_len: 0\n";
    std::cout << "default target_mac: 00:0a:35:00:00:01\n";

    // Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "i:c:")) != -1) {
        switch (opt) {
            case 'i':
                strncpy(interface, optarg, sizeof(interface) - 1);
                break;
            case 'c':
                tuple5_cnt = atoi(optarg);
                break;
            case 'l':
                payload_len = atoi(optarg);
                break;
            case 't':
                strncpy(target_mac_str, optarg, sizeof(target_mac_str) - 1);
                break;
            default:
                std::cerr << "Invalid option: -" << static_cast<char>(optopt) << "\n";
                return -1;
        }
    }

    {
        // assign target_mac
        char *p = target_mac_str;
        for (int i = 0; i < 6; i++) {
            target_mac[i] = strtol(p, &p, 16);
            p++;
        }
    }

    assert(payload_len >= 0 && payload_len <= 1500);

    std::cout << "using interface: " << interface << "\n";
    std::cout << "using tuple5_cnt: " << tuple5_cnt << "\n";
    std::cout << "using payload_len: " << payload_len << "\n";

    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        std::cerr << "raw socket create error " << strerror(errno);
        return -1;
    }

    // bind the socket to the interface
    struct sockaddr_ll addr;
    memset(&addr, 0, sizeof(addr));
    addr.sll_family = AF_PACKET;
    addr.sll_ifindex = if_nametoindex(interface);

    if (addr.sll_ifindex == ENXIO) {
        std::cerr << "interface " << interface << " not found";
        return -1;
    }

    addr.sll_protocol = htons(ETH_P_ALL);
    if (bind(sockfd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
        std::cerr << "bind error " << strerror(errno);
        return -1;
    }

    // get the mac address of the open interface
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, interface);
    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) < 0) {
        std::cerr << "ioctl error " << strerror(errno);
        return -1;
    }


    std::vector<Tuple5> tuple5s(tuple5_cnt);
    {
        // construct tuple5_cnt number of 5-tuple

        for (int i = 0; i < tuple5_cnt; i++) {
            tuple5s[i].src_ip = rand();
            tuple5s[i].dst_ip = rand();
            tuple5s[i].src_port = rand();
            tuple5s[i].dst_port = rand();
            tuple5s[i].protocol = 0x06; // TCP
        }
    }


    {
        // latency test
        // do pktcnt times of ping-pong test
        // send a packet, wait for the response, and then send the next one

        const size_t pktcnt = 10;
        std::vector<double> latencies(pktcnt);

        // warm up 1/10 of the packets

        char buf[8192];
        
        for (int i = 0; i * 10 <= pktcnt; i++) {
            
        }

        // TODO:
    }

    close(sockfd);
    return 0;
}
