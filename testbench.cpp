#include <stdlib.h> //所有文件都要包含
#include <iostream> //所有文件都要包含
#include <verilated.h> //仿真的API
#include <verilated_vcd_c.h> //生成波形文件的API
#include "obj_dir/Vour_ip.h" // verilated 后 our_ip模块的类定义
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

struct Sender {
    std::deque<Packet> packets;
    int sleeping = 0;

    void rising_edge(Vour_ip *our) {
        if (our->s_axis_tvalid && our->s_axis_tready) {
            // if last time transmitting is done.
            Packet& packet = packets.front();
            size_t rest_byte = packet.data.size();
            size_t transmit = rest_byte <= 8 ? rest_byte : 8;

            packet.data.erase(packet.data.begin(), packet.data.begin() + transmit);
            if (packet.data.empty()) {
                packets.pop_front();
            }

            sleeping = rand() % 3;
        }

        if (sleeping > 0) {
            sleeping--;
            our->s_axis_tvalid = 0;
            return;
        }

        if (packets.empty()) {
            our->s_axis_tvalid = 0;
            return;
        }

        // propose some new data.
        our->s_axis_tvalid = 1;
        Packet& packet = packets.front();
        size_t rest_byte = packet.data.size();
        size_t transmit = rest_byte < 8 ? rest_byte : 8;
        our->s_axis_tdata = 0;
        for (int i = 0; i < transmit; i++) {
            our->s_axis_tdata |= (uint64_t) packet.data[i] << (i * 8);
        }
        our->s_axis_tlast = packet.data.size() <= transmit;
        our->s_axis_tkeep = (1 << transmit) - 1;
    }

};

struct Recver {
    std::deque<Packet> packets = {}; 
    Packet current_packet = Packet(0);

    // int sleeping = 0;

    void rising_edge(Vour_ip *our) {
        // our->m_axis_tready is 1 default.
        
        if (our->m_axis_tvalid) {
            // if last time transmitting is done.
            
            // append to the last packet.
            // our->m_axis_tkeep is reg[7:0], 1 for valid, 0 for invalid.
            // only consecutive low bits are valid.
            for (int i = 0; i < 8; i++) {
                if (our->m_axis_tkeep & (1 << i)) {
                    current_packet.data.push_back((our->m_axis_tdata >> (i * 8)) & 0xff);
                }
            }

            if (our->m_axis_tlast) {
                packets.push_back(current_packet);
                current_packet = Packet(0);
            }
        }

        // if (sleeping > 0) {
        //     sleeping--;
        //     our->m_axis_tready = 0;
        //     return;
        // }

        // our->m_axis_tready = 1;
    }
 
};

bool exact_compare(std::vector<Packet>& packets_a, std::vector<Packet>& packets_b) {
    if (packets_a.size() != packets_b.size()) {
        std::cout << "packet cnt mismatch" << std::endl;
        std::cout << "aside packet cnt = " << packets_a.size() << std::endl;
        std::cout << "bside packet cnt = " << packets_b.size() << std::endl;
        return false;
    }

    for (int i = 0; i < packets_a.size(); i++) {
        Packet& aside = packets_a[i];
        Packet& bside = packets_b[i];
        if (aside.data.size() != bside.data.size()) {
            std::cout << "aside.data.size() != bside.data.size()" << std::endl;
            std::cout << "pkt idx = " << i << std::endl;
            return false;
        }

        for (int j = 0; j < aside.data.size(); j++) {
            if (aside.data[j] != bside.data[j]) {
                std::cout << "aside.data[j] != bside.data[j]" << std::endl;
                std::cout << "pkt idx = " << i << std::endl;
                std::cout << "byte idx = " << j << std::endl;
                return false;
            }
        }
    }

    return true;
}

std::vector<Packet> gen_packet_ARP(int n = 10) {
    // just ARP packets.
    std::vector<Packet> packets;

    for (int i = 0; i < n; i++) {
        Packet packet = Packet(14 + 20 + 20 + rand() % 100);

        // fill all bytes with random value.
        for (int j = 0; j < packet.data.size(); j++) {
            packet.data[j] = rand() & 0xff;
        }

        packet.ethernet_type_first_byte()[0] = 0x08;
        packet.ethernet_type_first_byte()[1] = 0x06;

        packets.push_back(packet);
    }

    return packets;
}


