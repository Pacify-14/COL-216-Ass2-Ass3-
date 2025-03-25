#include <bits/stdc++.h>
#include <fstream>
using namespace std;

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

    if (c >= '0' && c <= '9')
        return lookup[c - '0'];
    if (c >= 'a' && c <= 'f')
        return lookup[c - 'a' + 10];

    return "Invalid";
}

class Processor {
    int pc;  // program counter
    int32_t reg_set[32];  // register file: 32 registers (x0 - x31)
    
    // Pipeline registers
    struct IF_ID_reg {
        int cur_pc;
        string out;
    } if_id;
    
    struct ID_EX_reg {
        uint32_t opcode, funct3, funct7;
        uint32_t rs1, rs2, rd;
        int32_t imm;
        char inst_type;
    } id_ex;
    
    struct EX_MEM_reg {
        int32_t alu_result;
        uint32_t rd;
        int32_t rs2_value;
        uint32_t opcode, funct3;
        bool memRead, memWrite, regWrite;
    } ex_mem;
    
    struct MEM_WB_reg {
        int32_t result;
        uint32_t rd;
        bool regWrite;
    } mem_wb;
    
public:
    Processor() {
        pc = 0;
        for (int i = 0; i < 32; i++)
            reg_set[i] = 0;
    }
    
    void IF(vector<string>& bin_inst) {
        if (pc >= bin_inst.size()) {
            cerr << "IF ERROR: PC out of range! PC = " << pc << endl;
            return;
        }
        if_id.out = bin_inst[pc];
        if_id.cur_pc = pc;
        cout << "Fetched Instruction: " << if_id.out << endl;
    }
    
    void ID() {
        if (if_id.out.length() < 32) {
            cerr << "ERROR: Instruction too short!\n";
            return;
        }
    
        string opcodeStr = if_id.out.substr(if_id.out.size() - 7, 7);
        uint32_t opcode = binToUint(opcodeStr);
        id_ex.opcode = opcode;
    
        cout << "Decoding: " << if_id.out << endl;
        cout << "Opcode: " << opcode << endl;
    
        if (opcode == 0b0110011) { // R-type
            id_ex.inst_type = 'R';
            id_ex.rs1    = binToUint(if_id.out.substr(12, 5));
            id_ex.rs2    = binToUint(if_id.out.substr(7, 5));
            id_ex.rd     = binToUint(if_id.out.substr(20, 5));
            id_ex.funct3 = binToUint(if_id.out.substr(17, 3));
            id_ex.funct7 = binToUint(if_id.out.substr(0, 7));
            id_ex.imm = 0; // R-type has no immediate
        } else if (opcode == 0b0000011 || opcode == 0b0010011) { // I-type
            id_ex.inst_type = 'I';
            id_ex.rs1    = binToUint(if_id.out.substr(12, 5));
            id_ex.rd     = binToUint(if_id.out.substr(20, 5));
            id_ex.funct3 = binToUint(if_id.out.substr(17, 3));
            id_ex.imm     = signExtend32(binToUint(if_id.out.substr(0, 12)), 12);
        } else if (opcode == 0b0100011) { // S-type
            id_ex.inst_type = 'S';
            id_ex.rs1    = binToUint(if_id.out.substr(12, 5));
            id_ex.rs2    = binToUint(if_id.out.substr(7, 5));
            id_ex.imm     = signExtend32(binToUint(if_id.out.substr(0, 7) + if_id.out.substr(25, 5)), 12);
            id_ex.rd = 0; // S-type has no rd
        } else if (opcode == 0b1100011) { // B-type
            id_ex.inst_type = 'B';
            id_ex.rs1    = binToUint(if_id.out.substr(12, 5));
            id_ex.rs2    = binToUint(if_id.out.substr(7, 5));
            id_ex.imm     = signExtend32(binToUint(if_id.out.substr(0, 1) + if_id.out.substr(24, 6) + if_id.out.substr(1, 6)), 13);
            id_ex.rd = 0; // B-type has no rd
        } else {
            cerr << "Unknown instruction type!\n";
            return;
        }
    
        cout << "ID: Opcode = " << id_ex.opcode << ", rs1 = " << id_ex.rs1 
             << ", rs2 = " << id_ex.rs2 << ", rd = " << id_ex.rd 
             << ", imm = " << id_ex.imm << endl;
    }
    
