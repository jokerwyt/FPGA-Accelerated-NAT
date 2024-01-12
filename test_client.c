// this should be run on soarx.

#include <stdio.h>
#include <stdlib.h>
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
#include <netinet/tcp.h>

char interface[128] = "enp2s0d1";
int tuple5_cnt = 1;
int payload_len = 128;
char target_mac_str[128] = "00:0a:35:00:00:01";
char target_mac[6]; // this field will be copied into the ethernet header directly

struct ifreq ifr;


struct Tuple5 {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t protocol;
};

struct Packet {
    uint8_t *data;
    int length;
};

// src mac: ifr.ifr_hwaddr.sa_data
// dst mac: target_mac    
void create_custom_L4_pkt(uint8_t *data, struct Tuple5 tuple5, int payload_len) {
    
    int pkt_len = 14 + 20 + 20 + payload_len;

    // fill data with random data
    for (int i = 0; i < pkt_len; i++) {
        data[i] = rand();
    }

    // fill src mac and dest mac
    for (int i = 0; i < 6; i++) {
        data[i] = target_mac[i];
        data[6 + i] = ifr.ifr_hwaddr.sa_data[i];
    }

    // set special ethernet protocol
    data[12] = 0x09;
    data[13] = 0x00;

    // fill ip src ip and dst ip
    memcpy(data + 14 + 12, &tuple5.src_ip, 4);
    memcpy(data + 14 + 16, &tuple5.dst_ip, 4);

    // fill tcp src port and dst port
    memcpy(data + 14 + 20, &tuple5.src_port, 2);
    memcpy(data + 14 + 20 + 2, &tuple5.dst_port, 2);

    // fill ip protocol (TCP)
    data[14 + 9] = tuple5.protocol;
}


int main(int argc, char *argv[]) {
    printf("usage: ./test_client [-i <interface>] [-c <tuple5_cnt>] [-l <payload_len>] [-t <target_mac>]\n");
    // printf("default interface: enp2s0d1\n");
    // printf("default tuple5_cnt: 1\n");
    // printf("default payload_len: 0\n");
    // printf("default target_mac: 00:0a:35:00:00:01\n");

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
                fprintf(stderr, "Invalid option: -%c\n", optopt);
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

    printf("using interface: %s\n", interface);
    printf("using tuple5_cnt: %d\n", tuple5_cnt);
    printf("using payload_len: %d\n", payload_len);
    printf("using target_mac: %s\n", target_mac_str);

    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        fprintf(stderr, "raw socket create error %s", strerror(errno));
        return -1;
    }

    // bind the socket to the interface
    struct sockaddr_ll addr;
    memset(&addr, 0, sizeof(addr));
    addr.sll_family = AF_PACKET;
    addr.sll_ifindex = if_nametoindex(interface);

    if (addr.sll_ifindex == ENXIO) {
        fprintf(stderr, "interface %s not found", interface);
        return -1;
    }

    addr.sll_protocol = htons(ETH_P_ALL);
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "bind error %s", strerror(errno));
        return -1;
    }

    // get the mac address of the open interface
    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, interface);
    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) < 0) {
        fprintf(stderr, "ioctl error %s", strerror(errno));
        return -1;
    }

    struct Tuple5 *tuple5s = malloc(tuple5_cnt * sizeof(struct Tuple5));
    if (tuple5s == NULL) {
        fprintf(stderr, "memory allocation error");
        return -1;
    }

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

    struct Packet pkt;
    pkt.data = malloc(pkt_len * sizeof(uint8_t));
    pkt.length = pkt_len;
    if (pkt.data == NULL) {
        fprintf(stderr, "memory allocation error");
        return -1;
    }

    
    uint8_t *recv_buf = malloc(8192 * sizeof(uint8_t));
    if (recv_buf == NULL) {
        fprintf(stderr, "memory allocation error");
        return -1;
    }

    for (int i = 0; i <= pktcnt; i++) {

        create_custom_L4_pkt(pkt.data, tuple5s[rand() % tuple5_cnt], payload_len);

        // send a packet
        if (send(sockfd, pkt.data, pkt.length, 0) < 0) {
            fprintf(stderr, "send error %s", strerror(errno));
            return -1;
        }

        // wait for the response
        while (1) {
            printf("try to recv a new pkt...\n");
            int n = recv(sockfd, recv_buf, 8192, 0);
            if (n < 0) {
                fprintf(stderr, "recv error %s", strerror(errno));
                return -1;
            }
            if (n == 8192) {
                fprintf(stderr, "too big frame len: larger than 8192\n");
                return -1;
            }
            if (n != pkt.length) {
                printf("recv %d bytes, expected %d bytes\n", n, pkt.length);
                continue;
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

        // check if ip reverse
        if (memcmp(pkt.data + 14 + 12, recv_buf + 14 + 16, 4) != 0) {
            fprintf(stderr, "src-dst ip does not match\n");
            return -1;
        }
        if (memcmp(pkt.data + 14 + 16, recv_buf + 14 + 12, 4) != 0) {
            fprintf(stderr, "dst-src ip does not match\n");
            return -1;
        }

        // check if tcp port reverse
        if (memcmp(pkt.data + 14 + 20, recv_buf + 14 + 20 + 2, 2) != 0) {
            fprintf(stderr, "src-dst tcp port does not match\n");
            return -1;
        }

        if (memcmp(pkt.data + 14 + 20 + 2, recv_buf + 14 + 20, 2) != 0) {
            fprintf(stderr, "dst-src tcp port does not match\n");
            return -1;
        }
    }
    close(sockfd);

    printf("%d pkt test passed\n", pktcnt);
    return 0;
}
