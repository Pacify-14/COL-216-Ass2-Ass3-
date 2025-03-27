#include <bits/stdc++.h>
#include <fstream>
#include <iomanip> // Include this for setw()
#include <sstream>
#include <filesystem> 

using namespace std;
namespace fs = std::filesystem;

vector<uint32_t> dmem(1000, 0);
vector<int32_t> reg_set(32,0); // set of 32 registers x0 - x31
int clock_cycle = 0;
int prev_IF = 0;
int spaces = 0;
bool stall = false;
void printRegisters() {
    for (int i = 0; i < 32; ++i) {
        cout << "x" << i << ": " << reg_set[i] << endl;
    }
}

// Utility functions
uint32_t binToUint(const std::string &binStr) {
    uint32_t value = 0;
    for (char c : binStr) {
        value <<= 1;
        if (c == '1') value |= 1;
    }
    return value;
}

int32_t signExtend32(uint32_t imm, int bits) {
    int32_t mask = 1 << (bits - 1);
    int32_t extended = imm;
    if (extended & mask)
        extended |= (-1 << bits);
    return extended;
}

string hex_to_bin(char c) {
    static string lookup[] = {
        "0000", "0001", "0010", "0011",
        "0100", "0101", "0110", "0111",
        "1000", "1001", "1010", "1011",
        "1100", "1101", "1110", "1111"
    };

    if (c >= '0' && c <= '9') return lookup[c - '0'];
    if (c >= 'a' && c <= 'f') return lookup[c - 'a' + 10];
    return "Invalid";
}

class Processor {
public:
    int pc = 0; // program counter
    
    // Pipeline Stage Registers
    struct IF_ID_reg {
        int cur_pc;
        string out;
    };

    // Type-specific instruction structs
    struct R_type_ID_EX_reg {
        uint32_t funct7;
        uint32_t rs2;
        uint32_t rs1;
        uint32_t funct3;
        uint32_t rd;
        uint32_t opcode;
        string op;
    };
    
    struct I_type_ID_EX_reg {
        int32_t imm;
        uint32_t rs1;
        uint32_t funct3;
        uint32_t rd;
        uint32_t opcode;
        string op;
    };
    
    struct S_type_ID_EX_reg {
        int32_t imm;
        uint32_t rs2;
        uint32_t rs1;
        uint32_t funct3;
        uint32_t opcode;
        string op;
    };
    
    struct U_type_ID_EX_reg {
        uint32_t imm;
        uint32_t rd;
        uint32_t opcode;
    };
    
    struct B_type_ID_EX_reg {
        int32_t imm;
        uint32_t rs2;
        uint32_t rs1;
        uint32_t funct3;
        uint32_t opcode;
        string op;
    };
    struct L_type_ID_EX_reg {
        int32_t imm;      // Immediate offset (signed)
        uint32_t rs1;     // Base register
        uint32_t funct3;  // Function code to specify load type (e.g., byte, halfword, word)
        uint32_t rd;      // Destination register
        uint32_t opcode;  // Opcode for load instructions
        std::string op;   // Instruction name (e.g., "LW", "LH", "LB")
    };
    
    struct J_type_ID_EX_reg {
        int32_t imm;
        uint32_t rd;
        uint32_t opcode;
    };

    // Pipeline Stage Registers
    IF_ID_reg if_id;
    L_type_ID_EX_reg l_type_ID_EX_reg;
    R_type_ID_EX_reg r_type_ID_EX_reg;
    I_type_ID_EX_reg i_type_ID_EX_reg;
    U_type_ID_EX_reg u_type_ID_EX_reg;
    S_type_ID_EX_reg stype;
    B_type_ID_EX_reg btype;
    J_type_ID_EX_reg jtype;
    
    char inst_type;

    // For hazard detection: track which registers are read/written by current instruction
    uint32_t currentRs1 = 0;
    uint32_t currentRs2 = 0;
    uint32_t currentRd  = 0;
    bool     currentRegWrite = false;

    // Pipeline Stage Register for EX/MEM
    struct EX_MEM_reg {
        uint32_t aluResult;
        uint32_t rd;
        uint32_t storeData;
        bool memRead;
        bool memWrite;
        bool regWrite;
        string op;
        char inst_type;
    } ex_mem;

    // Pipeline Stage Register for MEM/WB
    struct MEM_WB_reg {
        uint32_t memData;
        uint32_t aluResult;
        uint32_t rd;
        bool regWrite;
        bool memToReg;
    } mem_wb;

public:
    // Constructor to initialize register set