std::vector<Packet> process(std::vector<Packet> ingress_pkts, const size_t MAX_SIM_CYCLE = 10000) {
    // 仿真cycle为MAX_SIM_CYCLE（时钟上升沿数量）

    vluint64_t sim_time = 0; // 用于计数时钟边沿

    Vour_ip *our = new Vour_ip;
    VerilatedVcdC *m_trace = new VerilatedVcdC; //定义跟踪对象

    //接下来四行代码用于设置波形存储为VCD文件
    Verilated::traceEverOn(true); //打开跟踪选项
    our->trace(m_trace, 5); //将our和跟踪对象进行绑定,跟踪深度限制在our的5级以内
    m_trace->open("waveform.vcd"); //将跟踪波形输出为vcd格式

    Sender sender;
    Recver recver;

    sender.packets.insert(sender.packets.end(), ingress_pkts.begin(), ingress_pkts.end());
    our->clk = 0;
    our->eval();

    int packet_cnt = ingress_pkts.size();

    //第一个上升沿就在时刻0.
    //实际仿真的代码
    while (sim_time < MAX_SIM_CYCLE * 2) {
        our->clk = 1;
        our->eval(); // 上升沿
        m_trace->dump(sim_time); //把被跟踪的信号写入到波形中
        sim_time++; //更新仿真时间

        // 上升沿后的更新
        sender.rising_edge(our);
        recver.rising_edge(our);

        our->clk = 0; 
        our->eval(); // 下降沿
        m_trace->dump(sim_time); //把被跟踪的信号写入到波形中
        sim_time++; //更新仿真时间

        if (recver.packets.size() == packet_cnt) {
            break;
        }
    }

    std::cout << "processing for " << sim_time/2 << " cycles, cycle limitation = " << MAX_SIM_CYCLE << std::endl;
    m_trace->close(); //关闭跟踪
    
    // make sure recv.current_packet is empty.
    if (recver.current_packet.data.size() != 0) {
        std::cout << "corruptted last packet, size = " << recver.current_packet.data.size() << std::endl;
        exit(-1);
    }

    return std::vector<Packet>(recver.packets.begin(), recver.packets.end());
}

void test_arp() {
    std::vector<Packet> packets;
    packets = gen_packet_ARP(10000);
    
    auto result = process(packets, 1000000);

    if (exact_compare(packets, result) == false) {
        exit(-1);
    }

    printf("ARP test passed!\n");
}


// ============== IP packet test below =================

