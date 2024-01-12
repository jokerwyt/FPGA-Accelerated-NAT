
#ifndef __tuple5_h__
#define __tuple5_h__


// make sure it's cpp
#ifndef __cplusplus
#error "This is a C++ header file."
#endif

#include <stdint.h>
#include <deque>


struct Tuple5 {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t protocol;

    Tuple5() {
        src_ip = 0;
        dst_ip = 0;
        src_port = 0;
        dst_port = 0;
        protocol = 0;
    }

    Tuple5(uint32_t src_ip, uint32_t dst_ip, uint16_t src_port, uint16_t dst_port, uint8_t protocol) {
        this->src_ip = src_ip;
        this->dst_ip = dst_ip;
        this->src_port = src_port;
        this->dst_port = dst_port;
        this->protocol = protocol;
    }

    Tuple5(uint8_t *pkt_ptr) {
        src_ip = *(uint32_t*) (pkt_ptr + 26);
        dst_ip = *(uint32_t*) (pkt_ptr + 30);
        src_port = *(uint16_t*) (pkt_ptr + 34);
        dst_port = *(uint16_t*) (pkt_ptr + 36);
        protocol = *(uint8_t*) (pkt_ptr + 23);
    }

    bool operator==(const Tuple5& other) const {
        return src_ip == other.src_ip && dst_ip == other.dst_ip && src_port == other.src_port && dst_port == other.dst_port && protocol == other.protocol;
    }

    bool operator!=(const Tuple5& other) const {
        return !(*this == other);
    }

    bool operator<(const Tuple5& other) const {
        if (src_ip != other.src_ip) {
            return src_ip < other.src_ip;
        }
        if (dst_ip != other.dst_ip) {
            return dst_ip < other.dst_ip;
        }
        if (src_port != other.src_port) {
            return src_port < other.src_port;
        }
        if (dst_port != other.dst_port) {
            return dst_port < other.dst_port;
        }
        return protocol < other.protocol;
    }

    bool is_reply(const Tuple5& other) const {
        return src_ip == other.dst_ip && dst_ip == other.src_ip && src_port == other.dst_port && dst_port == other.src_port && protocol == other.protocol;
    }

    // operator << 
    friend std::ostream& operator<<(std::ostream& os, const Tuple5& tuple5) {
        os << "src_ip: " << tuple5.src_ip << ", dst_ip: " << tuple5.dst_ip << ", src_port: " << tuple5.src_port << ", dst_port: " << tuple5.dst_port << ", protocol: " << (int) tuple5.protocol;
        return os;
    }
};

struct Packet {
    std::deque<uint8_t> data; // begin with ethernet header.

    Packet(int len = 14 + 20 + 20) {
        data.resize(len);
    }

    uint8_t* ethernet_type_first_byte() {
        return (uint8_t*) &data[12];
    }

    uint16_t* ip_protocol() {
        return (uint16_t*) &data[23];
    }

    uint32_t* ip_src() {
        return (uint32_t*) &data[26];
    }

    uint32_t* ip_dst() {
        return (uint32_t*) &data[30];
    }

    uint16_t* tcp_src_port() {
        return (uint16_t*) &data[34];
    }

    uint16_t* tcp_dst_port() {
        return (uint16_t*) &data[36];
    }

    Tuple5 tuple5() {
        return Tuple5(*ip_src(), *ip_dst(), *tcp_src_port(), *tcp_dst_port(), *ip_protocol());
    }

    void apply(Tuple5 tuple5) {
        *ip_src() = tuple5.src_ip;
        *ip_dst() = tuple5.dst_ip;
        *tcp_src_port() = tuple5.src_port;
        *tcp_dst_port() = tuple5.dst_port;
        *ip_protocol() = tuple5.protocol;
    }
};
#endif