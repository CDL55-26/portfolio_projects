module multdiv(
	data_operandA, data_operandB, 
	ctrl_MULT, ctrl_DIV, 
	clock, 
	data_result, data_exception, data_resultRDY);

    input [31:0] data_operandA, data_operandB;
    input ctrl_MULT, ctrl_DIV, clock;

    output [31:0] data_result;
    output data_exception, data_resultRDY;

    // add your code here

    //wires
    wire [31:0] mult_result, div_result;
    wire op_ctrl_dff_out, mult_rdy, div_rdy, mult_error, div_error;
    wire corrected_div_ready, corrected_mult_ready;

    //MultDiv
    divider div(data_operandA, data_operandB, ctrl_DIV, clock, div_result, div_error ,div_rdy);
    multiplier mult(data_operandA, data_operandB, ctrl_MULT, clock, mult_result, mult_error , mult_rdy);

    //Result Mux
    mux_2 result_mux(data_result,op_ctrl_dff_out, div_result, mult_result);

    //ctrl DFF

    /* 
    if mult asserted high, enable dff -> set output to 1, if div asserted high, reset -> ouput 0
    */ 
    dffe_ref op_ctrl_dff(op_ctrl_dff_out,1'b1,clock,ctrl_MULT,ctrl_DIV);

    //Handle exception

    mux_2_1bit exception_mux(data_exception, op_ctrl_dff_out, div_error, mult_error);

    //Handle data rdy
    assign corrected_div_ready = div_rdy & ~(ctrl_DIV | ctrl_MULT);
    assign corrected_mult_ready = mult_rdy & ~(ctrl_DIV | ctrl_MULT);
    mux_2_1bit ready_mux(data_resultRDY, op_ctrl_dff_out, corrected_div_ready, corrected_mult_ready);

endmodule