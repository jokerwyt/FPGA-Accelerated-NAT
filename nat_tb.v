module nat_tb();

    `define NUM_TUPLES 1024

    reg clk = 1, tuple_valid_i = 0, conn_ready_i = 1;
    wire tuple_ready_o, conn_valid_o;
    reg [31:0] tuple_data_i = 0;
    wire [31:0] conn_data_o;
    main nat(clk, tuple_valid_i, tuple_data_i, tuple_ready_o, conn_valid_o, conn_data_o, conn_ready_i);

    // input arrays
    reg [31:0] src_ip_buffer [0:`NUM_TUPLES - 1];
    reg [31:0] dst_ip_buffer [0:`NUM_TUPLES - 1];
    reg [15:0] src_port_buffer [0:`NUM_TUPLES - 1];
    reg [15:0] dst_port_buffer [0:`NUM_TUPLES - 1];
    reg [7:0] protocol_buffer [0:`NUM_TUPLES - 1];

    // output arrays
    reg [31:0] conn_buffer [0:`NUM_TUPLES - 1];

    // sender state
    reg [3:0] state = 0;
    reg [31:0] tuple_index = 0;
    // 0: sending src ip
    // 1: sending dst ip
    // 2: sending src port and dst port
    // 3: sending protocol

    // receiver state
    reg [31:0] result_index = 0;

    // randomize input arrays
    initial begin: init_arrays
        integer i;
        for (i = 0; i < `NUM_TUPLES; i = i + 1) begin
            // with a probability of 1/2, generate a random tuple
            // with a probability of 1/2, generate a tuple that is the same as the previous one
            if (i == 0 || ($random & 1)) begin
                src_ip_buffer[i] = $random;
                dst_ip_buffer[i] = $random;
                src_port_buffer[i] = $random;
                dst_port_buffer[i] = $random;
                protocol_buffer[i] = $random;
            end else begin
                src_ip_buffer[i] = src_ip_buffer[i - 1];
                dst_ip_buffer[i] = dst_ip_buffer[i - 1];
                src_port_buffer[i] = src_port_buffer[i - 1];
                dst_port_buffer[i] = dst_port_buffer[i - 1];
                protocol_buffer[i] = protocol_buffer[i - 1];
            end
        end
    end

    // clk
    initial begin: clk_gen
        clk = 1'b1;
        forever begin
            #1 clk = 1'b0;
            #1 clk = 1'b1;
        end
    end
    
    // debug control
    initial begin
        $dumpvars(clk, tuple_valid_i, tuple_data_i, tuple_ready_o, 
            conn_valid_o, conn_data_o, conn_ready_i, state, tuple_index);

        // shutdown after 100000 cycles
        #1000000;
        $display("test failed: timeout.");
        $finish;
    end

    // send tuples
    initial begin
        #1;
        tuple_data_i = src_ip_buffer[0];
        tuple_valid_i = 1'b1;
    end
    always @(posedge clk) begin: send_tuple
        if (tuple_valid_i & tuple_ready_o) begin
            case (state)
                0: begin
                    tuple_data_i <= dst_ip_buffer[tuple_index];
                    state <= 1;
                end
                1: begin
                    tuple_data_i <= {dst_port_buffer[tuple_index], src_port_buffer[tuple_index]};
                    state <= 2;
                end
                2: begin
                    tuple_data_i <= protocol_buffer[tuple_index];
                    state <= 3;
                end
                3: begin
                    tuple_data_i <= src_ip_buffer[tuple_index + 1];
                    tuple_index <= tuple_index + 1;
                    state <= 0;
                end
            endcase
        end
    end

    // receive result
    always @(posedge clk) begin: recv_result
        if (conn_valid_o & conn_ready_i) begin
            conn_buffer[result_index] <= conn_data_o;
            result_index <= result_index + 1;
        end
    end


    // verify results
    always @(posedge clk) begin: verify_results
        integer i, j;
        if (result_index == `NUM_TUPLES) begin
            // check results
            // the same tuple should have the same result
            // different tuples should have different results
            for (i = 0; i < `NUM_TUPLES; i = i + 1) begin
                for (j = i + 1; j < `NUM_TUPLES; j = j + 1) begin
                    if (src_ip_buffer[i] == src_ip_buffer[j] 
                        && dst_ip_buffer[i] == dst_ip_buffer[j] 
                        && src_port_buffer[i] == src_port_buffer[j] 
                        && dst_port_buffer[i] == dst_port_buffer[j] 
                        && protocol_buffer[i] == protocol_buffer[j]) begin

                        if (conn_buffer[i] != conn_buffer[j]) begin
                            $display("Error: tuple %d and tuple %d should have the same result", i, j);
                            $display("tuple %d: %d", i, conn_buffer[i]);
                            $display("tuple %d: %d", j, conn_buffer[j]);
                            $display("end");
                            $finish;
                        end
                    end else begin
                        if (conn_buffer[i] == conn_buffer[j]) begin
                            $display("Error: tuple %d and tuple %d should have different results", i, j);
                            $display("tuple %d: %d", i, conn_buffer[i]);
                            $display("tuple %d: %d", j, conn_buffer[j]);
                            $display("end");
                            $finish;
                        end
                    end
                end
            end
            $display("test passed");
            $finish;
        end
    end

endmodule