module pipeline (
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

    reg input_valid = 0;
    wire input_ready;

    // hash stage
    wire [`hash_len-1:0] hash_value = 
        tuple_buffer[`srcip]
        ^ tuple_buffer[`dstip]
        ^ tuple_buffer[`srcport]
        ^ tuple_buffer[`dstport]
        ^ tuple_buffer[`protocol];
    reg [`hash_len-1:0] hash_buffer = 0;
    reg hash_valid = 0;
    wire hash_ready;

    // probe stage
    reg [`hash_len:0] loc_to_probe = 0;
    reg [`hash_len:0] probe_buffer = 0;
    reg probe_valid = 0;
    wire probe_ready;

    // output stage
    reg output_valid = 0;
    wire output_ready = conn_ready_i;

    assign tuple_ready_o = input_ready;
    assign input_ready = !input_valid || hash_ready;
    assign hash_ready = !hash_valid || probe_ready;
    assign probe_ready = !probe_valid || output_ready;

    always @(posedge clk) begin
        // input stage
        if (tuple_valid_i && input_ready) begin
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
                    input_valid <= 1;
                    state <= 0;
                end
            endcase
        end
    end

    // hash stage
    always @(posedge clk) begin
        if (input_valid && hash_ready) begin
            hash_buffer <= hash_value;
            hash_valid <= 1;
            input_valid <= 0;
        end
    end

    // probe stage
    always @(posedge clk) begin
        if (hash_valid && probe_ready) begin
            if (conn_mem[hash_buffer] == tuple_buffer || conn_mem[hash_buffer] == 0) begin
                conn_mem[hash_buffer] <= tuple_buffer;
                probe_buffer <= hash_buffer;
                probe_valid <= 1;
                hash_valid <= 0;
            end else begin
                loc_to_probe <= hash_buffer + 1;
            end
        end else if (probe_valid && probe_ready) begin
            if (conn_mem[loc_to_probe] == tuple_buffer || conn_mem[loc_to_probe] == 0) begin
                conn_mem[loc_to_probe] <= tuple_buffer;
                probe_buffer <= loc_to_probe;
                probe_valid <= 1;
            end else begin
                loc_to_probe <= loc_to_probe + 1;
            end
        end
    end

    // output stage
    always @(posedge clk) begin
        if (probe_valid && output_ready) begin
            probe_valid <= 0;
            output_valid <= 0;
        end else if (probe_valid && !output_ready) begin
            output_valid <= 1;
        end
        conn_valid_o = output_valid;
        conn_data_o = probe_buffer;
    end

endmodule