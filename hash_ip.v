module hash (
    input clk,

    input [127:0] tuple_data_0,
    input tuple_valid_0,

    output reg [15:0] conn_data_0, 
    output reg conn_valid_0,

    input [127:0] tuple_data_1,
    input tuple_valid_1,

    output reg [15:0] conn_data_1, 
    output reg conn_valid_1);

    reg tuple_ready_0 = 1;
    reg tuple_ready_1 = 1;

    parameter hash_len = 6; // >=8 need special treatment for byte order
    parameter id_space = 1 << hash_len;
    parameter WIDTH = 104;

    reg [2:0] stage = 0;
    // 0: wait for rx and tx
    // 1: search in rx's table
    // 2: insert value into tx's table
    // 3: search in tx's table

    reg [31:0] inner_ip;
    reg [31:0] outer_ip;
    reg [15:0] inner_port;
    reg [15:0] outer_port;
    reg [7:0] protocol;
    reg [hash_len-1:0] loc_to_probe = 0;

    reg [WIDTH-1:0] conn_mem_rx [0:id_space-1];
    reg [hash_len-1:0] conn_idx_rx [0:id_space-1];
    reg [hash_len-1:0] next_conn_idx = 0;

    reg [WIDTH-1:0] conn_mem_tx [0:id_space-1];
    reg [hash_len-1:0] conn_idx_tx [0:id_space-1];

    integer i;
    initial begin
        for (i = 0; i <= id_space; i = i + 1) begin
            conn_mem_rx[i] = 0;
            conn_mem_tx[i] = 0;
        end
        conn_data_0 = 0;
        conn_valid_0 = 0;
        conn_data_1 = 0;
        conn_valid_1 = 0;        
    end

    always @(posedge clk) begin
        conn_valid_0 <= 0;
        conn_valid_1 <= 0;
        tuple_ready_0 <= 1;
        tuple_ready_1 <= 1;
        case (stage)
            0: begin
                if (tuple_valid_0 && tuple_ready_0) begin
                    stage <= 1;
                    protocol <= tuple_data_0[7:0];
                    outer_port <= tuple_data_0[23:8];
                    inner_port <= tuple_data_0[39:24];
                    outer_ip <= tuple_data_0[71:40];
                    inner_ip <= tuple_data_0[103:72];
                    loc_to_probe <= tuple_data_0[0+hash_len-1:0] 
                        ^ tuple_data_0[8+hash_len-1:8]
                        ^ tuple_data_0[24+hash_len-1:24]
                        ^ tuple_data_0[40+hash_len-1:40]
                        ^ tuple_data_0[72+hash_len-1:72];
                end else if (tuple_valid_1) begin
                    stage <= 3;
                    protocol <= tuple_data_1[7:0];
                    inner_port <= tuple_data_1[23:8];
                    outer_port <= tuple_data_1[39:24];
                    inner_ip <= tuple_data_1[71:40];
                    outer_ip <= tuple_data_1[103:72];
                    loc_to_probe <= tuple_data_1[0+hash_len-1:0] 
                        ^ tuple_data_1[8+hash_len-1:8]
                        ^ tuple_data_1[24+hash_len-1:24]
                        ^ tuple_data_1[40+hash_len-1:40]
                        ^ tuple_data_1[72+hash_len-1:72];
                end
            end
            1: begin
                if (conn_mem_rx[loc_to_probe] == { inner_ip, outer_ip, inner_port, outer_port, protocol } || conn_mem_rx[loc_to_probe] == 0) begin
                    if (conn_mem_rx[loc_to_probe] == 0) begin
                        // Insert value into rx's table
                        conn_mem_rx[loc_to_probe] <= { inner_ip, outer_ip, inner_port, outer_port, protocol };
                        conn_idx_rx[loc_to_probe] <= next_conn_idx;
                        stage <= 2;
                        loc_to_probe <= inner_ip ^ outer_ip ^ next_conn_idx ^ outer_port ^ protocol;
                        // CAUTIOUS: now conn_idx_rx[loc_to_probe] has no value yet.
                        conn_data_0 <= next_conn_idx; 
                    end else begin
                        conn_data_0 <= conn_idx_rx[loc_to_probe]; 
                    end
                    conn_valid_0 <= 1;
                    tuple_ready_0 <= 0;
                end else begin
                    loc_to_probe <= loc_to_probe + 1;
                end                
            end
            2: begin
                if (conn_mem_tx[loc_to_probe] == 0) begin
                    conn_mem_tx[loc_to_probe] <= { inner_ip, outer_ip, next_conn_idx, outer_port, protocol };
                    conn_idx_tx[loc_to_probe] <= inner_port;
                    next_conn_idx <= next_conn_idx + 1;
                    stage <= 0;
                end else begin
                    loc_to_probe <= loc_to_probe + 1;
                end
            end
            3: begin
                if (conn_mem_tx[loc_to_probe] == { inner_ip, outer_ip, inner_port, outer_port, protocol }) begin
                    conn_data_1 <= conn_idx_tx[loc_to_probe];
                    conn_valid_1 <= 1;
                    tuple_ready_1 <= 0;
                end else begin
                    loc_to_probe <= loc_to_probe + 1;
                end
            end
        endcase
    end

endmodule