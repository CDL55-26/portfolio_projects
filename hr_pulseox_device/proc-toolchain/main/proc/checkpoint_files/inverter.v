module inverter(
    output [31:0] inverted_val,
    output OVF_flag,
    input [31:0] input_val,
    input invert_control
);

//wires
wire [31:0] TC_out;

//input vall through tc unit
Tcomp_unit tc(TC_out, input_val, invert_control);

CLA32 cla(inverted_val, OVF_flag, 32'b0, TC_out, invert_control); //input A always zero

endmodule