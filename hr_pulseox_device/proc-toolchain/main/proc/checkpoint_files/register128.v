module register128(
    output [127:0] data_out,
    input [127:0] data_in,

    input clr,
    input en,
    input clk

);

genvar i;
generate
    for (i = 0; i < 128; i = i + 1) begin : gen_reg
    dffe_ref dff_inst(//create 32 dffs 
        .q(data_out[i]),
        .d(data_in[i]),
        .clk(clk),
        .en(en),
        .clr(clr)
    );
    end
endgenerate


endmodule