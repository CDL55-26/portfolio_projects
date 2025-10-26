module regfile (
	clock,
	ctrl_writeEnable, ctrl_reset, ctrl_writeReg,
	ctrl_readRegA, ctrl_readRegB, data_writeReg,
	data_readRegA, data_readRegB
);

	input clock, ctrl_writeEnable, ctrl_reset;
	input [4:0] ctrl_writeReg, ctrl_readRegA, ctrl_readRegB;
	input [31:0] data_writeReg;

	output [31:0] data_readRegA, data_readRegB;

	// add your code here

// assign out = enable << select; decoder 
//bufif1 my_buf (out, in, enable); tri state

wire [31:0] read_ctrlA, read_ctrlB, write_decode;

//Read Decoders
assign read_ctrlA = 32'b1 << ctrl_readRegA;
assign read_ctrlB = 32'b1 << ctrl_readRegB;

//Write Decoder
assign write_decode = ctrl_writeEnable ? (32'b1 << ctrl_writeReg) : 32'b0;

//zero register

wire [31:0] w0;

//enable always zero for zero register
register zero_register (w0,data_writeReg,ctrl_reset,1'b0,clock);
tristate32 zero_bufA (data_readRegA,w0,read_ctrlA[0]);//fill in with correct decoder
tristate32 zero_bufB (data_readRegB,w0,read_ctrlB[0]);

genvar i;

generate
    for (i = 1; i < 32; i = i + 1) begin : gen_regfile_buff
	
	wire [31:0] reg_out;
    
	register reg_inst (
		reg_out,
		data_writeReg,
		ctrl_reset,
		write_decode[i],
		clock
	);

	tristate32 buffA (
		data_readRegA,
		reg_out,
		read_ctrlA[i]
	);

	tristate32 buffB (
		data_readRegB,
		reg_out,
		read_ctrlB[i]
	);

    end
endgenerate

endmodule