bool check_nat_funtionality(std::vector<Packet>& packets, std::vector<Packet>& recv_packets) {
    // first check packet numbers
    if (packets.size() != recv_packets.size()) {
        std::cout << "packet number not match" << std::endl;
        std::cout << "send packet cnt = " << packets.size() << std::endl;
        std::cout << "recv packet cnt = " << recv_packets.size() << std::endl;
        return false;
    }

    std::map<Tuple5, Tuple5> nat_table;
    for (int i = 0; i < packets.size(); i++) {
        Packet& packet = packets[i];
        Packet& recv_packet = recv_packets[i];

        if (packet.data.size() != recv_packet.data.size()) {
            std::cout << "packet.data.size() != recv_packet.data.size()" << std::endl;
            std::cout << "packet idx = " << i << std::endl;
            return false;
        }

        // if ARP packet, exact compare.
        if (packet.ethernet_type_first_byte()[0] == 0x08 && packet.ethernet_type_first_byte()[1] == 0x06) {
            for (int j = 0; j < packet.data.size(); j++) {
                if (packet.data[j] != recv_packet.data[j]) {
                    std::cout << "ARP packet.data[j] != recv_packet.data[j]" << std::endl;
                    std::cout << "packet idx = " << i << std::endl;
                    std::cout << "byte idx = " << j << std::endl;
                    std::cout << "packet.data[j] = " << (int) packet.data[j] << std::endl;
                    std::cout << "recv_packet.data[j] = " << (int) recv_packet.data[j] << std::endl;
                    return false;
                }
            }
            continue;
        }

        // IP packets:
        // check payload (after byte[14+20+20], included)
        for (int j = 14 + 20 + 20; j < packet.data.size(); j++) {
            if (packet.data[j] != recv_packet.data[j]) {
                std::cout << "packet.data[j] != recv_packet.data[j]" << std::endl;
                std::cout << "packet idx = " << i << std::endl;
                std::cout << "byte idx = " << j << std::endl;
                return false;
            }
        }

        // check nat table

        Tuple5 tuple5 = packet.tuple5();
        Tuple5 recv_tuple5 = recv_packet.tuple5();

        if (nat_table.find(tuple5) == nat_table.end()) {

            // special guarantee
            if (nat_table.size() != recv_tuple5.dst_port) {
                printf("new connection mismatch: nat_table.size() != recv_tuple5.dst_port\n");
                printf("packet idx = %d\n", i);
                printf("nat_table.size() = %ld\n", nat_table.size());
                printf("recv_tuple5.dst_port = %d\n", recv_tuple5.dst_port);
                printf("tuple5 = %x %x %x %x %x\n", tuple5.src_ip, tuple5.dst_ip, tuple5.src_port, tuple5.dst_port, tuple5.protocol);
                printf("recv_tuple5 = %x %x %x %x %x\n", recv_tuple5.src_ip, recv_tuple5.dst_ip, recv_tuple5.src_port, recv_tuple5.dst_port, recv_tuple5.protocol);
                return false;
            }

            nat_table[tuple5] = recv_tuple5;
        } else {
            if (nat_table[tuple5] != recv_tuple5) {
                std::cout << "nat_table[tuple5] != recv_tuple5" << std::endl;
                std::cout << "packet idx = " << i << std::endl;
                printf("tuple5 = %x %x %x %x %x\n", tuple5.src_ip, tuple5.dst_ip, tuple5.src_port, tuple5.dst_port, tuple5.protocol);
                printf("nat_table[tuple5] (last mapping) = %x %x %x %x %x\n", nat_table[tuple5].src_ip, nat_table[tuple5].dst_ip, nat_table[tuple5].src_port, nat_table[tuple5].dst_port, nat_table[tuple5].protocol);
                printf("recv_tuple5 (this mapping) = %x %x %x %x %x\n", recv_tuple5.src_ip, recv_tuple5.dst_ip, recv_tuple5.src_port, recv_tuple5.dst_port, recv_tuple5.protocol);
                return false;
            }
        }
    }
    std::cout << "NAT table size = " << nat_table.size() << std::endl;
    return true;
}

std::vector<Packet> gen_packet_ip(int n = 50, int tuple5_cnt = 5) {
    // gen tcp/ip packet with random 5tuple
    std::vector<Packet> packets;

    Tuple5 tuple5s[tuple5_cnt];
    for (int i = 0; i < tuple5_cnt; i++) {
        tuple5s[i] = Tuple5(rand(), rand(), rand(), rand(), 0x06); // TCP only now.
    }

    for (int i = 0; i < n; i++) {
        Packet packet = Packet(14 + 20 + 20 + rand() % 100);


        // fill all bytes with random value.
        for (int j = 0; j < packet.data.size(); j++) {
            packet.data[j] = rand() & 0xff;
        }

        if (rand() % 100 < 50) {
            // fill in ARP packet.
            packet.ethernet_type_first_byte()[0] = 0x08;
            packet.ethernet_type_first_byte()[1] = 0x06;
            
        } else {
            // fill in IP packet.
            packet.ethernet_type_first_byte()[0] = 0x08;
            packet.ethernet_type_first_byte()[1] = 0x00;
            packet.apply(tuple5s[rand() % tuple5_cnt]);
        }


        packets.push_back(packet);
    }

    return packets;
}

void test_ip() {
    std::vector<Packet> packets;

    int pkt_cnt = 10000;

    packets = gen_packet_ip(pkt_cnt, 64);
    auto result = process(packets, pkt_cnt * (256 / 8 + 64));

    if (check_nat_funtionality(packets, result) == false) {
        exit(-1);
    }
    printf("IP test passed!\n");
}

int main(int argc, char** argv, char** env) {
    srand(time(0));
    test_arp();
    test_ip();
}