    // Instruction Fetch Stage
    void IF(IF_ID_reg &if_id, int cur_pc, const vector<string>& bin_inst) {
        // Just fetch the binary string at index (cur_pc-1)
        if_id.cur_pc = cur_pc;
        if (cur_pc > 0 && cur_pc <= (int)bin_inst.size()) {
            if_id.out = bin_inst[cur_pc - 1];
        } else {
            cerr << "ERROR: Invalid PC or Instruction Memory Access\n";
        }
    }

    // Instruction Decode Stage
    void ID(IF_ID_reg &if_id) {
        
        string opcode = if_id.out.substr(25, 7);
        inst_type = 'X'; // default

        // Reset the "current" read/write registers
        currentRs1 = 0;
        currentRs2 = 0;
        currentRd  = 0;
        currentRegWrite = false;

        if (opcode == "0110011") {
            // R-type
            inst_type = 'R';
            r_type_ID_EX_reg.funct7 = binToUint(if_id.out.substr(0, 7));
            r_type_ID_EX_reg.rs2    = binToUint(if_id.out.substr(7, 5));
            r_type_ID_EX_reg.rs1    = binToUint(if_id.out.substr(12, 5));
            r_type_ID_EX_reg.funct3 = binToUint(if_id.out.substr(17, 3));
            r_type_ID_EX_reg.rd     = binToUint(if_id.out.substr(20, 5));
            r_type_ID_EX_reg.opcode = binToUint(opcode);

            // Identify the operation
            uint32_t f7 = r_type_ID_EX_reg.funct7;
            uint32_t f3 = r_type_ID_EX_reg.funct3;
            string &op   = r_type_ID_EX_reg.op;
            if(f7 == 32 && f3 == 0) op = "sub";
            if(f7 == 0  && f3 == 0) op = "add";
            if(f7 == 0  && f3 == 1) op = "sll";
            if(f7 == 0  && f3 == 2) op = "slt";
            if(f7 == 0  && f3 == 3) op = "sltu";
            if(f7 == 0  && f3 == 4) op = "xor";
            if(f7 == 0  && f3 == 5) op = "srl";
            if(f7 == 32 && f3 == 5) op = "sra";
            if(f7 == 0  && f3 == 6) op = "or";
            if(f7 == 0  && f3 == 7) op = "and";
            if(f7 == 1  && f3 == 0) op = "mul";
            if(r_type_ID_EX_reg.funct7 == 1 && r_type_ID_EX_reg.funct3 == 1)op = "mulh";
            if(r_type_ID_EX_reg.funct7 == 1 && r_type_ID_EX_reg.funct3 == 2)op = "mulhsu";
            if(r_type_ID_EX_reg.funct7 == 1 && r_type_ID_EX_reg.funct3 == 3)op = "mulhu";
            if(r_type_ID_EX_reg.funct7 == 1 && r_type_ID_EX_reg.funct3 == 4)op = "div";
            if(r_type_ID_EX_reg.funct7 == 1 && r_type_ID_EX_reg.funct3 == 5)op = "divu";
            if(r_type_ID_EX_reg.funct7 == 1 && r_type_ID_EX_reg.funct3 == 6)op = "rem";
            if(r_type_ID_EX_reg.funct7 == 1 && r_type_ID_EX_reg.funct3 == 7)op = "remu";

            // For hazard detection
            currentRs1 = r_type_ID_EX_reg.rs1;
            currentRs2 = r_type_ID_EX_reg.rs2;
            currentRd  = r_type_ID_EX_reg.rd;
            currentRegWrite = true;  // R-type writes a register
        }
        else if (opcode == "0000011") {
            // L-type (Load instructions)
            inst_type = 'L';
            // cout << "L type detected \n";
            l_type_ID_EX_reg.imm    = signExtend32(binToUint(if_id.out.substr(0, 12)), 12);
            l_type_ID_EX_reg.rs1    = binToUint(if_id.out.substr(12, 5));
            l_type_ID_EX_reg.funct3 = binToUint(if_id.out.substr(17, 3));
            l_type_ID_EX_reg.rd     = binToUint(if_id.out.substr(20, 5));
            l_type_ID_EX_reg.opcode = binToUint(opcode);
    
            // Identify the load instruction
            string &op = l_type_ID_EX_reg.op;
            uint32_t f3 = l_type_ID_EX_reg.funct3;
            if (f3 == 0) op = "lb";
            if (f3 == 1) op = "lh";
            if (f3 == 2) op = "lw";
            if (f3 == 4) op = "lbu";
            if (f3 == 5) op = "lhu";
    
            // Load instructions read rs1 and write to rd
            currentRs1 = l_type_ID_EX_reg.rs1;
            currentRd  = l_type_ID_EX_reg.rd;
            currentRegWrite = true;  // Load writes to a register
        }
        else if (opcode == "0010011"  ||
                 opcode == "1100111" || opcode == "1110011") {
            // I-type
            inst_type = 'I';
            uint32_t rawImm = binToUint(if_id.out.substr(0, 12));
            i_type_ID_EX_reg.imm    = signExtend32(rawImm, 12);
            i_type_ID_EX_reg.rs1    = binToUint(if_id.out.substr(12, 5));
            i_type_ID_EX_reg.funct3 = binToUint(if_id.out.substr(17, 3));
            i_type_ID_EX_reg.rd     = binToUint(if_id.out.substr(20, 5));
            i_type_ID_EX_reg.opcode = binToUint(opcode);

            // Identify the operation
            string &op = i_type_ID_EX_reg.op;
            uint32_t f3 = i_type_ID_EX_reg.funct3;
            int32_t imm = i_type_ID_EX_reg.imm;
            if(f3 == 0) op = "addi";
            if(f3 == 1) op = "slli";
            if(f3 == 2) op = "slti";
            if(f3 == 3) op = "sltiu";
            if(f3 == 4) op = "xori";
            if(f3 == 5 && ((imm >> 10) == 0))  op = "srli";
            if(f3 == 5 && ((imm >> 10) == 32)) op = "srai";
            if(f3 == 6) op = "ori";
            if(f3 == 7) op = "andi";

            currentRs1 = i_type_ID_EX_reg.rs1;
            // for typical I-type, only rs1 is read
            currentRd  = i_type_ID_EX_reg.rd;
            currentRegWrite = true;  // I-type usually writes a register (e.g. addi)
        }
        else if (opcode == "0100011") {
            // S-type
            inst_type = 'S';
            uint32_t imm_part1 = binToUint(if_id.out.substr(0, 7));
            uint32_t imm_part2 = binToUint(if_id.out.substr(20, 5));
            uint32_t rawImm = (imm_part1 << 5) | imm_part2;
            stype.imm    = signExtend32(rawImm, 12);
            stype.rs2    = binToUint(if_id.out.substr(7, 5));
            stype.rs1    = binToUint(if_id.out.substr(12, 5));
            stype.funct3 = binToUint(if_id.out.substr(17, 3));
            stype.opcode = binToUint(opcode);

            string &op = stype.op;
            if(stype.funct3 == 0) op = "sb";
            if(stype.funct3 == 1) op = "sh";
            if(stype.funct3 == 2) op = "sw";

            currentRs1 = stype.rs1; // base register
            currentRs2 = stype.rs2; // data to store
            currentRegWrite = false; // store does not write a register
        }
        else if (opcode == "0110111" || opcode == "0010111") {
            // U-type
            inst_type = 'U';
            u_type_ID_EX_reg.imm = binToUint(if_id.out.substr(0, 20));
            u_type_ID_EX_reg.rd  = binToUint(if_id.out.substr(20, 5));
            u_type_ID_EX_reg.opcode = binToUint(opcode);

            // LUI/AUIPC write to rd
            currentRd  = u_type_ID_EX_reg.rd;
            currentRegWrite = true;
        }
        else if (opcode == "1100011") {
            // B-type
            inst_type = 'B';
            uint32_t imm12   = binToUint(if_id.out.substr(0, 1));
            uint32_t imm10_5 = binToUint(if_id.out.substr(1, 6));
            uint32_t imm4_1  = binToUint(if_id.out.substr(20, 4));
            uint32_t imm11   = binToUint(if_id.out.substr(24, 1));
            uint32_t rawImm  = (imm12 << 12) | (imm11 << 11) 
                              | (imm10_5 << 5) | (imm4_1 << 1);
            btype.imm    = signExtend32(rawImm, 13);
            btype.rs2    = binToUint(if_id.out.substr(7, 5));
            btype.rs1    = binToUint(if_id.out.substr(12, 5));
            btype.funct3 = binToUint(if_id.out.substr(17, 3));
            btype.opcode = binToUint(opcode);

            // Branches read rs1, rs2, do not write a register
            currentRs1 = btype.rs1;
            currentRs2 = btype.rs2;
            currentRegWrite = false;
        }
        else if (opcode == "1101111") {
            // J-type
            inst_type = 'J';
            uint32_t imm20   = binToUint(if_id.out.substr(0, 1));
            uint32_t imm10_1 = binToUint(if_id.out.substr(1, 10));
            uint32_t imm11   = binToUint(if_id.out.substr(11, 1));
            uint32_t imm19_12= binToUint(if_id.out.substr(12, 8));
            uint32_t rawImm  = (imm20 << 20) | (imm19_12 << 12)
                              | (imm11 << 11) | (imm10_1 << 1);
            jtype.imm = signExtend32(rawImm, 21);
            jtype.rd  = binToUint(if_id.out.substr(20, 5));
            jtype.opcode = binToUint(opcode);

            currentRd  = jtype.rd;
            currentRegWrite = true;  // JAL writes return addr to rd
        }

        else {
            // Unknown instruction
            inst_type = 'X';
        }
    }

