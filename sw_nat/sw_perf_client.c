// sw_nat/sw_perf_client.c
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
#include <time.h>
#include <pthread.h>

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

struct SendPktsArgs {
    int sockfd;
    struct Packet *pkt;
    struct Tuple5 *tuple5s;
    struct HashTable *ht;
    int pktcnt;
};

#define kTableSize 1024
struct HashTable {
    // mutex
    pthread_mutex_t mutex;

    size_t conn_cnt;

    // hash table
    struct Tuple5 tuple5s[kTableSize];
    int unique_ids[kTableSize];
    int valid[kTableSize];
} ht;

size_t hash_value(struct Tuple5 *tuple5) {
    size_t hash = 0;
    hash = hash * 131 + tuple5->src_ip;
    hash = hash * 131 + tuple5->dst_ip;
    hash = hash * 131 + tuple5->src_port;
    hash = hash * 131 + tuple5->dst_port;
    hash = hash * 131 + tuple5->protocol;
    return hash;
}

size_t get_unique_id(struct HashTable *ht, struct Tuple5 *tuple5) {
    size_t idx = hash_value(tuple5) % kTableSize;

    // lock
    pthread_mutex_lock(&ht->mutex);

    // linear probe
    while (ht->valid[idx] == 1) {
        if (memcmp(&ht->tuple5s[idx], tuple5, sizeof(struct Tuple5)) == 0) {
            // found
            break;
        }
        idx = (idx + 1) % kTableSize;
    }

    if (ht->valid[idx] == 0) {
        // not found
        memcpy(&ht->tuple5s[idx], tuple5, sizeof(struct Tuple5));
        ht->unique_ids[idx] = ht->conn_cnt++;
        ht->valid[idx] = 1;
    }

    // release lock
    pthread_mutex_unlock(&ht->mutex);

    return idx;
}

void* send_pkts(void *args) {
    struct SendPktsArgs *send_pkts_args = (struct SendPktsArgs *)args;
    int sockfd = send_pkts_args->sockfd;
    struct Packet *pkt = send_pkts_args->pkt;
    struct Tuple5 *tuple5s = send_pkts_args->tuple5s;
    struct HashTable *ht = send_pkts_args->ht;
    int pktcnt = send_pkts_args->pktcnt;

    for (int i = 0; i < pktcnt; i++) { 
        int idx = i % tuple5_cnt;

        // send a packet
        size_t hash_idx = get_unique_id(ht, &tuple5s[i]);
        
        // fill idx & 0xffff into the src port
        // races are everywhere, but since it's just a baseline we don't care.
        memcpy(pkt[idx].data + 14 + 20, &hash_idx, 2);

        if (send(sockfd, pkt[idx].data, pkt[idx].length, 0) < 0) {
            fprintf(stderr, "send error %s", strerror(errno));
            exit(-1);
        }

        // wait for the response
        // while (1) {
        //     // printf("try to recv a new pkt...\n");
        //     int n = recv(sockfd, recv_buf, 8192, 0);
        //     if (n < 0) {
        //         fprintf(stderr, "recv error %s", strerror(errno));
        //         return -1;
        //     }

        //     if (n == 8192) {
        //         fprintf(stderr, "too big frame len: larger than 8192\n");
        //         return -1;
        //     }

        //     // check protocol == 0x0900
        //     if (recv_buf[12] == 0x09 && recv_buf[13] == 0x00) {
        //         // printf("Catch a pkt with protocol 0x0900\n");
        //         if (n != pkt[idx].length) {
        //             printf("pkt len does not match: %d != %d\n", n, pkt[idx].length);
        //             return -1;
        //         }
        //         break;
        //     } else {
        //         // not our pkt, skip
        //     }
        // }
    }
    return NULL;
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

    // assert(payload_len >= 0 && payload_len <= 1500);

    printf("using interface: %s\n", interface);
    printf("using tuple5_cnt: %d\n", tuple5_cnt);
    printf("using payload_len: %d\n", payload_len);
    printf("using target_mac: %s\n", target_mac_str);

    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        fprintf(stderr, "raw socket create error %s", strerror(errno));
        return -1;
    }

    int socket_buffer_size = 1024 * 1024 * 10;
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &socket_buffer_size, sizeof(socket_buffer_size)) < 0) {
        fprintf(stderr, "setsockopt error %s", strerror(errno));
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

   
    int pkt_len = 14 + 20 + 20 + payload_len;

    struct Packet pkt[tuple5_cnt];
    for (int i = 0; i < tuple5_cnt; i++) {
        pkt[i].data = malloc(pkt_len * sizeof(uint8_t));
        if (pkt[i].data == NULL) {
            fprintf(stderr, "memory allocation error");
            return -1;
        }
        pkt[i].length = pkt_len;
        create_custom_L4_pkt(pkt[i].data, tuple5s[i], payload_len);
    }
    
    uint8_t *recv_buf = malloc(8192 * sizeof(uint8_t));
    if (recv_buf == NULL) {
        fprintf(stderr, "memory allocation error");
        return -1;
    }

    int pktcnt = 1024 * 128;

    // init mutex
    pthread_mutex_init(&ht.mutex, NULL);

    // get high precision time
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // create 4 threads to send pkts
    int thread_num = 4;
    pthread_t threads[thread_num];
    printf("creating %d threads to send pkts...\n", thread_num);

    for (int i = 0; i < thread_num; i++) {
        struct SendPktsArgs *send_pkts_args = (struct SendPktsArgs *)malloc(sizeof(struct SendPktsArgs));
        send_pkts_args->sockfd = sockfd;
        send_pkts_args->pkt = pkt;
        send_pkts_args->pktcnt = pktcnt;
        send_pkts_args->tuple5s = tuple5s;
        send_pkts_args->ht = &ht;
        pthread_create(&threads[i], NULL, send_pkts, send_pkts_args);
    }

    // join them
    for (int i = 0; i < thread_num; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    close(sockfd);

    printf("time elapsed: %ld ms\n", (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000);
    printf("data sent %lf Gb\n", 1.0 * thread_num * pktcnt * pkt_len * 8.0 / 1e9);
    printf("avg thput: %lf Gbps\n",  1.0 * thread_num * pktcnt * pkt_len * 8.0 / ((end.tv_sec - start.tv_sec) * 1e9 + end.tv_nsec - start.tv_nsec));
    return 0;
}