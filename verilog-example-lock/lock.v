module Lock(
    input wire clock,
    input wire [3:0] value,
    input wire new,
    input wire reset,
    output wire opened
);
    reg [3:0] state;
    reg opened_r;

    initial begin
        state = 4'd0;
        opened_r = 1'b0;
    end

    always @(posedge clock) begin

        if (reset) begin
            state <= 4'd0;
            opened_r <= 1'b0;
        end else if (new) begin
            if (state == 4'd0) begin
                if (value == 4'd1) begin
                    state <= 4'd1;
                end else begin
                    state <= 4'd0;
                end
            end

            if (state == 4'd1) begin
                if (value == 4'd2) begin
                    state <= 4'd2;
                end else begin
                    state <= 4'd0;
                end
            end

            if (state == 4'd2) begin
                if (value == 4'd3) begin
                    state <= 4'd3;
                    opened_r <= 1'b1;
                end else begin
                    state <= 4'd0;
                end
            end
        end
    end
    assign opened = opened_r;
endmodule