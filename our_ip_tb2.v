module our_ip_tb();
    `define NUM_TUPLES 1024

    reg clk = 0, s_axis_tvalid = 0, m_axis_tready = 1;
    wire m_axis_tvalid, s_axis_tready;
    reg [63:0] s_axis_tdata = 0;
    wire [63:0] m_axis_tdata;
    reg [7:0] s_axis_tkeep = 8'hff;
    wire [7:0] m_axis_tkeep;
    reg s_axis_tlast = 0;
    wire m_axis_tlast;
    main our_ip(clk, s_axis_tdata, s_axis_tkeep, s_axis_tlast, s_axis_tvalid, m_axis_tdata, m_axis_tkeep, m_axis_tlast, m_axis_tvalid, s_axis_tready);

    reg [31:0] src_ip_buffer [0:`NUM_TUPLES - 1];
    reg [31:0] dst_ip_buffer [0:`NUM_TUPLES - 1];
    reg [15:0] src_port_buffer [0:`NUM_TUPLES - 1];
    reg [15:0] dst_port_buffer [0:`NUM_TUPLES - 1];
    reg [7:0] protocol_buffer [0:`NUM_TUPLES - 1];

    reg [31:0] conn_buffer [0:`NUM_TUPLES - 1];

    reg [31:0] tuple_index = 0;
    reg [31:0] result_index = 0;

    reg [31:0] send_cnt = -8;
    reg [31:0] recv_cnt = 0;

    initial begin: init_arrays
        integer i;
        for (i = 0; i < `NUM_TUPLES; i = i + 1) begin
            if (i == 0 || ($random & 1)) begin
                src_ip_buffer[i] = $random;
                dst_ip_buffer[i] = $random;
                src_port_buffer[i] = $random;
                dst_port_buffer[i] = $random;
                protocol_buffer[i] = 8'h06;
            end else begin
                src_ip_buffer[i] = src_ip_buffer[i - 1];
                dst_ip_buffer[i] = dst_ip_buffer[i - 1];
                src_port_buffer[i] = src_port_buffer[i - 1];
                dst_port_buffer[i] = dst_port_buffer[i - 1];
                protocol_buffer[i] = 8'h06;
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
    
    initial begin
        $dumpvars(s_axis_tvalid, s_axis_tdata, s_axis_tkeep, s_axis_tlast, s_axis_tready, 
            m_axis_tvalid, m_axis_tdata, m_axis_tkeep, m_axis_tlast, m_axis_tready, tuple_index,
            clk, send_cnt, recv_cnt);
        #100000;
        $display("timeout.");
        $finish;
    end

    always @(posedge clk) begin: send_tuple
        if (s_axis_tvalid & s_axis_tready) begin
            s_axis_tdata <= 0;
            s_axis_tlast <= 0;
            send_cnt <= send_cnt + 8;
            case (send_cnt)
                0: begin
                    s_axis_tvalid <= 1;
                    s_axis_tdata[7:0] <= 8'h01;
                end
                8: begin
                    s_axis_tdata[39:32] <= 8'h08;
                    s_axis_tdata[47:40] <= 8'h06;
                end
                16: begin
                    s_axis_tdata[63:56] <= protocol_buffer[tuple_index];
                end
                24: begin
                    s_axis_tdata[47:16] <= src_ip_buffer[tuple_index];
                    s_axis_tdata[63:48] <= dst_ip_buffer[tuple_index][15:0];
                end
                32: begin
                    s_axis_tdata[15:0] <= dst_ip_buffer[tuple_index][31:16];
                    s_axis_tdata[31:16] <= src_port_buffer[tuple_index];
                    s_axis_tdata[47:32] <= dst_port_buffer[tuple_index];
                end
                48: begin
                    s_axis_tlast <= 1;
                end
                56: begin
                    send_cnt <= -8;
                    s_axis_tvalid <= 0;
                    tuple_index <= tuple_index + 1;
                end
            endcase
        end else if (send_cnt == 0) begin
            send_cnt <= send_cnt + 8;
            s_axis_tvalid <= 1;
            s_axis_tdata[7:0] <= 8'h01;
        end else if (send_cnt == -8) begin
            send_cnt <= send_cnt + 8;
        end
    end

    always @(posedge clk) begin: recv_result
        if (m_axis_tvalid & m_axis_tready) begin
            case (recv_cnt)
                24: begin
                    if (src_ip_buffer[result_index] != m_axis_tdata[47:16]) begin
                        $display("Error: send src_ip %x but receive %x, result_index=%d", src_ip_buffer[result_index], m_axis_tdata[47:16], result_index);
                        $finish;
                    end
                    if (dst_ip_buffer[result_index][15:0] != m_axis_tdata[63:48]) begin
                        $display("Error: send dst_ip but receive error");
                        $finish;
                    end
                end
                32: begin
                    if (dst_ip_buffer[result_index][31:16] != m_axis_tdata[15:0]) begin
                        $display("Error: send dst_ip but receive error");
                        $finish;
                    end
                    if (src_port_buffer[result_index] != m_axis_tdata[31:16]) begin
                        $display("Error: send src_port %d but receive %d", src_port_buffer[result_index], m_axis_tdata[31:16]);
                        $finish;
                    end
                    if (dst_port_buffer[result_index] != m_axis_tdata[47:32]) begin
                        $display("Error: send dst_port %d but receive %d", dst_port_buffer[result_index], m_axis_tdata[31:16]);
                        $finish;
                    end
                    conn_buffer[result_index] <= m_axis_tdata[47:32];
                end
            endcase
            
            // Update the byte count
            recv_cnt <= recv_cnt + 8;
            if (m_axis_tlast) begin
                recv_cnt <= 0;
                result_index <= result_index + 1;
            end
        end
    end

    always @(posedge clk) begin: verify_results
        integer i;
        if (result_index == `NUM_TUPLES) begin
            for (i = 0; i < `NUM_TUPLES; i = i + 1) begin
                if (conn_buffer[i] != dst_port_buffer[i]) begin
                    $display("Error %d", i);
                    $finish;
                end
            end
            $display("test passed");
            $finish;
        end
    end
endmodule
