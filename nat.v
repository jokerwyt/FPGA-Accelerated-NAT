module main (
    input clk,
    
    // 5-tuple input
    input tuple_valid_i,
    input [31:0] tuple_data_i,
    output tuple_ready_o,
    
    // Connection ID output
    output reg conn_valid_o,
    output reg [31:0] conn_data_o,
    input conn_ready_i);

    `define WIDTH 104
    `define srcip 31:0
    `define dstip 63:32
    `define srcport 79:64
    `define dstport 95:80
    `define protocol 103:96
    `define hash_len 10
    `define id_space (1<<`hash_len)

    // map connection from id to tuple5. All zero represents no connection
    reg [`WIDTH-1:0] conn_mem [0:`id_space];
    integer i;
    initial begin
        for (i = 0; i <= `id_space; i = i + 1) begin
            conn_mem[i] = 0;
        end
    end

    reg [`WIDTH-1:0] tuple_buffer = 0;
    
    reg [3:0] state = 0;
    // 0: wait for src ip
    // 1: wait for dst ip
    // 2: wait for src port placed at [15:0] and dst port placed at [31:16]
    // 3: wait for protocol
    // 4: processing
    // 5: result ready


    wire [`hash_len-1:0] hash_value = 
        tuple_buffer[`srcip]
        ^ tuple_buffer[`dstip]
        ^ tuple_buffer[`srcport]
        ^ tuple_buffer[`dstport]
        ^ tuple_data_i[7:0];

    assign tuple_ready_o = (state < 4);
    reg [`hash_len:0] loc_to_probe = 0;


    always @(posedge clk) begin
        // handle input
        // this block handles AXL-like input.
        // conform to AXI handshaking to get our 5 tuples
        if (tuple_valid_i && tuple_ready_o) begin
            case (state)
                0: begin
                    tuple_buffer[`srcip] <= tuple_data_i;
                    state <= 1;
                end
                1: begin
                    tuple_buffer[`dstip] <= tuple_data_i;
                    state <= 2;
                end
                2: begin
                    tuple_buffer[`srcport] <= tuple_data_i[15:0];
                    tuple_buffer[`dstport] <= tuple_data_i[31:16];
                    state <= 3;
                end
                3: begin
                    tuple_buffer[`protocol] <= tuple_data_i[7:0];
                    loc_to_probe <= hash_value;
                    state <= 4;
                end
            endcase
        end
    end
   
    initial begin
        $dumpvars(clk, tuple_valid_i, tuple_data_i, tuple_ready_o, conn_valid_o, conn_data_o, conn_ready_i, loc_to_probe, state, tuple_buffer);
    end

    always @(posedge clk) begin
        if (state < 4) begin
            // still waiting for input.
            // do nothing.
            conn_valid_o <= 0;
            conn_data_o <= 0;
        end else if (state == 5) begin
            // we have found the connection.
            if (conn_ready_i) begin
                // connection is ready to be sent out
                // reset state
                conn_valid_o <= 0;
                conn_data_o <= 0;
                state <= 0;
            end
        end else begin
            // linear probe
            if (conn_mem[loc_to_probe] == tuple_buffer || conn_mem[loc_to_probe] == 0) begin
                conn_mem[loc_to_probe] <= tuple_buffer;
                conn_valid_o <= 1;
                conn_data_o <= loc_to_probe;
                state <= 5;
            end else begin
                loc_to_probe <= loc_to_probe + 1;
                conn_valid_o <= 0;
            end
        end
    end

endmodule