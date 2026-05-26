// ============================================================
//  MIPS CPU — Single-Cycle Implementation
//  Translated from C simulator
//
//  Supported instructions:
//    R-format : add, sub, and, or, mult, div, jr
//    I-format : beq, bne, lw, sw
//    J-format : j, jal
// ============================================================

`timescale 1ns/1ps

// ------------------------------------------------------------
//  1-bit ALU cell (matches ALU1bit in C)
// ------------------------------------------------------------
module alu_1bit (
    input  wire a,
    input  wire b,
    input  wire cin,
    input  wire invertB,
    input  wire [1:0] operation,   // 0=AND 1=OR 2=ADD/SUB
    output wire result,
    output wire cout
);
    wire real_b = invertB ? ~b : b;
    wire sum    = a ^ real_b ^ cin;
    assign cout = (a & real_b) | (cin & (a ^ real_b));

    assign result = (operation == 2'd0) ? (a & real_b) :
                    (operation == 2'd1) ? (a | real_b) :
                                          sum;
endmodule


// ------------------------------------------------------------
//  32-bit Ripple-Carry ALU (matches ALU in C)
//  ctrl[2:0] :
//    3'b000 = AND
//    3'b001 = OR
//    3'b010 = ADD
//    3'b110 = SUB
// ------------------------------------------------------------
module alu_32bit (
    input  wire [2:0]  ctrl,
    input  wire [31:0] A,
    input  wire [31:0] B,
    output wire [31:0] result,
    output wire        overflow
);
    wire invertB  = ctrl[2];              // SUB sets bit[2]
    wire [1:0] op = ctrl[1:0];
    wire [32:0] carry;
    assign carry[0] = invertB;           // cin = 1 for SUB (two's complement)

    genvar i;
    generate
        for (i = 0; i < 32; i = i + 1) begin : alu_chain
            alu_1bit u_cell (
                .a        (A[i]),
                .b        (B[i]),
                .cin      (carry[i]),
                .invertB  (invertB),
                .operation(op),
                .result   (result[i]),
                .cout     (carry[i+1])
            );
        end
    endgenerate

    assign overflow = carry[32] ^ carry[31];
endmodule


// ------------------------------------------------------------
//  Instruction Memory  (ROM — read from parameter or $readmemb)
// ------------------------------------------------------------
module instr_mem #(
    parameter DEPTH = 256
)(
    input  wire [7:0]  addr,       // word address (= PC)
    output wire [31:0] instr
);
    reg [31:0] mem [0:DEPTH-1];
    integer k;
    initial begin
        for (k = 0; k < DEPTH; k = k + 1) mem[k] = 32'b0;
        // Load binary file produced by assembler
        $readmemb("input_text.bin", mem);
    end
    assign instr = mem[addr];
endmodule


// ------------------------------------------------------------
//  Data Memory  (initialised like the C simulator)
// ------------------------------------------------------------
module data_mem #(
    parameter DEPTH = 1024
)(
    input  wire        clk,
    input  wire        we,
    input  wire [9:0]  addr,
    input  wire [31:0] wdata,
    output wire [31:0] rdata
);
    reg [31:0] mem [0:DEPTH-1];
    integer k;
    initial begin
        for (k = 0; k < DEPTH; k = k + 1) mem[k] = 32'b0;
        mem[8] = 32'd99;    // matches C: data_mem[8] = 99
    end
    always @(posedge clk) begin
        if (we) mem[addr] <= wdata;
    end
    assign rdata = mem[addr];
endmodule


// ------------------------------------------------------------
//  Register File  (32 × 32-bit, R0 hard-wired to 0)
//  Initial values match the C simulator:
//    R1 = 10  (bit 28 and bit 30 set  → 2+8=10, little-endian index)
//    R2 =  3  (bit 30 and bit 31 set  → 1+2=3)
//  NOTE: the C code uses big-endian bit ordering inside each int[32],
//        so reg[1][28]=1, reg[1][30]=1 → value = 2^(31-28)+2^(31-30) = 8+2 = 10
// ------------------------------------------------------------
module reg_file (
    input  wire        clk,
    input  wire        we,
    input  wire [4:0]  ra,   // read port A
    input  wire [4:0]  rb,   // read port B
    input  wire [4:0]  rw,   // write port
    input  wire [31:0] wdata,
    output wire [31:0] qa,
    output wire [31:0] qb
);
    reg [31:0] regs [0:31];
    integer k;
    initial begin
        for (k = 0; k < 32; k = k + 1) regs[k] = 32'b0;
        regs[1] = 32'd10;   // R1 = 10
        regs[2] = 32'd3;    // R2 = 3
    end

    // R0 is always 0 (write ignored)
    always @(posedge clk) begin
        if (we && rw != 5'd0) regs[rw] <= wdata;
    end

    assign qa = (ra == 5'd0) ? 32'd0 : regs[ra];
    assign qb = (rb == 5'd0) ? 32'd0 : regs[rb];
endmodule


// ------------------------------------------------------------
//  32×32-bit Multiplier  (shift-and-add, matches mult() in C)
//  Result: {HI, LO} = rs × rt  (signed)
//  Purely combinational for single-cycle; use clocked version
//  in a multi-cycle design.
// ------------------------------------------------------------
module multiplier (
    input  wire signed [31:0] rs,
    input  wire signed [31:0] rt,
    output wire        [31:0] hi,
    output wire        [31:0] lo
);
    wire signed [63:0] product = rs * rt;
    assign hi = product[63:32];
    assign lo = product[31:0];
endmodule


// ------------------------------------------------------------
//  32-bit Divider  (signed, matches div_mips() in C)
// ------------------------------------------------------------
module divider (
    input  wire signed [31:0] rs,
    input  wire signed [31:0] rt,
    output wire        [31:0] hi,   // remainder
    output wire        [31:0] lo    // quotient
);
    assign lo = (rt != 32'd0) ? $signed(rs) / $signed(rt) : 32'hDEAD_BEEF;
    assign hi = (rt != 32'd0) ? $signed(rs) % $signed(rt) : 32'hDEAD_BEEF;
endmodule


// ------------------------------------------------------------
//  Sign-Extender  (16 → 32 bits)
// ------------------------------------------------------------
module sign_extend (
    input  wire [15:0] in,
    output wire [31:0] out
);
    assign out = {{16{in[15]}}, in};
endmodule


// ------------------------------------------------------------
//  Control Unit
//  Decodes opcode (and funct for R-type) into control signals
// ------------------------------------------------------------
module control_unit (
    input  wire [5:0]  opcode,
    input  wire [5:0]  funct,

    // register-file controls
    output reg         reg_dst,    // 1=rd, 0=rt
    output reg         reg_write,
    output reg         link,       // jal writes PC+1 to R31

    // ALU controls
    output reg  [2:0]  alu_ctrl,   // passed directly to alu_32bit
    output reg         alu_src,    // 1=sign-extended imm, 0=reg

    // memory controls
    output reg         mem_read,
    output reg         mem_write,
    output reg         mem_to_reg, // 1=load from memory

    // branch / jump controls
    output reg         branch_eq,  // beq
    output reg         branch_ne,  // bne
    output reg         jump,       // j / jal
    output reg         jumpreg,    // jr

    // HI/LO write enables
    output reg         hilo_write, // mult / div
    output reg         is_mult     // 1=mult, 0=div
);
    always @(*) begin
        // defaults (NOP)
        reg_dst    = 0; reg_write  = 0; link       = 0;
        alu_ctrl   = 3'b010; alu_src = 0;
        mem_read   = 0; mem_write  = 0; mem_to_reg = 0;
        branch_eq  = 0; branch_ne  = 0;
        jump       = 0; jumpreg    = 0;
        hilo_write = 0; is_mult    = 0;

        case (opcode)
            6'd0: begin   // R-format
                case (funct)
                    6'd8:  begin jumpreg = 1;                                    end // jr
                    6'd24: begin hilo_write = 1; is_mult = 1;                   end // mult
                    6'd26: begin hilo_write = 1; is_mult = 0;                   end // div
                    6'd32: begin reg_dst=1; reg_write=1; alu_ctrl=3'b010;       end // add
                    6'd34: begin reg_dst=1; reg_write=1; alu_ctrl=3'b110;       end // sub
                    6'd36: begin reg_dst=1; reg_write=1; alu_ctrl=3'b000;       end // and
                    6'd37: begin reg_dst=1; reg_write=1; alu_ctrl=3'b001;       end // or
                    default: ;
                endcase
            end

            6'd4:  begin branch_eq = 1; alu_ctrl = 3'b110;                      end // beq
            6'd5:  begin branch_ne = 1; alu_ctrl = 3'b110;                      end // bne
            6'd35: begin alu_src=1; mem_read=1; reg_write=1; mem_to_reg=1;      end // lw
            6'd43: begin alu_src=1; mem_write=1; alu_ctrl=3'b010;               end // sw
            6'd2:  begin jump = 1;                                               end // j
            6'd3:  begin jump = 1; link = 1; reg_write = 1;                     end // jal
            default: ;
        endcase
    end
endmodule


// ------------------------------------------------------------
//  TOP-LEVEL SINGLE-CYCLE MIPS CPU
// ------------------------------------------------------------
module mips_cpu (
    input  wire clk,
    input  wire rst
);
    // ---- PC ----
    reg  [7:0]  pc;
    wire [7:0]  pc_next_seq = pc + 8'd1;

    // ---- Instruction fields ----
    wire [31:0] instr;
    wire [5:0]  opcode = instr[31:26];
    wire [4:0]  rs     = instr[25:21];
    wire [4:0]  rt     = instr[20:16];
    wire [4:0]  rd     = instr[15:11];
    // shamt = instr[10:6]  (not used in this ISA subset)
    wire [5:0]  funct  = instr[5:0];
    wire [15:0] immed  = instr[15:0];
    wire [25:0] target = instr[25:0];

    // ---- Control signals ----
    wire reg_dst, reg_write, link;
    wire [2:0] alu_ctrl;
    wire alu_src;
    wire mem_read, mem_write, mem_to_reg;
    wire branch_eq, branch_ne, jump, jumpreg;
    wire hilo_write, is_mult;

    // ---- Register file ----
    wire [4:0]  rw_addr  = link    ? 5'd31 :
                           reg_dst ? rd    : rt;
    wire [31:0] reg_qa, reg_qb;
    wire [31:0] reg_wdata;
    wire        rf_we = reg_write;

    reg  [31:0] HI, LO;   // HI / LO special registers

    // ---- Sign extension ----
    wire [31:0] sign_ext_imm;

    // ---- ALU ----
    wire [31:0] alu_b    = alu_src ? sign_ext_imm : reg_qb;
    wire [31:0] alu_out;
    wire        alu_ovf;

    // ---- Data memory ----
    wire [31:0] dmem_rdata;

    // ---- Multiplier / Divider ----
    wire [31:0] mult_hi, mult_lo;
    wire [31:0] div_hi,  div_lo;

    // ---- Branch / Jump PC ----
    wire        alu_zero    = (alu_out == 32'd0);
    wire        take_branch = (branch_eq & alu_zero) | (branch_ne & ~alu_zero);
    wire [7:0]  branch_pc   = pc_next_seq + sign_ext_imm[7:0]; // word-offset branch
    wire [7:0]  jump_pc     = target[7:0];
    wire [7:0]  jr_pc       = reg_qa[7:0];

    wire [7:0]  pc_next =
        jumpreg    ? jr_pc      :
        jump       ? jump_pc    :
        take_branch? branch_pc  :
                     pc_next_seq;

    // ---- Write-back data ----
    assign reg_wdata =
        link       ? {24'd0, pc_next_seq} :   // jal: save return address
        mem_to_reg ? dmem_rdata            :
                     alu_out;

    // ===================== Module Instances =====================

    instr_mem #(.DEPTH(256)) u_imem (
        .addr  (pc),
        .instr (instr)
    );

    reg_file u_rf (
        .clk   (clk),
        .we    (rf_we),
        .ra    (rs),
        .rb    (rt),
        .rw    (rw_addr),
        .wdata (reg_wdata),
        .qa    (reg_qa),
        .qb    (reg_qb)
    );

    sign_extend u_sext (
        .in  (immed),
        .out (sign_ext_imm)
    );

    alu_32bit u_alu (
        .ctrl   (alu_ctrl),
        .A      (reg_qa),
        .B      (alu_b),
        .result (alu_out),
        .overflow(alu_ovf)
    );

    data_mem #(.DEPTH(1024)) u_dmem (
        .clk   (clk),
        .we    (mem_write),
        .addr  (alu_out[9:0]),
        .wdata (reg_qb),
        .rdata (dmem_rdata)
    );

    multiplier u_mult (
        .rs (reg_qa),
        .rt (reg_qb),
        .hi (mult_hi),
        .lo (mult_lo)
    );

    divider u_div (
        .rs (reg_qa),
        .rt (reg_qb),
        .hi (div_hi),
        .lo (div_lo)
    );

    control_unit u_ctrl (
        .opcode     (opcode),
        .funct      (funct),
        .reg_dst    (reg_dst),
        .reg_write  (reg_write),
        .link       (link),
        .alu_ctrl   (alu_ctrl),
        .alu_src    (alu_src),
        .mem_read   (mem_read),
        .mem_write  (mem_write),
        .mem_to_reg (mem_to_reg),
        .branch_eq  (branch_eq),
        .branch_ne  (branch_ne),
        .jump       (jump),
        .jumpreg    (jumpreg),
        .hilo_write (hilo_write),
        .is_mult    (is_mult)
    );

    // ===================== Sequential Logic =====================

    always @(posedge clk or posedge rst) begin
        if (rst) begin
            pc <= 8'd0;
            HI <= 32'd0;
            LO <= 32'd0;
        end else begin
            pc <= pc_next;

            // Update HI/LO on mult or div
            if (hilo_write) begin
                if (is_mult) begin
                    HI <= mult_hi;
                    LO <= mult_lo;
                end else begin
                    HI <= div_hi;   // remainder
                    LO <= div_lo;   // quotient
                end
            end
        end
    end

endmodule


// ============================================================
//  Testbench
// ============================================================

module tb_mips_cpu;
    reg clk, rst;

    mips_cpu dut (.clk(clk), .rst(rst));

    // 10 ns clock
    initial clk = 0;
    always #5 clk = ~clk;

    initial begin
        $dumpfile("mips_cpu.vcd");
        $dumpvars(0, tb_mips_cpu);

        rst = 1;
        @(posedge clk); #1;
        rst = 0;

        // Run for enough cycles to finish a small program
        repeat (200) @(posedge clk);

        // Dump register state (R0–R31)
        $display("\n===== REGISTER DUMP =====");
        begin : dump
            integer i;
            for (i = 0; i < 32; i = i + 1)
                $display("R%0d = %0d", i, $signed(dut.u_rf.regs[i]));
        end
        $display("HI = %0d", $signed(dut.HI));
        $display("LO = %0d", $signed(dut.LO));
        $display("PC = %0d", dut.pc);
        $display("=========================");

        $finish;
    end
endmodule