    // Execute Stage
    void EX() {
        uint32_t aluResult = 0;
        uint32_t storeData = 0;
        bool memRead  = false;
        bool memWrite = false;
        bool regWrite = false;

        ex_mem.inst_type = inst_type;
        
        switch(inst_type) {
            case 'R': {
                int rs1_val = reg_set[r_type_ID_EX_reg.rs1];
                int rs2_val = reg_set[r_type_ID_EX_reg.rs2];

                // string op   = r_type_ID_EX_reg.op;
                // if(op == "add")       aluResult = rs1_val + rs2_val;
                // else if(op == "sub")  aluResult = rs1_val - rs2_val;
                // else if(op == "sll")  aluResult = rs1_val << (rs2_val & 0x1F);
                // else if(op == "slt")  aluResult = (rs1_val < rs2_val) ? 1 : 0;
                // else if(op == "sltu") aluResult = ((unsigned)rs1_val < (unsigned)rs2_val) ? 1 : 0;
                // else if(op == "xor")  aluResult = rs1_val ^ rs2_val;
                // else if(op == "srl")  aluResult = ((unsigned)rs1_val) >> (rs2_val & 0x1F);
                // else if(op == "sra")  aluResult = rs1_val >> (rs2_val & 0x1F);
                // else if(op == "or")   aluResult = rs1_val | rs2_val;
                // else if(op == "and")  aluResult = rs1_val & rs2_val;
                // else if(op == "mul")  aluResult = rs1_val * rs2_val;
                // // etc.
                int t_m = max(rs1_val,rs2_val);
                regWrite = true;
                ex_mem.rd = r_type_ID_EX_reg.rd;
                ex_mem.op = r_type_ID_EX_reg.op;
                reg_set[ex_mem.rd] = spaces;
                if (spaces <= t_m) {stall = true; return ;}
                else {stall = false;return;}

                // break;
            }
            case 'I': {
                int t_m = reg_set[i_type_ID_EX_reg.rs1];
                int imm_val = i_type_ID_EX_reg.imm;
                string op   = i_type_ID_EX_reg.op;
                // if(op == "addi")  aluResult = rs1_val + imm_val;
                // else if(op == "slli") aluResult = rs1_val << (imm_val & 0x1F);
                // else if(op == "slti") aluResult = (rs1_val < imm_val) ? 1 : 0;
                // else if(op == "sltiu")aluResult = ((unsigned)rs1_val < (unsigned)imm_val) ? 1 : 0;
                // else if(op == "xori") aluResult = rs1_val ^ imm_val;
                // else if(op == "srli") aluResult = ((unsigned)rs1_val) >> (imm_val & 0x1F);
                // else if(op == "srai") aluResult = rs1_val >> (imm_val & 0x1F);
                // else if(op == "ori")  aluResult = rs1_val | imm_val;
                // else if(op == "andi") aluResult = rs1_val & imm_val;
                
                regWrite = true;
                ex_mem.rd = i_type_ID_EX_reg.rd;
                ex_mem.op = i_type_ID_EX_reg.op;
                reg_set[ex_mem.rd] = spaces;

                if (spaces > t_m){stall = false;return;}

                else {stall = true;return ;}
                // break;
            }
            case 'L': {
                // cout << "L type EXE detected\n";
                /*  inst_type = 'L';
            l_type_ID_EX_reg.imm    = signExtend32(binToUint(if_id.out.substr(0, 12)), 12);
            l_type_ID_EX_reg.rs1    = binToUint(if_id.out.substr(12, 5));
            l_type_ID_EX_reg.funct3 = binToUint(if_id.out.substr(17, 3));
            l_type_ID_EX_reg.rd     = binToUint(if_id.out.substr(20, 5));
            l_type_ID_EX_reg.opcode = binToUint(opcode); 
    */
                int t_m = reg_set[l_type_ID_EX_reg.rs1];
                if(t_m > spaces){stall = true;return;}

                else {stall = false;return ;}
            }
            case 'S': {
                int rs1_val = reg_set[stype.rs1];
                int rs2_val = reg_set[stype.rs2];
                // aluResult = rs1_val + stype.imm; // effective address
                // storeData = rs2_val;
                memWrite  = true;
                regWrite  = false; // store does not write a register
                
                ex_mem.op = stype.op;
                int t_m = max(rs1_val, rs2_val);
                if(t_m > spaces){stall = true;return;}
                else {stall = false;return ;}
                // break;
            }
            case 'U': {
                // LUI or AUIPC
                aluResult = (u_type_ID_EX_reg.imm << 12);
                regWrite = true;
                stall = false;
                ex_mem.rd = u_type_ID_EX_reg.rd;
                break;
            }
            case 'B': {
                // Very naive: only handles BEQ as example
                // (You can extend for other branches)
                int rs1_val = reg_set[btype.rs1];
                int rs2_val = reg_set[btype.rs2];
                if(btype.funct3 == 0) { // BEQ
                    // if(rs1_val == rs2_val) { ... }
                    // etc.
                }
                // Branch does not write registers
                int t = max(rs1_val,rs2_val);
                if (spaces > t){stall = false;return ;}
                else {stall = true; return ;}
                regWrite = false;
                break;
            }
            case 'J': {
                // JAL: store PC+4 in rd, compute new PC in real pipeline
                // Here, we just do a placeholder
                aluResult = jtype.imm; 
                regWrite  = true;
                ex_mem.rd = jtype.rd;
                break;
            }
            default:
                break;
        }

        ex_mem.aluResult = aluResult;
        ex_mem.storeData = storeData;
        ex_mem.memRead   = false;   // (set true if you implement loads)
        ex_mem.memWrite  = memWrite;
        ex_mem.regWrite  = regWrite;
    }

