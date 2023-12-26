module main (
    input clk,

    input [63:0] s_axis_tdata, 
    input [7:0] s_axis_tkeep, 
    input s_axis_tlast,
    input s_axis_tvalid, 
    input s_axis_tuser,

    output reg [63:0] m_axis_tdata, 
    output reg [7:0] m_axis_tkeep, 
    output reg m_axis_tlast,
    output reg m_axis_tvalid, 
    output reg m_axis_tuser,
    output s_axis_tready);

    `define WIDTH 104
    `define hash_len 16
    `define id_space (1<<`hash_len)

    reg [31:0] src_ip;
    reg [31:0] dst_ip; 
    reg [15:0] src_port; 
    reg [15:0] dst_port; 
    reg [7:0] protocol;

    reg [31:0] byte_cnt = 0;
    reg is_ip = 0;
    reg tready = 1;
    assign s_axis_tready = tready;

    wire [`hash_len-1:0] hash_value = src_ip ^ dst_ip ^ src_port ^ dst_port ^ protocol;
    reg [`hash_len-1:0] loc_to_probe = 0;
    reg hash_stage = 0;

    // map connection from id to tuple5. All zero represents no connection
    reg [`WIDTH-1:0] conn_mem [0:`id_space];
    integer i;
    initial begin
        for (i = 0; i <= `id_space; i = i + 1) begin
            conn_mem[i] = 0;
        end
    end

    always @(posedge clk) begin
        if (s_axis_tvalid && s_axis_tready) begin
            // Copy the input to the output
            m_axis_tdata <= s_axis_tdata;
            m_axis_tkeep <= s_axis_tkeep;
            m_axis_tlast <= s_axis_tlast;
            m_axis_tvalid <= s_axis_tvalid;
            m_axis_tuser <= s_axis_tuser;
            
            // Extract the 5-tuple from the input
            case (byte_cnt)
                8: begin
                    // Check if the packet is IP
                    if (s_axis_tdata[39:32] == 8'h08 && s_axis_tdata[47:40] == 8'h00) begin
                        is_ip <= 1;
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
                    if (is_ip) begin
                        hash_stage <= 1;
                        tready <= 0;
                        m_axis_tvalid <= 0; 
                        loc_to_probe <= hash_value;
                    end
                end
            endcase
            
            // Update the byte count
            byte_cnt <= byte_cnt + 8;
            if (s_axis_tlast) begin
                // Reset the byte count
                byte_cnt <= 0;
                is_ip <= 0;
            end
        end else if (hash_stage) begin
            // linear probe
            if (conn_mem[loc_to_probe] == { src_ip, dst_ip, src_port, dst_port, protocol } || conn_mem[loc_to_probe] == 0) begin
                conn_mem[loc_to_probe] <= { src_ip, dst_ip, src_port, dst_port, protocol };
                hash_stage <= 0;
                tready <= 1;
                m_axis_tvalid <= 1;
                m_axis_tdata[31:16] <= loc_to_probe; // store hash value into src_port
            end else begin
                loc_to_probe <= loc_to_probe + 1;
            end
        end
    end

endmodule
