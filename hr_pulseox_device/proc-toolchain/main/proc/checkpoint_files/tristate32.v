module tristate32 (
    output [31:0] buffer_out,
    input [31:0] buffer_in,
    input enable
);

genvar i;

generate
    for (i = 0; i < 32; i = i + 1) begin : tri_buf32
      bufif1 buf_inst (buffer_out[i], buffer_in[i], enable);
    end
endgenerate

endmodule