#include <stdlib.h> //所有文件都要包含
#include <iostream> //所有文件都要包含
#include <verilated.h> //仿真的API
#include <verilated_vcd_c.h> //生成波形文件的API
#include "obj_dir/Vour_ip.h" // verilated 后 ALU模块的类定义

#define MAX_SIM_TIME 20  // 仿真总时钟边沿数
vluint64_t sim_time = 0; // 用于计数时钟边沿

Vour_ip *our;

struct Packet {
    std::vector<int> data; // begin with ethernet header.

    Packet(int len = 128) {
        data.resize(len);
    }

    uint16_t* get_ethernet_type() {
        return (uint16_t*) &data[12];
    }

    uint16_t* get_ip_protocol() {
        return (uint16_t*) &data[23];
    }

    uint32_t* get_ip_src() {
        return (uint32_t*) &data[26];
    }

    uint32_t* get_ip_dst() {
        return (uint32_t*) &data[30];
    }

    uint16_t* get_tcp_src_port() {
        return (uint16_t*) &data[34];
    }

    uint16_t* get_tcp_dst_port() {
        return (uint16_t*) &data[36];
    }
};

struct Sender {
    std::vector<Packet> packets;
} sender;

void construct_input() {
    // do something ...
}

int main(int argc, char** argv, char** env) {
    our= new Vour_ip;

    //接下来四行代码用于设置波形存储为VCD文件
    Verilated::traceEverOn(true); //打开跟踪选项
    VerilatedVcdC *m_trace = new VerilatedVcdC; //定义跟踪对象
    our->trace(m_trace, 5); //将our和跟踪对象进行绑定,跟踪深度限制在our的5级以内
    m_trace->open("waveform.vcd"); //将跟踪波形输出为vcd格式

    //实际仿真的代码
    while (sim_time < MAX_SIM_TIME) { //没有达到20边沿数时，则一直进行仿真
        our->clk ^= 1; //clk 与 1 异或，翻转时钟
        our->eval(); //更新模块的状态，每一个周期更新一次信号
        m_trace->dump(sim_time); //把被跟踪的信号写入到波形中
        sim_time++; //更新仿真时间
    }

    m_trace->close(); //关闭跟踪
    delete our; //删除对象
    exit(EXIT_SUCCESS); //退出。并打印EXIT_SUCCESS
}