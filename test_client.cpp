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
#include "tuple5.h"
#include <cassert>
#include <netinet/tcp.h>

char interface[128] = "enp2s0d1";
int tuple5_cnt = 1;
int payload_len = 128;
char target_mac_str[128] = "00:0a:35:00:00:01";
char target_mac[6]; // this field will be copied into the ethernet header directly

int main(int argc, char *argv[]) {
    std::cout << "usage: ./test_client [-i <interface>] [-c <tuple5_cnt>] [-l <payload_len>] [-t <target_mac>]\n";
    // std::cout << "default interface: enp2s0d1\n";
    // std::cout << "default tuple5_cnt: 1\n";
    // std::cout << "default payload_len: 0\n";
    // std::cout << "default target_mac: 00:0a:35:00:00:01\n";

    // Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "i:c:l:t:")) != -1) {
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
    std::cout << "using target_mac: " << target_mac_str << "\n";

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


    int pktcnt = 10;
    int pkt_len = 14 + 20 + 20 + payload_len;

    Packet pkt = Packet(pkt_len);
    {
        // fill pkt with random data
        for (int i = 0; i < pkt_len; i++) {
            pkt.data[i] = rand();
        }

        // fill src mac and dest mac
        for (int i = 0; i < 6; i++) {
            pkt.data[i] = target_mac[i];
            pkt.data[6 + i] = ifr.ifr_hwaddr.sa_data[i];
        }

        // apply a random 5-tuple
        Tuple5 tuple5 = tuple5s[rand() % tuple5_cnt];

        pkt.apply(tuple5);

        // set special ethernet protocol
        pkt.ethernet_type_first_byte()[0] = 0x09;
        pkt.ethernet_type_first_byte()[1] = 0x00;
    }

    std::vector<uint8_t> buf(pkt.data.begin(), pkt.data.end());
    std::vector<uint8_t> recv_buf(pkt.data.size());

    for (int i = 0; i <= pktcnt; i++) {
        // send a packet
        if (send(sockfd, buf.data(), pkt_len, 0) < 0) {
            std::cerr << "send error " << strerror(errno);
            return -1;
        }

        // wait for the response

        while (1) {
            printf("try to recv...\n");
            int n = recv(sockfd, recv_buf.data(), sizeof(recv_buf), 0);
            if (n < 0) {
                std::cerr << "recv error " << strerror(errno);
                return -1;
            }
             
            if (n != pkt_len) {
                // printf("recv %d bytes, expected %d bytes\n", n, pkt_len);
                continue;
                // std::cerr << "recv error: recv " << n << " bytes, expected " << pkt_len << " bytes\n";
                
                // // hexdump the recv_buf
                // for (int j = 0; j < n; j++) {
                //     printf("%02x ", recv_buf[j]);
                // }
                // printf("\n");
                // return -1;
            }

            // check protocol == 0x0900
            if (recv_buf[12] == 0x09 && recv_buf[13] == 0x00) {
                break;
            }
        }

        // compare the response, things needed to be swapped:
        // 1. ethernet src mac and dst mac, (done since we receive the packet the interface we sent pkt)
        // 2. ip src ip and dst ip,
        // 3. tcp src port and dst port
        // payload should be the same

        if (Tuple5(recv_buf.data()).is_reply(pkt.tuple5()) == false) {
            std::cerr << "reply pkt " << i << " tuple5 mismatch\n";

            // print them all
            std::cerr << "sent pkt tuple5: " << pkt.tuple5() << "\n";
            std::cerr << "recv pkt tuple5: " << Tuple5(recv_buf.data()) << "\n";
            return -1;
        }

        // match payload
        for (int j = 0; j < payload_len; j++) {
            if (recv_buf[14 + 20 + 20 + j] != buf[14 + 20 + 20 + j]) {
                std::cerr << "reply pkt " << i << " payload mismatch\n";
                return -1;
            }
        }
    }
    close(sockfd);

    printf("%d pkt test apassed\n", pktcnt);
    return 0;
}
