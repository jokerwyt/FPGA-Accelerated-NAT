/* verilator lint_off DECLFILENAME */
module main (
    input clk,

    input [63:0] s_axis_tdata, 
    input [7:0] s_axis_tkeep, 
    input s_axis_tlast,
    input s_axis_tvalid, 

    input reset,

    output reg [63:0] m_axis_tdata, 
    output reg [7:0] m_axis_tkeep, 
    output reg m_axis_tlast,
    output m_axis_tvalid, 
    output s_axis_tready);

    reg [31:0] src_ip;
    reg [31:0] dst_ip; 
    reg [15:0] src_port; 
    reg [15:0] dst_port; 
    reg [7:0] protocol;

    reg [31:0] byte_cnt = 0;
    reg is_ip = 0;
    reg tready = 1;
    reg tvalid = 0;
    assign s_axis_tready = tready;
    assign m_axis_tvalid = tvalid;

    parameter hash_len = 6; // >=8 need special treatment for byte order
    parameter id_space = 1 << hash_len;
    parameter WIDTH = 104;

    wire [hash_len-1:0] hash_value = src_ip[hash_len-1:0] 
        ^ dst_ip[hash_len-1:0] 
        ^ src_port[hash_len-1:0] 
        ^ dst_port[hash_len-1:0] 
        ^ protocol[hash_len-1:0];

    reg [hash_len-1:0] loc_to_probe = 0;
    reg hash_stage = 0;
    reg probe_stage = 0;

    // map connection from id to tuple5. All zero represents no connection
    reg [WIDTH-1:0] conn_mem [0:id_space-1];

    reg [hash_len-1:0] next_conn_idx = 0;
    reg [hash_len-1:0] conn_idx [0:id_space-1];

    integer i;
    initial begin
        for (i = 0; i <= id_space; i = i + 1) begin
            conn_mem[i] = 0;
        end
        m_axis_tlast = 0;
    end

    initial begin
        // $dumpvars(clk, s_axis_tvalid, s_axis_tdata, s_axis_tkeep, s_axis_tlast, s_axis_tready, tvalid, tready, 
        //    m_axis_tdata, m_axis_tkeep, m_axis_tlast, m_axis_tvalid, byte_cnt, is_ip, protocol, hash_stage, probe_stage, loc_to_probe, hash_value);
    end    

    always @(posedge clk) begin
        if (reset == 1'b0) begin
            tvalid <= 0;
            tready <= 1;
            byte_cnt <= 0;
            is_ip <= 0;
            hash_stage <= 0;
            probe_stage <= 0;

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
                        is_ip <= 1;
                    end else begin
                        is_ip <= 0;
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
                    if (is_ip && protocol == 8'h06) begin
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
                byte_cnt <= 0;
                is_ip <= 0;
            end
        end else if (hash_stage) begin
            if (conn_mem[hash_value] == { src_ip, dst_ip, src_port, dst_port, protocol } || conn_mem[hash_value] == 0) begin
                if (conn_mem[hash_value] == 0) begin
                    conn_mem[hash_value] <= { src_ip, dst_ip, src_port, dst_port, protocol };
                    conn_idx[hash_value] <= next_conn_idx;
                    next_conn_idx <= next_conn_idx + 1;

                    // CAUTIOUS: now conn_idx[hash_value] has no value yet.
                    m_axis_tdata[40+hash_len-1:40] <= next_conn_idx; // store hash value into dst_port
                end else begin
                    m_axis_tdata[40+hash_len-1:40] <= conn_idx[hash_value]; // store hash value into dst_port
                end

                hash_stage <= 0;
                tready <= 1;
                tvalid <= 1;
                m_axis_tdata[47:40+hash_len] <= 0;
                m_axis_tdata[39:32] <= 0;
            end else begin
                tvalid <= 0;
                hash_stage <= 0;
                probe_stage <= 1;
                loc_to_probe <= hash_value + 1;
            end
        end else if (probe_stage) begin
            // linear probe
            if (conn_mem[loc_to_probe] == { src_ip, dst_ip, src_port, dst_port, protocol } || conn_mem[loc_to_probe] == 0) begin
                if (conn_mem[loc_to_probe] == 0) begin
                    conn_mem[loc_to_probe] <= { src_ip, dst_ip, src_port, dst_port, protocol };
                    conn_idx[loc_to_probe] <= next_conn_idx;
                    next_conn_idx <= next_conn_idx + 1;
                    // CAUTIOUS: now conn_idx[hash_value] has no value yet.
                    m_axis_tdata[40+hash_len-1:40] <= next_conn_idx; // store hash value into dst_port
                end else begin
                    m_axis_tdata[40+hash_len-1:40] <= conn_idx[loc_to_probe]; // store hash value into dst_port
                end

                probe_stage <= 0;
                tready <= 1;
                tvalid <= 1;
                m_axis_tdata[47:40+hash_len] <= 0;
                m_axis_tdata[39:32] <= 0;
            end else begin
                tvalid <= 0;
                loc_to_probe <= loc_to_probe + 1;
            end
        end else begin
            // Reset tvalid
            tvalid <= 0;
        end
    end

endmodule
