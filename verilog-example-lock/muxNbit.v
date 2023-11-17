module Mux2to1(d0, d1, s, y);
    parameter N = 4;
    input wire [N-1:0] d0;
    input wire [N-1:0] d1;
    input wire s;
    output wire [N-1:0] y;

    assign y = (d0 & {N{(~s)}}) | (d1 & {N{s}});
endmodule

module Mux4to1(d0, d1, d2, d3, s, y);
    parameter N = 4;
    input wire [N-1:0] d0;
    input wire [N-1:0] d1;
    input wire [N-1:0] d2;
    input wire [N-1:0] d3;
    input wire [1:0] s;
    output wire [N-1:0] y;

    wire [N-1:0] y0, y1;
    Mux2to1 #(.N(N)) m0(d0, d1, s[0], y0);
    Mux2to1 #(.N(N)) m1(d2, d3, s[0], y1);
    Mux2to1 #(.N(N)) m2(y0, y1, s[1], y);
endmodule