    void EX() {
        int32_t rs1_val = reg_set[id_ex.rs1];
        int32_t rs2_val = (id_ex.inst_type == 'R' || id_ex.inst_type == 'I') ? reg_set[id_ex.rs2] : 0;
        ex_mem.opcode = id_ex.opcode;
        ex_mem.funct3 = id_ex.funct3;
        ex_mem.rd = id_ex.rd;
        ex_mem.rs2_value = rs2_val;
        ex_mem.regWrite = false;
        ex_mem.memRead = false;
        ex_mem.memWrite = false;

        if (id_ex.inst_type == 'R') {
            switch (id_ex.funct3) {
                case 0b000: // ADD / SUB
                    if (id_ex.funct7 == 0b0000000)
                        ex_mem.alu_result = rs1_val + rs2_val; // ADD
                    else if (id_ex.funct7 == 0b0100000)
                        ex_mem.alu_result = rs1_val - rs2_val; // SUB
                    ex_mem.regWrite = true;
                    break;
                case 0b111: // AND
                    ex_mem.alu_result = rs1_val & rs2_val;
                    ex_mem.regWrite = true;
                    break;
                case 0b110: // OR
                    ex_mem.alu_result = rs1_val | rs2_val;
                    ex_mem.regWrite = true;
                    break;
                case 0b100: // XOR
                    ex_mem.alu_result = rs1_val ^ rs2_val;
                    ex_mem.regWrite = true;
                    break;
                case 0b010: // SLT
                    ex_mem.alu_result = (rs1_val < rs2_val) ? 1 : 0;
                    ex_mem.regWrite = true;
                    break;
                default:
                    cout << "Unknown R-type instruction\n";
                    break;
            }
        } 
        else if (id_ex.inst_type == 'I') {
            switch (id_ex.opcode) {
                case 0b0010011: // ADDI
                    ex_mem.alu_result = rs1_val + id_ex.imm;
                    ex_mem.regWrite = true;
                    break;
                case 0b0000011: // LOAD
                    ex_mem.alu_result = rs1_val + id_ex.imm;
                    ex_mem.memRead = true;
                    ex_mem.regWrite = true;
                    break;
                default:
                    cout << "Unknown I-type instruction\n";
                    break;
            }
        } 
        else if (id_ex.inst_type == 'S') {
            ex_mem.alu_result = rs1_val + id_ex.imm;
            ex_mem.memWrite = true;
        } 
        else if (id_ex.inst_type == 'B') {
            bool takeBranch = false;
            switch (id_ex.funct3) {
                case 0b000: // BEQ
                    takeBranch = (rs1_val == rs2_val);
                    break;
                case 0b001: // BNE
                    takeBranch = (rs1_val != rs2_val);
                    break;
                case 0b100: // BLT
                    takeBranch = (rs1_val < rs2_val);
                    break;
                case 0b101: // BGE
                    takeBranch = (rs1_val >= rs2_val);
                    break;
                default:
                    cout << "Unknown branch instruction\n";
                    break;
            }
            if (takeBranch) {
                pc += id_ex.imm; // Update PC for branch
            }
        }
    }
    
    void run(vector<string>& bin_inst) {
        while (pc < bin_inst.size()) {
            cout << "\nCycle: " << pc + 1 << endl;

            // Execute the pipeline stages
            EX();
            ID();
            IF(bin_inst);
            
            // Update the pipeline registers
            // Move ID to EX
            id_ex = {};
            // Move IF to ID
            if_id = {};
            
            pc++;  // Increment PC
        }
    }
};

int main(){
    ifstream file("ass_inst.txt");
    vector<string> lines;
    string line;
    if (!file) {
        cerr << "No input file taken\n";
        return 1;
    }
    while (getline(file, line)) {
        lines.push_back(line);
    }
    file.close();

    // Convert hexadecimal instructions to binary strings.
    int n = lines.size();
    vector<string> binary_mc(n);
    for (int i = 0; i < n; i++) {
        ostringstream oss;
        for (char c : lines[i]) {
            if (c != ' ' && c != '\n' && c != '\t' && c != '\0')
                oss << hex_to_bin(c);
        }
        binary_mc[i] = oss.str();
        cout << "Instruction " << i << ": " << lines[i] << " -> " << binary_mc[i] << endl;
    }
    
    Processor proc;
    proc.run(binary_mc);
    return 0;
}