/* verilator lint_off DECLFILENAME */
/* verilator lint_off UNUSED */
module main (
    input clk,
    input reset,

    output [127:0] tuple_data_o,
    output tuple_valid_o,

    input [15:0] conn_data_i, // hash_len must not be greater than 16
    input conn_valid_i,

    input [63:0] s_axis_tdata, 
    input [7:0] s_axis_tkeep, 
    input s_axis_tlast,
    input s_axis_tvalid, 

    output reg [63:0] m_axis_tdata, 
    output reg [7:0] m_axis_tkeep, 
    output reg m_axis_tlast,
    output m_axis_tvalid, 
    output s_axis_tready);

    /*
    // For testbench
    wire [15:0] conn_data_i; // hash_len must not be greater than 16
    wire conn_valid_i;

    reg [127:0] unused_1 = 0;
    reg unused_2 = 0;
    wire [15:0] unused_3;
    wire unused_4;
    hash hash_ip(clk, tuple_data_o, tuple_valid_o, conn_data_i, conn_valid_i, unused_1, unused_2, unused_3, unused_4);
    */

    reg [31:0] src_ip;
    reg [31:0] dst_ip; 
    reg [15:0] src_port; 
    reg [15:0] dst_port; 
    reg [7:0] protocol;

    reg [31:0] byte_cnt = 0;
    reg is_custom_ip = 0;
    reg tready = 1;
    reg tvalid = 0;
    reg tuple_valid = 0;
    assign s_axis_tready = tready;
    assign m_axis_tvalid = tvalid;
    assign tuple_data_o = { 24'h0, src_ip, dst_ip, src_port, dst_port, protocol };
    assign tuple_valid_o = tuple_valid;

    parameter port_position = 16; // dst_port=32, src_port=16
    parameter hash_len = 8; // >=8 need special treatment for byte order

    reg [hash_len-1:0] loc_to_probe = 0;
    reg hash_stage = 0;

    reg [15:0] packet_cnt = 0;

    integer i;
    initial begin
        m_axis_tlast = 0;
    end

    always @(posedge clk) begin
        if (reset == 1'b0) begin
            tvalid <= 0;
            tready <= 1;
            byte_cnt <= 0;
            is_custom_ip <= 0;
            hash_stage <= 0;

            // TODO: clear out conn_mem
            // but verilator says no.
            
            // for (i = 0; i <= id_space; i = i + 1) begin
            //     conn_mem[i] <= 0;
            // end


        end else if (s_axis_tvalid && s_axis_tready) begin
            // Copy the input to the output
            m_axis_tdata <= s_axis_tdata;
            m_axis_tkeep <= s_axis_tkeep;
            m_axis_tlast <= s_axis_tlast;
            tvalid <= 1;
            
            // Extract the 5-tuple from the input
            case (byte_cnt)
                8: begin
                    // Check if the packet is IP
                    if (s_axis_tdata[39:32] == 8'h08 && s_axis_tdata[47:40] == 8'h00) begin
                        is_custom_ip <= 1;
                    end else begin
                        is_custom_ip <= 0;
                    end
                end
                16: begin
                    protocol <= s_axis_tdata[63:56];
                end
                24: begin
                    src_ip <= s_axis_tdata[47:16];
                    dst_ip[15:0] <= s_axis_tdata[63:48];
                end
                32: begin
                    dst_ip[31:16] <= s_axis_tdata[15:0];
                    src_port <= s_axis_tdata[31:16];
                    dst_port <= s_axis_tdata[47:32];
                    if (is_custom_ip && protocol == 8'h06) begin
                        tuple_valid <= 1;
                        hash_stage <= 1;
                        tready <= 0;
                        tvalid <= 0; 
                    end
                end
            endcase
            
            // Update the byte count
            byte_cnt <= byte_cnt + 8;
            if (s_axis_tlast) begin
                // Reset the byte count
                packet_cnt <= packet_cnt + 1;
		byte_cnt <= 0;
                is_custom_ip <= 0;
            end
        end else if (hash_stage) begin
            if (conn_valid_i) begin
                // CAUTIOUS: 
                m_axis_tdata[port_position+15:port_position] <= conn_data_i;
                tready <= 1;
                tvalid <= 1;
                tuple_valid <= 0;
		        hash_stage <= 0;
            end
        end else begin
            // Reset tvalid
            tvalid <= 0;
        end
    end

endmodule
