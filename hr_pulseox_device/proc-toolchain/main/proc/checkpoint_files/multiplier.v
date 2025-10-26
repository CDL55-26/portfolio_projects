module multiplier(
    data_operandA, data_operandB, 
	ctrl_MULT,
	clock, 
	data_result, data_exception, data_resultRDY
    );

    input [31:0] data_operandA, data_operandB;
    input ctrl_MULT, clock;
    output [31:0] data_result;
    output data_exception;
    output data_resultRDY;

    // Start multiplier
    //wires
    wire signed [64:0] reg_mux_out, extended_opB, shifted_reg_in, concat_reg_in, reg_out;
    wire [63:0] edge_case_out, edge_case_XOR;
    wire signed [31:0] add_sub_mux_out, acc_mux_out, tc_unit_out, cla_out;
    wire [1:0] lsb_wire;
    wire acc_mux_select, add_OVF, latch_enable, count_wire, exception_unit_out;

    //Create extended opB
    assign extended_opB[0] = 1'b0;
    assign extended_opB[32:1] = data_operandB;
    assign extended_opB[64:33] = 32'b0;

    //counter
    counter count(count_wire,clock,~ctrl_MULT,ctrl_MULT);

    //register
    mux_2_65 reg_in_mux(reg_mux_out, ~ctrl_MULT, extended_opB, shifted_reg_in );//mux that chooses operand or reg_out value

    register65 register(reg_out, reg_mux_out, 1'b0, latch_enable,clock);
    assign lsb_wire[1] = reg_out[1];
    assign lsb_wire[0] = reg_out[0];

    //add/sub mux
    mux_4 add_sub_mux(add_sub_mux_out, lsb_wire, 32'b0, 32'b0, 32'b1, 32'b0 );//if 01, add; if 10, sub -> others don't care
    assign acc_mux_select = (lsb_wire[1]^lsb_wire[0]); //xor lsbs to determine add/sub or not

    //Adder
    Tcomp_unit tcUnit(tc_unit_out,data_operandA,add_sub_mux_out[0]);
    CLA32 cla(cla_out, add_OVF, reg_out[64:33], tc_unit_out, add_sub_mux_out[0]);

    mux_2 acc_mux(acc_mux_out,acc_mux_select,reg_out[64:33],cla_out);

    //Concat the accumulator reg val
    assign concat_reg_in = {acc_mux_out, reg_out[32:0]};

    assign shifted_reg_in = concat_reg_in >>> 1; //right shift everything by 1 

    assign data_result = reg_out[32:1];
    assign data_resultRDY = count_wire & ~ctrl_MULT;
    assign latch_enable = ~count_wire;

    exception_unit eu(exception_unit_out,data_operandA,data_operandB);

    assign data_exception = (~(&reg_out[64:32] | ~(|reg_out[64:32])) | add_OVF) & ~exception_unit_out;

endmodule