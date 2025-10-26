module CLA8(
    output [7:0] S,
    output G,
    output P,
    output OvF,
    input [7:0] A, B,
    input C0
);

    wire C1, C2, C3, C4, C5, C6, C7, Cout; 
    
    //Getting S bits
    full_adder fadder0(S[0], A[0], B[0], C0);
    full_adder fadder1(S[1], A[1], B[1], C1);
    full_adder fadder2(S[2], A[2], B[2], C2);
    full_adder fadder3(S[3], A[3], B[3], C3);
    full_adder fadder4(S[4], A[4], B[4], C4);
    full_adder fadder5(S[5], A[5], B[5], C5);
    full_adder fadder6(S[6], A[6], B[6], C6);
    full_adder fadder7(S[7], A[7], B[7], C7);

    //Create p wires
    wire p0, p1, p2, p3, p4, p5, p6, p7;

    or or_p0(p0, A[0], B[0]);
    or or_p1(p1, A[1], B[1]);
    or or_p2(p2, A[2], B[2]);
    or or_p3(p3, A[3], B[3]);
    or or_p4(p4, A[4], B[4]);
    or or_p5(p5, A[5], B[5]);
    or or_p6(p6, A[6], B[6]);
    or or_p7(p7, A[7], B[7]);

    //Create g wires
    wire g0, g1, g2, g3, g4, g5, g6, g7;

    and and_g0(g0, A[0], B[0]);
    and and_g1(g1, A[1], B[1]);
    and and_g2(g2, A[2], B[2]);
    and and_g3(g3, A[3], B[3]);
    and and_g4(g4, A[4], B[4]);
    and and_g5(g5, A[5], B[5]);
    and and_g6(g6, A[6], B[6]);
    and and_g7(g7, A[7], B[7]);

    //Get P output of CLA unit
    and get_P(P,p7,p6,p5,p4,p3,p2,p1,p0);
    
    //Get G output of CLA unit
    wire wB,wC,wD,wE,wF,wG,wH;

    and g_and_0(wB, p7, p6, p5, p4, p3, p2, p1, g0);
    and g_and_1(wC, p7, p6, p5, p4, p3, p2, g1);
    and g_and_2(wD, p7, p6, p5, p4, p3, g2);
    and g_and_3(wE, p7, p6, p5, p4, g3);
    and g_and_4(wF, p7, p6, p5, g4);
    and g_and_5(wG, p7, p6, g5);
    and g_and_6(wH, p7, g6);
    or get_G(G, wB, wC, wD, wE, wF, wG, wH, g7);

    //Get Overflow 
    xor get_overflow(OvF,C7,Cout); //Cout defined at bottom

//Get carry in bits

    // C1
    wire w1;
    and c1_and_0(w1, p0, C0);
    or c1_or(C1, w1, g0);
    
    // C2
    wire w2, w3;
    and c2_and_0(w2, p1, p0, C0);
    and c2_and_1(w3, p1, g0);
    or c2_or(C2, w2, w3, g1);

    // C3
    wire w4, w5, w6;
    and c3_and_0(w4, p2, p1, p0, C0);
    and c3_and_1(w5, p2, p1, g0);
    and c3_and_2(w6, p2, g1);
    or c3_or(C3, w4, w5, w6, g2);

    // C4
    wire w7, w8, w9, w10;
    and c4_and_0(w7, p3, p2, p1, p0, C0);
    and c4_and_1(w8, p3, p2, p1, g0);
    and c4_and_2(w9, p3, p2, g1);
    and c4_and_3(w10, p3, g2);
    or c4_or(C4, w7, w8, w9, w10, g3);

    // C5
    wire w11, w12, w13, w14, w15;
    and c5_and_0(w11, p4, p3, p2, p1, p0, C0);
    and c5_and_1(w12, p4, p3, p2, p1, g0);
    and c5_and_2(w13, p4, p3, p2, g1);
    and c5_and_3(w14, p4, p3, g2);
    and c5_and_4(w15, p4, g3);
    or c5_or(C5, w11, w12, w13, w14, w15, g4);

    // C6
    wire w16, w17, w18, w19, w20, w21;
    and c6_and_0(w16, p5, p4, p3, p2, p1, p0, C0);
    and c6_and_1(w17, p5, p4, p3, p2, p1, g0);
    and c6_and_2(w18, p5, p4, p3, p2, g1);
    and c6_and_3(w19, p5, p4, p3, g2);
    and c6_and_4(w20, p5, p4, g3);
    and c6_and_5(w21, p5, g4);
    or c6_or(C6, w16, w17, w18, w19, w20, w21, g5);

    // C7
    wire w22, w23, w24, w25, w26, w27, w28;
    and c7_and_0(w22, p6, p5, p4, p3, p2, p1, p0, C0);
    and c7_and_1(w23, p6, p5, p4, p3, p2, p1, g0);
    and c7_and_2(w24, p6, p5, p4, p3, p2, g1);
    and c7_and_3(w25, p6, p5, p4, p3, g2);
    and c7_and_4(w26, p6, p5, p4, g3);
    and c7_and_5(w27, p6, p5, g4);
    and c7_and_6(w28, p6, g5);
    or c7_or(C7, w22, w23, w24, w25, w26, w27, w28, g6);

    // Cout
    wire w29, w30, w31, w32, w33, w34, w35, w36;
    and cout_and_0(w29, p7, p6, p5, p4, p3, p2, p1, p0, C0);
    and cout_and_1(w30, p7, p6, p5, p4, p3, p2, p1, g0);
    and cout_and_2(w31, p7, p6, p5, p4, p3, p2, g1);
    and cout_and_3(w32, p7, p6, p5, p4, p3, g2);
    and cout_and_4(w33, p7, p6, p5, p4, g3);
    and cout_and_5(w34, p7, p6, p5, g4);
    and cout_and_6(w35, p7, p6, g5);
    and cout_and_7(w36, p7, g6);
    or cout_or(Cout, w29, w30, w31, w32, w33, w34, w35, w36, g7);

    
endmodule

    