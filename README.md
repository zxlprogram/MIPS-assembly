# MIPS Assembly Simulator

A comprehensive MIPS processor simulator and instruction generator, developed as an educational project to understand MIPS architecture and assembly language execution.

## Overview

This project provides a complete MIPS assembly simulation environment consisting of:
- **MIPS Decoder (C)**: A software simulator that decodes and executes MIPS assembly instructions
- **Test Case Generator (C++)**: Generates binary machine code test cases for validation
- **Verilog Implementation**: Hardware-level MIPS processor implementation

## Project Structure

```
MIPS-assembly/
├── MIPSdecoder/           # C-based MIPS simulator
│   └── main.c             # Main decoder implementation
├── MIPSdecoderCpp/        # C++ alternative decoder implementation
├── testCaseGenerator/     # C++ test case generation tool
├── testcase/              # Example test cases (.bin files)
│   └── testcase1.bin      # Binary machine code examples
└── verilogFile/           # Hardware Verilog implementation
    └── input.txt          # Input test vectors
```

## Supported MIPS Instructions

### R-Type Instructions (Register Format)
- `add` - Addition
- `addu` - Unsigned Addition
- `sub` - Subtraction
- `and` - Bitwise AND
- `or` - Bitwise OR
- `slt` - Set on Less Than
- `mult` - Multiplication (stores result in HI/LO registers)
- `div` - Division (quotient in LO, remainder in HI)
- `jr` - Jump Register
- `syscall` - System Call

### I-Type Instructions (Immediate Format)
- `addi` - Add Immediate
- `addiu` - Add Immediate Unsigned
- `beq` - Branch if Equal
- `bne` - Branch if Not Equal
- `lw` - Load Word
- `sw` - Store Word
- `lui` - Load Upper Immediate

### J-Type Instructions (Jump Format)
- `j` - Jump (absolute)
- `jal` - Jump and Link (subroutine call)

## Features

### MIPS Decoder (Software Simulator)
- **32 General-Purpose Registers** (R0-R31) with read-protected R0
- **Special Registers**: HI and LO for multiplication/division results
- **Data Memory**: 1024-word addressable memory
- **32-bit ALU**: Supports ADD, SUB, AND, OR operations
- **Binary File Input**: Reads 32-bit machine code from `.bin` files
- **Execution Tracing**: Detailed instruction execution logs with register state
- **Sign-Extended Arithmetic**: Proper handling of signed and unsigned operations

### Test Case Generator
- Generates binary machine code in `.bin` format
- Supports batch test generation
- Each instruction is encoded as a 32-bit word

### Verilog Implementation
- Hardware-level MIPS processor design
- Intended for FPGA implementation
- Provides low-level control flow and data path implementation

## Building & Compilation

### Requirements
- **C Compiler**: GCC (or compatible)
- **C++ Compiler**: G++ (or compatible)
- **IDE**: Code::Blocks (recommended for development)
- **Hardware Simulation**: ModelSim or Intel FPGA 17.1 (for Verilog)

### Compiling the MIPS Decoder (C)
```bash
cd MIPSdecoder
gcc -o mips_decoder main.c
```

### Compiling the C++ Decoder (Alternative)
```bash
cd MIPSdecoderCpp
g++ -o mips_decoder_cpp *.cpp
```

### Compiling the Test Case Generator
```bash
cd testCaseGenerator
g++ -o test_gen *.cpp
```

## Usage

### Running the MIPS Simulator
```bash
./mips_decoder
```

The simulator expects a binary file named `fibonacci.bin` in the working directory containing 32-bit machine code instructions.

**Example execution output:**
```
success to load file, include with 16 commands
add R3, R1, R2 (result: 13)
sub R4, R3, R1 (result: 3)
beq compare R3 and R4 -> (result: 0)
addi R2, R1, 5 (result: 15)
...
==================== REGISTER DUMP ====================
R0: 0    R1: 10    R2: 3    R3: 13
R4: 3    R5: 0    R6: 0    R7: 0
...
HI: 0    LO: 0    PC: 16
=======================================================
```

### Example Input Format
The test case should be a binary file containing 32-bit MIPS machine code. See `verilogFile/input.txt` for hexadecimal examples:
```
24080007  # Load immediate (instruction 1)
24100000  # Load immediate (instruction 2)
0109502a  # Set less than (instruction 3)
...
```

