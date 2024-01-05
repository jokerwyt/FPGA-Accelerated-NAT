#include <stdlib.h> //所有文件都要包含
#include <iostream> //所有文件都要包含
#include <verilated.h> //仿真的API
#include <verilated_vcd_c.h> //生成波形文件的API
#include "obj_dir/Vour_ip.h" // verilated 后 our_ip模块的类定义
#include <deque>

#define MAX_SIM_TIME 200  // 仿真时钟边沿（包括下降沿和上升沿），实际cycle为MAX_SIM_TIME/2
vluint64_t sim_time = 0; // 用于计数时钟边沿

Vour_ip *our;
VerilatedVcdC *m_trace; //定义跟踪对象

struct Packet {
    std::deque<uint8_t> data; // begin with ethernet header.

    Packet(int len = 14 + 20 + 20) {
        data.resize(len);
    }

    uint16_t* ethernet_type() {
        return (uint16_t*) &data[12];
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
};

struct Sender {
    std::deque<Packet> packets;
    int sleeping = 0;

    void rising_edge() {
        if (our->s_axis_tvalid && our->s_axis_tready) {
            // if last time transmitting is done.
            Packet& packet = packets.front();
            size_t rest_byte = packet.data.size();
            size_t transmit = rest_byte <= 8 ? rest_byte : 8;

            packet.data.erase(packet.data.begin(), packet.data.begin() + transmit);
            if (packet.data.empty()) {
                packets.pop_front();
            }
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

} sender;

struct Recver {
    std::deque<Packet> packets = {}; 
    Packet current_packet = Packet(0);

    // int sleeping = 0;

    void rising_edge() {
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
                if (current_packet.data.size() < 14 + 20 + 20) {
                    std::cout << "packet size is too small" << std::endl;
                    std::cout << "packet size = " << current_packet.data.size() << std::endl;
                    m_trace->close(); //关闭跟踪
                    exit(EXIT_FAILURE);
                }
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

    bool compare(std::vector<Packet>& packets) {
        if (packets.size() != this->packets.size()) {
            std::cout << "send packet cnt != recv packet cnt" << std::endl;
            std::cout << "send packet cnt = " << packets.size() << std::endl;
            std::cout << "recv packet cnt = " << this->packets.size() << std::endl;
            return false;
        }

        for (int i = 0; i < packets.size(); i++) {
            Packet& packet = packets[i];
            Packet& this_packet = this->packets[i];
            if (packet.data.size() != this_packet.data.size()) {
                std::cout << "packet.data.size() != this_packet.data.size()" << std::endl;
                std::cout << "packet idx = " << i << std::endl;
                return false;
            }

            for (int j = 0; j < packet.data.size(); j++) {
                if (packet.data[j] != this_packet.data[j]) {
                    std::cout << "packet.data[j] != this_packet.data[j]" << std::endl;
                    std::cout << "packet idx = " << i << std::endl;
                    std::cout << "byte idx = " << j << std::endl;
                    return false;
                }
            }
        }

        return true;
    }
} recver;


std::vector<Packet> gen_packet_ARP(int n = 10) {
    // just ARP packets.
    std::vector<Packet> packets;

    for (int i = 0; i < n; i++) {
        Packet packet = Packet(14 + 20 + 20);

        // fill all bytes with random value.
        for (int j = 0; j < packet.data.size(); j++) {
            packet.data[j] = rand() & 0xff;
        }

        packet.ethernet_type()[0] = 0x08;
        packet.ethernet_type()[1] = 0x06;

        packets.push_back(packet);
    }

    return packets;
}

std::vector<Packet> packets;
void init_context() {
    packets = gen_packet_ARP();
    sender.packets.insert(sender.packets.end(), packets.begin(), packets.end());
}

int main(int argc, char** argv, char** env) {
    our= new Vour_ip;
    m_trace = new VerilatedVcdC; //定义跟踪对象

    //接下来四行代码用于设置波形存储为VCD文件
    Verilated::traceEverOn(true); //打开跟踪选项
    our->trace(m_trace, 5); //将our和跟踪对象进行绑定,跟踪深度限制在our的5级以内
    m_trace->open("waveform.vcd"); //将跟踪波形输出为vcd格式

    init_context();
    our->clk = 0;
    our->eval();

    //第一个上升沿就在时刻0.
    //实际仿真的代码
    while (sim_time < MAX_SIM_TIME) {
        our->clk = 1;
        our->eval(); // 上升沿
        m_trace->dump(sim_time); //把被跟踪的信号写入到波形中
        sim_time++; //更新仿真时间


        sender.rising_edge();
        recver.rising_edge();
        // 上升沿后的更新

        our->clk = 0; 
        our->eval(); // 下降沿
        m_trace->dump(sim_time); //把被跟踪的信号写入到波形中
        sim_time++; //更新仿真时间
    }

    if (recver.compare(packets) == false) {
        m_trace->close(); //关闭跟踪
        exit(EXIT_FAILURE);
    }

    m_trace->close(); //关闭跟踪
    delete our; //删除对象

    printf("Test passed!\n"); //打印成功信息
    exit(EXIT_SUCCESS); //退出。并打印EXIT_SUCCESS
}