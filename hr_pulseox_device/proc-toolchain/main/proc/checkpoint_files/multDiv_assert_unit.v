module multDiv_assert_unit(
    output start_mult,
    output start_div,
    input assert_mult,
    input assert_div,
    input clock,
    input reset
);

wire dff_inst1_out, dff_inst2_out, dff_inst3_out, dff_inst4_out;

dffe_ref dff_inst1(dff_inst1_out, assert_mult, clock, 1'b1, reset);
dffe_ref dff_inst2(dff_inst2_out, dff_inst1_out, ~clock, 1'b1, reset); //latches on falling edge.

assign start_mult = assert_mult & ~dff_inst2_out;


dffe_ref dff_inst3(dff_inst3_out, assert_div, clock, 1'b1, reset);
dffe_ref dff_inst4(dff_inst4_out, dff_inst3_out, ~clock, 1'b1, reset); //latches on falling edge, h

assign start_div = assert_div & ~dff_inst4_out;

endmodule