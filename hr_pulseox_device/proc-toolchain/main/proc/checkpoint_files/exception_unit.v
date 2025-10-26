module exception_unit(
    output exception,
    input  [31:0] opA,
    input  [31:0] opB
);

  wire [63:0] concat;
  assign concat = {opA, opB};

  wire eq1, eq2;
  assign eq1 = ~(|(concat ^ {32'h00000001, 32'h80000000}));
  assign eq2 = ~(|(concat ^ {32'h80000000, 32'h00000001}));

  assign exception = eq1 | eq2;

endmodule