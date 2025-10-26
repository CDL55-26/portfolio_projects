module alu(data_operandA, data_operandB, ctrl_ALUopcode, ctrl_shiftamt, data_result, isNotEqual, isLessThan, overflow);
        
    input [31:0] data_operandA, data_operandB;
    input [4:0] ctrl_ALUopcode, ctrl_shiftamt;

    output [31:0] data_result;
    output isNotEqual, isLessThan, overflow;

    // add your code here:

    wire [31:0] CLA32_out, Tcomp_unit_out, BS_out, Bit_AND_out, Bit_OR_out, Logic_out,unused_mux_input;
    
    //pass b through the twos comp buffer, if op[0] is 1, could be sub so flip bits
    Tcomp_unit Tcomp_unit_op(Tcomp_unit_out, data_operandB, ctrl_ALUopcode[0]);

    //CLA adder with inp A and ouput of the TC buffer. Carry in is same as sub bit
    
    wire ovf_wire,A_and_Ovf_wire, MSB_and_notOvf_wire, not_Ovf_wire;
    CLA32 CLA32_op(CLA32_out, ovf_wire, data_operandA, Tcomp_unit_out, ctrl_ALUopcode[0]);

    assign overflow = ovf_wire;

    //Calculate isLessThan
    and A_and_Ovf(A_and_Ovf_wire, data_operandA[31], ovf_wire);

    not not_Ovf(not_Ovf_wire, ovf_wire);
    and MSB_and_noOvf(MSB_and_notOvf_wire, CLA32_out[31], not_Ovf_wire);

    or get_isNotEqual(isLessThan, MSB_and_notOvf_wire, A_and_Ovf_wire);
    

    //calc isNotEqual
    multi_level_Or get_sub_equal(isNotEqual,CLA32_out);

    //Bitwise AND/OR ops
    Bit_AND Bit_AND_op(Bit_AND_out,data_operandA,data_operandB);
    Bit_OR Bit_OR_op(Bit_OR_out,data_operandA,data_operandB);

    mux_2 logic_op(Logic_out,ctrl_ALUopcode[0],Bit_AND_out,Bit_OR_out);

    //LLS or RAS barrel shifter
    Barrel_Shifter BS_op(data_operandA, ctrl_shiftamt, ctrl_ALUopcode[0], BS_out);

    assign unused_mux_input = 32'b0;
    mux_4 ALU_out_mux(data_result,ctrl_ALUopcode[2:1],CLA32_out,Logic_out,BS_out,unused_mux_input);
    

endmodule