    // Memory Stage
    void MEM() {
        if (ex_mem.memWrite) {
            if (ex_mem.aluResult < dmem.size()) {
                dmem[ex_mem.aluResult] = ex_mem.storeData;
            } else {
                cerr << "ERROR: Memory write out of bounds\n";
            }
        }
        // If load, ex_mem.memRead == true => mem_wb.memData = ...
        // but not implemented here.
        if(inst_type == 'L')                reg_set[l_type_ID_EX_reg.rd] = spaces;

        mem_wb.aluResult = ex_mem.aluResult;
        mem_wb.rd        = ex_mem.rd;
        mem_wb.regWrite  = ex_mem.regWrite;
        // If we had a load, mem_wb.memData = ...
        mem_wb.memData   = 0; 
        mem_wb.memToReg  = ex_mem.memRead;
    }

    // Write Back Stage
    void WB() {
        stall = false;
        // if(inst_type == 'R' || 'I' || 'L'){
        //     reg_set[mem_wb.rd] = spaces;
        // }
        // if(mem_wb.regWrite) {
        //     // If memToReg is true => write mem_wb.memData
        //     // else => write mem_wb.aluResult
        //     uint32_t writeData = mem_wb.memToReg ? mem_wb.memData : mem_wb.aluResult;
        //     // Don't ever write to x0
        //     if (mem_wb.rd != 0) {
        //         reg_set[mem_wb.rd] = writeData;
        //     }
        // }
    }