## Binary Machine Code Format

Each instruction is encoded as a 32-bit word:

- **R-Type**: `opcode[6] | rs[5] | rt[5] | rd[5] | shamt[5] | funct[6]`
- **I-Type**: `opcode[6] | rs[5] | rt[5] | immediate[16]`
- **J-Type**: `opcode[6] | target[26]`

### Example: `add R3, R1, R2`
Binary: `00000000001000100001100000100000`
- Opcode: `000000` (R-type)
- RS: `00001` (R1)
- RT: `00010` (R2)
- RD: `00011` (R3)
- SHAMT: `00000`
- FUNCT: `100000` (ADD)

## Register Information

### General-Purpose Registers
| Register | Number | Special Use |
|----------|--------|-------------|
| $zero    | R0     | Always 0 (read-only) |
| $at      | R1     | Assembler Temporary |
| $v0-$v1  | R2-R3  | Function Return Values |
| $a0-$a3  | R4-R7  | Function Arguments |
| $t0-$t7  | R8-R15 | Temporary Registers |
| $s0-$s7  | R16-R23| Saved Registers |
| $t8-$t9  | R24-R25| Temporary Registers |
| $k0-$k1  | R26-R27| Kernel Registers |
| $gp      | R28    | Global Pointer |
| $sp      | R29    | Stack Pointer |
| $fp      | R30    | Frame Pointer |
| $ra      | R31    | Return Address |

### Special Registers
- **HI**: Upper 32 bits of multiplication result / Remainder of division
- **LO**: Lower 32 bits of multiplication result / Quotient of division
- **PC**: Program Counter (tracks instruction line number)

## System Calls

The simulator supports basic system calls via `syscall`:

| $v0 Value | Function | Effect |
|-----------|----------|--------|
| 1         | Print Integer | Prints value in $a0 |
| 10        | Exit Program  | Terminates execution |

**Example:**
```c
// Print the value 42
$v0 = 1        // Set syscall code
$a0 = 42       // Set value to print
syscall        // Execute
```

## Project Status

⚠️ **Under Development** - This is an educational project still in active development. Features may be added or modified.

### Known Limitations
- Limited instruction set (core MIPS-I subset)
- No floating-point support
- No advanced memory management
- Fixed memory size (1024 words)

### Future Enhancements
- [ ] Support for more MIPS instructions (shifts, rotates, etc.)
- [ ] Floating-point unit implementation
- [ ] Enhanced test case generation
- [ ] Interactive debugger
- [ ] Performance profiling
- [ ] Complete pipelining in Verilog

## Testing

### Running Test Cases

1. **Generate a test binary** using the test case generator:
```bash
./test_gen > testcase.bin
```

2. **Place the binary** in the working directory:
```bash
cp testcase.bin fibonacci.bin
```

3. **Run the simulator**:
```bash
./mips_decoder
```

4. **Verify output** against expected results

### Example Test Cases
See the `testcase/` directory for pre-built binary test files and `verilogFile/input.txt` for hexadecimal machine code examples.

## Development Tools

- **IDE**: Code::Blocks
- **C/C++ Compilers**: GCC, G++
- **Hardware Simulation**: ModelSim or Intel FPGA 17.1
- **Version Control**: Git

## How It Works

### Execution Flow

```
Binary File (.bin) 
        ↓
 Read 32-bit words
        ↓
 Decode Instruction
 ├─ Extract opcode
 ├─ Extract operands
 └─ Identify instruction type
        ↓
 Execute Instruction
 ├─ ALU operations
 ├─ Register updates
 ├─ Memory access
 └─ PC updates
        ↓
 Continue until:
  - Program terminates (syscall 10)
  - PC reaches end of file
        ↓
 Dump final register state
```

### ALU Implementation

The simulator implements a 1-bit ALU that's chained for 32-bit operations:

```c
bool ALU1bit(a, b, cin, invertB, operation) -> result_bit, cout
```

Supported ALU operations:
- `00`: AND
- `01`: OR
- `10`: ADD/SUB (configurable via invertB)

## Contact & Contribution

This is an educational project developed as part of a computer architecture assignment. 

---

**Note**: This project is a work-in-progress. Documentation will be updated as new features are added.
