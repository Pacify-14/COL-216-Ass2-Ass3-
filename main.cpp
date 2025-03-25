#include <bits/stdc++.h>
#include <fstream>

using namespace std;

// Utility functions
uint32_t binToUint(const std::string &binStr) {
    uint32_t value = 0;
    for (char c : binStr) {
        value <<= 1;
        if (c == '1')
            value |= 1;
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
    string lookup[] = {
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
    vector<int32_t> reg_set; // set of 32 registers x0 - x31
    
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
        uint32_t imm;
        uint32_t rs1;
        uint32_t funct3;
        uint32_t rd;
        uint32_t opcode;
        string op;
    };
    
    struct S_type_ID_EX_reg {
        uint32_t imm;
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
        uint32_t imm;
        uint32_t rs2;
        uint32_t rs1;
        uint32_t funct3;
        uint32_t opcode;
        string op;
    };
    
    struct J_type_ID_EX_reg {
        uint32_t imm;
        uint32_t rd;
        uint32_t opcode;
    };

    // Pipeline Stage Registers
    IF_ID_reg if_id;
    R_type_ID_EX_reg r_type_ID_EX_reg;
    I_type_ID_EX_reg i_type_ID_EX_reg;
    U_type_ID_EX_reg u_type_ID_EX_reg;
    S_type_ID_EX_reg stype;
    B_type_ID_EX_reg btype;
    J_type_ID_EX_reg jtype;
    
    char inst_type;

public:
    // Constructor to initialize register set
    Processor() : reg_set(32, 0) {
        cout << "Processor Initialized: 32 Registers Set to 0" << endl;
    }

    // Instruction Fetch Stage
    void IF(IF_ID_reg &if_id, int cur_pc, vector<string>& bin_inst) {
        cout << "\n===== Instruction Fetch (IF) Stage =====" << endl;
        cout << "Current Program Counter: " << cur_pc << endl;
        
        if (cur_pc > 0 && cur_pc <= bin_inst.size()) {
            if_id.out = bin_inst[cur_pc - 1];
            if_id.cur_pc = cur_pc;
            
            cout << "Fetched Instruction (32-bit): " << if_id.out << endl;
        } else {
            cerr << "ERROR: Invalid Program Counter or Instruction Memory Access" << endl;
        }
    }

    // Instruction Decode Stage
    void ID(IF_ID_reg &if_id) {
        cout << "\n===== Instruction Decode (ID) Stage =====" << endl;
        
        string opcode = if_id.out.substr(25, 7);
        cout << "Full Instruction: " << if_id.out << endl;
        cout << "Opcode: " << opcode << endl;

        // Comprehensive instruction type decoding
        if (opcode == "0110011") {
            // R-type Instructions (Arithmetic and Logical)
            inst_type = 'R';
            r_type_ID_EX_reg.funct7 = binToUint(if_id.out.substr(0, 7));
            r_type_ID_EX_reg.rs2 = binToUint(if_id.out.substr(7, 5));
            r_type_ID_EX_reg.rs1 = binToUint(if_id.out.substr(12, 5));
            r_type_ID_EX_reg.funct3 = binToUint(if_id.out.substr(17, 3));
            r_type_ID_EX_reg.rd = binToUint(if_id.out.substr(20, 5));
            r_type_ID_EX_reg.opcode = binToUint(opcode);
            string* op = &r_type_ID_EX_reg.op;
            if(r_type_ID_EX_reg.funct7 == 32 && r_type_ID_EX_reg.funct3 == 0)*op = "sub";
            if(r_type_ID_EX_reg.funct7 == 0 && r_type_ID_EX_reg.funct3 == 0)*op = "add";
            if(r_type_ID_EX_reg.funct7 == 0 && r_type_ID_EX_reg.funct3 == 1)*op = "sll";
            if(r_type_ID_EX_reg.funct7 == 0 && r_type_ID_EX_reg.funct3 == 2)*op = "slt";
            if(r_type_ID_EX_reg.funct7 == 0 && r_type_ID_EX_reg.funct3 == 3)*op = "sltu";
            if(r_type_ID_EX_reg.funct7 == 0 && r_type_ID_EX_reg.funct3 == 4)*op = "xor";
            if(r_type_ID_EX_reg.funct7 == 0 && r_type_ID_EX_reg.funct3 == 5)*op = "srl";
            if(r_type_ID_EX_reg.funct7 == 32 && r_type_ID_EX_reg.funct3 == 5)*op = "sra";
            if(r_type_ID_EX_reg.funct7 == 0 && r_type_ID_EX_reg.funct3 == 6)*op = "or";
            if(r_type_ID_EX_reg.funct7 == 0 && r_type_ID_EX_reg.funct3 == 7)*op = "and";
            if(r_type_ID_EX_reg.funct7 == 1 && r_type_ID_EX_reg.funct3 == 0)*op = "mul";
            if(r_type_ID_EX_reg.funct7 == 1 && r_type_ID_EX_reg.funct3 == 1)*op = "mulh";
            if(r_type_ID_EX_reg.funct7 == 1 && r_type_ID_EX_reg.funct3 == 2)*op = "mulhsu";
            if(r_type_ID_EX_reg.funct7 == 1 && r_type_ID_EX_reg.funct3 == 3)*op = "mulhu";
            if(r_type_ID_EX_reg.funct7 == 1 && r_type_ID_EX_reg.funct3 == 4)*op = "div";
            if(r_type_ID_EX_reg.funct7 == 1 && r_type_ID_EX_reg.funct3 == 5)*op = "divu";
            if(r_type_ID_EX_reg.funct7 == 1 && r_type_ID_EX_reg.funct3 == 6)*op = "rem";
            if(r_type_ID_EX_reg.funct7 == 1 && r_type_ID_EX_reg.funct3 == 7)*op = "remu";

            cout << "R-type Instruction Detected" << endl;
            cout << "Operation Details:" << endl;
            cout << "  funct7: " << r_type_ID_EX_reg.funct7 << endl;
            cout << "  rs2 (Source Register 2): x" << r_type_ID_EX_reg.rs2 << endl;
            cout << "  rs1 (Source Register 1): x" << r_type_ID_EX_reg.rs1 << endl;
            cout << "  funct3: " << r_type_ID_EX_reg.funct3 << endl;
            cout << "  rd (Destination Register): x" << r_type_ID_EX_reg.rd << endl;
            cout << " op type: " << r_type_ID_EX_reg.op <<'\n';
        }
        else if (opcode == "0010011" || opcode == "0000011" || 
                 opcode == "1100111" || opcode == "1110011") {
            // I-type Instructions (Immediate, Load, JALR, System)
            inst_type = 'I';
            uint32_t rawImm = binToUint(if_id.out.substr(0, 12));
            i_type_ID_EX_reg.imm = signExtend32(rawImm, 12);
            i_type_ID_EX_reg.rs1 = binToUint(if_id.out.substr(12, 5));
            i_type_ID_EX_reg.funct3 = binToUint(if_id.out.substr(17, 3));
            i_type_ID_EX_reg.rd = binToUint(if_id.out.substr(20, 5));
            i_type_ID_EX_reg.opcode = binToUint(opcode);
            string* op = &i_type_ID_EX_reg.op;

            if(i_type_ID_EX_reg.funct3 == 0)*op = "addi";
            if(i_type_ID_EX_reg.funct3 == 1)*op = "slli";
            if(i_type_ID_EX_reg.funct3 == 2)*op = "slti";
            if(i_type_ID_EX_reg.funct3 == 3)*op = "sltiu";
            if(i_type_ID_EX_reg.funct3 == 4)*op = "xori";
            if(i_type_ID_EX_reg.funct3 == 5 && (i_type_ID_EX_reg.imm >> 10) == 0)*op = "srli";
            if(i_type_ID_EX_reg.funct3 == 5 && (i_type_ID_EX_reg.imm >> 10) == 32)*op = "srai";
            if(i_type_ID_EX_reg.funct3 == 6)*op = "ori";
            if(i_type_ID_EX_reg.funct3 == 7)*op = "andi";


            cout << "I-type Instruction Detected" << endl;
            cout << "Operation Details:" << endl;
            cout << "  Immediate Value: " << i_type_ID_EX_reg.imm << endl;
            cout << "  rs1 (Source Register): x" << i_type_ID_EX_reg.rs1 << endl;
            cout << "  funct3: " << i_type_ID_EX_reg.funct3 << endl;
            cout << "  rd (Destination Register): x" << i_type_ID_EX_reg.rd << endl;
            cout << " op type: " << i_type_ID_EX_reg.op << '\n';
        }
        else if (opcode == "0100011") {
            // S-type Instructions (Store)
            inst_type = 'S';
            uint32_t imm_part1 = binToUint(if_id.out.substr(0, 7));
            uint32_t imm_part2 = binToUint(if_id.out.substr(20, 5));
            uint32_t rawImm = (imm_part1 << 5) | imm_part2;
            stype.imm = signExtend32(rawImm, 12);
            stype.rs2 = binToUint(if_id.out.substr(7, 5));
            stype.rs1 = binToUint(if_id.out.substr(12, 5));
            stype.funct3 = binToUint(if_id.out.substr(17, 3));
            stype.opcode = binToUint(opcode);

            string* op = &stype.op;
            if(stype.funct3 == 0)*op = "sb";
            if(stype.funct3 == 1)*op = "sh";
            if(stype.funct3 == 2)*op = "sw";


            cout << "S-type Instruction Detected" << endl;
            cout << "Operation Details:" << endl;
            cout << "  Immediate Value: " << stype.imm << endl;
            cout << "  rs2 (Source Register 2): x" << stype.rs2 << endl;
            cout << "  rs1 (Base Register): x" << stype.rs1 << endl;
            cout << "  funct3: " << stype.funct3 << endl;
            cout << " op code: " << stype.op << '\n';
        }
        else if (opcode == "0110111" || opcode == "0010111") {
            // U-type Instructions (LUI, AUIPC)
            inst_type = 'U';
            u_type_ID_EX_reg.imm = binToUint(if_id.out.substr(0, 20));
            u_type_ID_EX_reg.rd = binToUint(if_id.out.substr(20, 5));
            u_type_ID_EX_reg.opcode = binToUint(opcode);

            cout << "U-type Instruction Detected" << endl;
            cout << "Operation Details:" << endl;
            cout << "  Immediate Value: " << u_type_ID_EX_reg.imm << endl;
            cout << "  rd (Destination Register): x" << u_type_ID_EX_reg.rd << endl;
        }
        else if (opcode == "1100011") {
            // B-type Instructions (Branches)
            inst_type = 'B';
            uint32_t imm_part1 = binToUint(if_id.out.substr(0, 7));
            uint32_t imm_part2 = binToUint(if_id.out.substr(20, 5));
            uint32_t imm12 = (imm_part1 >> 6) & 0x1;
            uint32_t imm10_5 = imm_part1 & 0x3F;
            uint32_t imm11 = (imm_part2 >> 4) & 0x1;
            uint32_t imm4_1 = imm_part2 & 0xF;
            uint32_t rawImm = (imm12 << 12) | (imm11 << 11) | (imm10_5 << 5) | (imm4_1 << 1);
            btype.imm = signExtend32(rawImm, 13);
            btype.rs2 = binToUint(if_id.out.substr(7, 5));
            btype.rs1 = binToUint(if_id.out.substr(12, 5));
            btype.funct3 = binToUint(if_id.out.substr(17, 3));
            btype.opcode = binToUint(opcode);

            cout << "B-type Instruction Detected" << endl;
            cout << "Operation Details:" << endl;
            cout << "  Immediate Value: " << btype.imm << endl;
            cout << "  rs2 (Compare Register): x" << btype.rs2 << endl;
            cout << "  rs1 (Compare Register): x" << btype.rs1 << endl;
            cout << "  funct3: " << btype.funct3 << endl;
        }
        else if (opcode == "1101111") {
            // J-type Instructions (JAL)
            inst_type = 'J';
            uint32_t imm20 = binToUint(if_id.out.substr(0, 1));
            uint32_t imm10_1 = binToUint(if_id.out.substr(1, 10));
            uint32_t imm11 = binToUint(if_id.out.substr(11, 1));
            uint32_t imm19_12 = binToUint(if_id.out.substr(12, 8));
            uint32_t rawImm = (imm20 << 20) | (imm19_12 << 12) | (imm11 << 11) | (imm10_1 << 1);
            jtype.imm = signExtend32(rawImm, 21);
            jtype.rd = binToUint(if_id.out.substr(20, 5));
            jtype.opcode = binToUint(opcode);

            cout << "J-type Instruction Detected" << endl;
            cout << "Operation Details:" << endl;
            cout << "  Immediate Value: " << jtype.imm << endl;
            cout << "  rd (Destination Register): x" << jtype.rd << endl;
        }
        else {
            cout << "ERROR: Unknown Instruction Type" << endl;
            inst_type = 'X';
        }
    }

    // Execute Stage
    void EX() {
        cout << "\n===== Execute (EX) Stage =====" << endl;
        
        switch(inst_type) {
            case 'R': {
                cout << "R-type Instruction Execution" << endl;
                // Placeholder for R-type instruction execution logic
                break;
            }
            case 'I': {
                cout << "I-type Instruction Execution" << endl;
                // Placeholder for I-type instruction execution logic
                break;
            }
            case 'S': {
                cout << "S-type Instruction Execution" << endl;
                // Placeholder for S-type instruction execution logic
                break;
            }
            case 'U': {
                cout << "U-type Instruction Execution" << endl;
                // Placeholder for U-type instruction execution logic
                break;
            }
            case 'B': {
                cout << "B-type Instruction Execution" << endl;
                // Placeholder for B-type instruction execution logic
                break;
            }
            case 'J': {
                cout << "J-type Instruction Execution" << endl;
                // Placeholder for J-type instruction execution logic
                break;
            }
            default:
                cout << "ERROR: Invalid Instruction Type in Execution Stage" << endl;
        }
    }

    // Debug method to print register contents
    void printRegisters() {
        cout << "\n===== Register Contents =====" << endl;
        for (int i = 0; i < 32; ++i) {
            cout << "x" << i << ": " << reg_set[i] << endl;
        }
    }
};

int main() {
    // Input file handling
    ifstream file("ass_inst.txt");
    vector<string> lines;
    string line;
    
    if (!file) {
        cerr << "ERROR: No input file found" << endl;
        return 1;
    }

    // Read input file
    while (getline(file, line)) {
        lines.push_back(line);
    }     
    file.close();

    // Hex to binary conversion
    vector<string> binary_mc(lines.size());
    for (size_t i = 0; i < lines.size(); ++i) {
        ostringstream oss;
        for (char c : lines[i]) {
            if (c != ' ' && c != '\n' && c != '\t' && c != '\0')
                oss << hex_to_bin(c);
        }
        binary_mc[i] = oss.str();
        
        // Print each converted binary instruction
        cout << "Binary Machine Code " << i + 1 << ": " << binary_mc[i] << endl;
    }

    // Processor simulation
    Processor processor;
    
    // Simulate pipeline stages for each instruction
    for (size_t pc = 1; pc <= binary_mc.size(); ++pc) {
        cout << "\n########## Processing Instruction " << pc << " ##########" << endl;
        
        Processor::IF_ID_reg if_id_reg;
        processor.IF(if_id_reg, pc, binary_mc);
        processor.ID(if_id_reg);
        processor.EX();
        
        // processor.printRegisters();
    }

    return 0;
}