module control(
    input [4:0] FD_opcode_wire, DX_opcode_wire, XM_opcode_wire, WB_opcode_wire,
    input [4:0] DX_rd_wire, DX_rs_wire,
    input [4:0] DX_ALU_op_wire,
    input [31:0] ALU_A_Bypass_mux_out, ALU_B_Bypass_mux_out,
    output assert_mult, assert_div, ALU_multDiv_mux_control, 
    output PC_ctrl_mux_select, B_J_mux_select, T_rd_mux_select, Pure_jump_bypass_correction_mux_select,
    output rs_rstatus_mux_select, rs_rd_mux_select, rt_rs_mux_select, rt_rd_mux_select,
    output [4:0] DX_opcode_OR,
    output B_Imm_mux_select, ALU_op_mux,
    output RAM_WE,
    output X_D_mux_select, jal_setx_mux_select, WB_mux_select,
    output WB_T_PC1_mux_select, WB_xm_ctrl_mux_select, Reg_WE
);

    /*Wires*/

        // Decoder Wires
        wire [31:0] FD_instruction_decoder, DX_instruction_decoder, XM_instruction_decoder, WB_instruction_decoder;
        wire [31:0] DX_ALU_op_decoder;

        //Control Wires
        wire bext_zero;

        //ALU wires
        wire[31:0]comp_data_result;
        wire comp_isNotEqual, comp_isLessThan, comp_overflow;

    
    // Decoders
    assign FD_instruction_decoder = 32'b1 << FD_opcode_wire;
    assign DX_instruction_decoder = 32'b1 << DX_opcode_wire; // 5-bit decoder with imem output (instruction) as input
    assign XM_instruction_decoder = 32'b1 << XM_opcode_wire;
    assign WB_instruction_decoder = 32'b1 << WB_opcode_wire;

    assign DX_ALU_op_decoder = 32'b1 << DX_ALU_op_wire; //decode the DX alu op

    //Multiplier Control

        assign assert_mult = DX_ALU_op_decoder[6] & ~(|DX_opcode_wire[4:0]);
        assign assert_div = DX_ALU_op_decoder[7] & ~(|DX_opcode_wire[4:0]);
        assign ALU_multDiv_mux_control = (DX_ALU_op_decoder[7] | DX_ALU_op_decoder[6]) & ~(|DX_opcode_wire[4:0]);

    //Control Bits

        alu comp_alu(ALU_A_Bypass_mux_out, ALU_B_Bypass_mux_out, 5'b00001, 5'b00000, comp_data_result, comp_isNotEqual, comp_isLessThan, comp_overflow);

        //1 if a jump or branch
        assign bext_zero = (ALU_A_Bypass_mux_out != 0);
        assign PC_ctrl_mux_select = (DX_instruction_decoder[1] | DX_instruction_decoder[3] | DX_instruction_decoder[4]) | (bext_zero & DX_instruction_decoder[22]) | (DX_instruction_decoder[2] & comp_isNotEqual) | (DX_instruction_decoder[6] & comp_isLessThan);

        //1 if branch 
        assign B_J_mux_select = ((DX_instruction_decoder[2] & comp_isNotEqual) | (DX_instruction_decoder[6] & comp_isLessThan));
        
        //1 if jr instr 
        assign T_rd_mux_select = DX_instruction_decoder[4];

        //1 if pure jump
        assign Pure_jump_bypass_correction_mux_select = DX_instruction_decoder[1] || DX_instruction_decoder[3] || DX_instruction_decoder[22] ;

    // Decode Bits
        //1 if bex T instr -> will read reg 30 into RS 
        assign rs_rstatus_mux_select = FD_instruction_decoder[22];

        //1 if a branch instruction -> will align rd and rs with rs and rt ports of reg
        assign rs_rd_mux_select = (FD_instruction_decoder[4] | FD_instruction_decoder[6] | FD_instruction_decoder[2]);

        //1 if store word, make the B read from rd bits
        assign rt_rd_mux_select =(FD_instruction_decoder[7]);

        assign rt_rs_mux_select = (FD_instruction_decoder[4] | FD_instruction_decoder[6] | FD_instruction_decoder[2]);

    //Execute Bits
        assign DX_opcode_OR = |DX_opcode_wire[4:0]; 

        //1 if an I type instr, will be zero for R types
        assign B_Imm_mux_select = DX_opcode_OR; 
        //1 if I type, should always cause ALU to add
        assign ALU_op_mux = DX_opcode_OR;
    
    //Memory Bits
        // 1 if sw instr
        assign RAM_WE = XM_instruction_decoder[7];

    //Write Back Bits
        //1 if LW, else take execute
        assign X_D_mux_select = WB_instruction_decoder[8]; 

        //1 if JAL instr -> writing to reg 31
        assign jal_setx_mux_select = WB_instruction_decoder[3];

        //1 if JAL or setx
        assign WB_mux_select = (WB_instruction_decoder[3] | WB_instruction_decoder[21]);

        //1 if setx instr
        assign WB_T_PC1_mux_select = WB_instruction_decoder[21];

        //1 if JAL or setx
        assign WB_xm_ctrl_mux_select = (WB_instruction_decoder[21] | WB_instruction_decoder[3]);

        //Reg WE if not these -> sw, j, bne, jr, blt, bext
        assign Reg_WE = (WB_instruction_decoder[0] | WB_instruction_decoder[5] | WB_instruction_decoder[8] | WB_instruction_decoder[3] | WB_instruction_decoder[21]);

endmodule
