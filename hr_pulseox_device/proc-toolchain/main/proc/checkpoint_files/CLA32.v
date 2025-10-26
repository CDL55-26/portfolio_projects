module CLA32(
    output [31:0] Stotal,
    output OvF,
    input [31:0] A, B,
    input C0
);

wire [7:0] S0, S1, S2, S3;
wire P3, P2, P1, P0, G3, G2, G1, G0;
wire C8,C16,C24,C32;

//Create CLA8 units
CLA8 cla8_unit0(.S(S0), .G(G0), .P(P0), .OvF(), .A(A[7:0]), .B(B[7:0]), .C0(C0));
CLA8 cla8_unit1(.S(S1), .G(G1), .P(P1), .OvF(), .A(A[15:8]), .B(B[15:8]), .C0(C8));
CLA8 cla8_unit2(.S(S2), .G(G2), .P(P2), .OvF(), .A(A[23:16]), .B(B[23:16]), .C0(C16));
CLA8 cla8_unit3(.S(S3), .G(G3), .P(P3), .OvF(OvF), .A(A[31:24]), .B(B[31:24]), .C0(C24));

// Combine wires into output Stotal
assign Stotal = {S3, S2, S1, S0};

//Calculate the carries 

    // C8
    wire w1;
    and C8_AND_0(w1, P0, C0);
    or C8_OR(C8, w1, G0);
    
    // C16
    wire w2, w3;
    and C16_AND_0(w2, P1, P0, C0);
    and C16_AND_1(w3, P1, G0);
    or C16_OR(C16, w2, w3, G1);

    // C24
    wire w4, w5, w6;
    and C24_AND_0(w4, P2, P1, P0, C0);
    and C24_AND_1(w5, P2, P1, G0);
    and C24_AND_2(w6, P2, G1);
    or C24_OR(C24, w4, w5, w6, G2);

endmodule