    // Debug method to print register contents (if desired)

};

//
// Simple hazard detection with the *previous* instruction only.
//
// bool detectHazard(uint32_t curRs1, uint32_t curRs2,
//                   uint32_t prevRd,  bool prevRegWrite)
// {
//     if (!prevRegWrite) return false;  // previous doesn't write => no hazard
//     if (prevRd == 0)   return false;  // writing to x0 => no hazard

//     // If the current instruction reads rs1 or rs2 = previous's rd => hazard
//     if (curRs1 == prevRd || curRs2 == prevRd)
//         return true;

//     return false;
// }

int main(int argc, char* argv[]) {

        if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <input_directory> <cyclecount>\n";
        return 1;
    }
    
         string inputFile = argv[1]; // Directory containing input files
    int cycleCount = stoi(argv[2]); // Convert cyclecount argument to an integer
    
    // Extract filename without extension
    string filename = fs::path(inputFile).stem().string(); 

    // Ensure output directory exists
    string outputDir = "outputfiles/";
    fs::create_directories(outputDir); 

    // Open output files
    string forward_out = outputDir + filename + "_forward_out.txt";
    ofstream forwardFile(forward_out);

    if (!forwardFile) {
        cerr << "ERROR: Could not create output files\n";
        return 1;
    }
    
    ifstream file(inputFile);
    
    if (!file) {
        cerr << "ERROR: Could not open input file\n";
        return 1;
    }

    vector<string> lines;       // To store hex instructions
    vector<string> risc_inst;   // To store RISC-V instructions
    string line, hex_part, inst_part;

    // Read the file and split lines into two columns
    while (getline(file, line)) {
        istringstream iss(line);
        if (!(iss >> hex_part)) continue;  // Read hex part
        iss >> ws; 
        getline(iss, inst_part);           // Read the rest as instruction
        lines.push_back(hex_part);
        risc_inst.push_back(inst_part);
    }

    file.close();

	// Convert hex to 32-bit binary
	vector<string> binary_mc(lines.size());
	for (size_t i = 0; i < lines.size(); ++i) {
		ostringstream oss;
		for (char c : lines[i]) {
			if (!isspace((unsigned char)c)) {
				oss << hex_to_bin(c);
			}
		}
		binary_mc[i] = oss.str();
	}

	Processor processor;

	// Track previous instruction's destination register + regWrite
	uint32_t prevRd = 0;
	bool     prevRegWrite = false;

	// Simulate each instruction in a naive pipeline manner
	vector<vector<int>>ans(risc_inst.size(),vector<int>(cycleCount,0)); // 0 -> " ", 1 -> "IF", 2 -> ID, 3 -> EX , 4-> MEM , 5-> WB
	
	int next_sp = 0;

	for (size_t pc = 1; pc <= binary_mc.size(); ++pc) {
		spaces = next_sp;

		// 1) Instruction Fetch (IF)
		ans[pc - 1][spaces] = 1;
		spaces++;

		Processor::IF_ID_reg if_id_reg;
		processor.IF(if_id_reg, pc, binary_mc);

		// 2) Instruction Decode (ID)
		stall = false;
		while(pc >= 2 && ans[pc-2][spaces] == 6){ans[pc-1][spaces] = 6;spaces++;}
		processor.ID(if_id_reg);
		while(stall){
			ans[pc-1][spaces] = 6;
			spaces++;
			processor.ID(if_id_reg);
		}
		next_sp = spaces;
		ans[pc - 1][spaces] = 2;
		spaces++;

		// 4) Execute (EX) - Ensure it always runs!
		stall = false;
		processor.EX();
		while(stall){
			ans[pc-1][spaces] = 6;
			spaces++;
			processor.EX();
		}
		ans[pc - 1][spaces] = 3;
		spaces++;

		// 5) Memory Access (MEM) - Ensure it always runs!
		processor.MEM();
		ans[pc - 1][spaces] = 4;
		spaces++;

		// 6) Write Back (WB) - Ensure it always runs!
		processor.WB();
		ans[pc - 1][spaces] = 5;

		// Update previous instruction info
		// prevRd = processor.ex_mem.rd;
		// prevRegWrite = processor.ex_mem.regWrite;
	}


        ostream& out = forwardFile; 
	// Print column headers

	forwardFile << setw(15) << "Instruction"; 

	for (size_t i = 0; i <= cycleCount; i++)
		forwardFile << setw(6) << i;
	forwardFile << '\n';

	// Print pipeline stages
	for (size_t i = 0; i < ans.size(); i++) {
		forwardFile << setw(15) << risc_inst[i] << "|";
		for (size_t j = 0; j < cycleCount; j++) {
			if (ans[i][j] == 1) forwardFile << setw(6) << "IF";
			else if (ans[i][j] == 2) forwardFile << setw(6) << "ID";
			else if (ans[i][j] == 3) forwardFile << setw(6) << "EX";
			else if (ans[i][j] == 4) forwardFile << setw(6) << "MEM";
			else if (ans[i][j] == 5) forwardFile << setw(6) << "WB";
			else if (ans[i][j] == 6) forwardFile << setw(6) << "-;";
			else forwardFile << setw(6) << ";";
		}
		forwardFile << '\n';
	}
	// Close output files
    forwardFile.close();
	return 0;
}
