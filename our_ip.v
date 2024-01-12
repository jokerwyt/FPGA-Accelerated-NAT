/* verilator lint_off DECLFILENAME */
/* verilator lint_off WIDTH */
/* verilator lint_off UNUSED */
module test(
    input clk,
    input reset,
    
    input [63:0] s_axis_tdata_tx, 
    input [7:0] s_axis_tkeep_tx, 
    input s_axis_tlast_tx,
    input s_axis_tvalid_tx, 

    output reg [63:0] m_axis_tdata_tx, 
    output reg [7:0] m_axis_tkeep_tx, 
    output reg m_axis_tlast_tx,
    output m_axis_tvalid_tx, 
    output s_axis_tready_tx,

    input [63:0] s_axis_tdata_rx, 
    input [7:0] s_axis_tkeep_rx, 
    input s_axis_tlast_rx,
    input s_axis_tvalid_rx, 

    output reg [63:0] m_axis_tdata_rx, 
    output reg [7:0] m_axis_tkeep_rx, 
    output reg m_axis_tlast_rx,
    output m_axis_tvalid_rx, 
    output s_axis_tready_rx);

    // 0 for tx
    wire [127:0] tuple_data_0;
    wire tuple_valid_0;

    wire [15:0] conn_data_0;
    wire conn_valid_0;

    // 1 for rx
    wire [127:0] tuple_data_1;
    wire tuple_valid_1;

    wire [15:0] conn_data_1;
    wire conn_valid_1;

    // tx
    main our_ip(clk, reset, tuple_data_0, tuple_valid_0, conn_data_0, conn_valid_0,
        s_axis_tdata_tx, s_axis_tkeep_tx, s_axis_tlast_tx, s_axis_tvalid_tx,
        m_axis_tdata_tx, m_axis_tkeep_tx, m_axis_tlast_tx, m_axis_tvalid_tx, s_axis_tready_tx);

    // hash
    hash hash_ip(clk, reset, tuple_data_0, tuple_valid_0, conn_data_0, conn_valid_0,
        tuple_data_1, tuple_valid_1, conn_data_1, conn_valid_1);

    // rx
    main our_ip_1(clk, reset, tuple_data_1, tuple_valid_1, conn_data_1, conn_valid_1,
        s_axis_tdata_rx, s_axis_tkeep_rx, s_axis_tlast_rx, s_axis_tvalid_rx,
        m_axis_tdata_rx, m_axis_tkeep_rx, m_axis_tlast_rx, m_axis_tvalid_rx, s_axis_tready_rx);

endmodule
