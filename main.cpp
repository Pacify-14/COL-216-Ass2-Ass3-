# include<bits/stdc++.h>
# include <fstream>

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

    if (c >= '0' && c <= '9') return lookup[c - '0'];
    if (c >= 'a' && c <= 'f') return lookup[c - 'a' + 10];

    return "Invalid";
}

class Processor{
    int pc = 0; // program counter
    // vector<int>reg_set(32,0); // set of 32 registers x0 - x31
    
    struct IF_ID_reg{
        int cur_pc;
        string out;
    };

    IF_ID_reg if_id;

    // storing binary instruction and pc (for beq type inst.s)
    void IF(IF_ID_reg &if_id, int cur_pc, vector<string>&bin_inst){
        if_id.out = bin_inst[cur_pc - 1]; // storing the output of the IF_ID pipeline as the binary string
        if_id.cur_pc = cur_pc;
    };


    struct R_type_ID_EX_reg {
        uint32_t funct7;
        uint32_t rs2;
        uint32_t rs1;
        uint32_t funct3;
        uint32_t rd;
        uint32_t opcode;
    };
    
    struct I_type_ID_EX_reg {
        uint32_t imm;
        uint32_t rs1;
        uint32_t funct3;
        uint32_t rd;
        uint32_t opcode;
    };
    
    struct S_type_ID_EX_reg {
        uint32_t imm;
        uint32_t rs2;
        uint32_t rs1;
        uint32_t funct3;
        uint32_t opcode;
    };
    
    struct U_type_ID_EX_reg {
        uint32_t imm;
        uint32_t rd;
        uint32_t opcode;
    };
    
    struct B_type_ID_EX_reg{
        uint32_t imm;
        uint32_t rs2;
        uint32_t rs1;
        uint32_t funct3;
        uint32_t opcode;
    };
    
    struct J_type_ID_EX_reg{
        uint32_t imm;
        uint32_t rd;

        uint32_t opcode;
    };
R_type_ID_EX_reg r_type_ID_EX_reg;I_type_ID_EX_reg i_type_ID_EX_reg;U_type_ID_EX_reg u_type_ID_EX_reg;S_type_ID_EX_reg stype;B_type_ID_EX_reg btype;J_type_ID_EX_reg jtype;
char inst_type;
void ID(IF_ID_reg &if_id) {
    string opcode = if_id.out.substr(25,7);
    if(opcode == "0110011") {
        inst_type = 'R';
        r_type_ID_EX_reg.funct7 = binToUint(if_id.out.substr(0,7));
        r_type_ID_EX_reg.rs2 = binToUint(if_id.out.substr(7,5));
        r_type_ID_EX_reg.rs1 = binToUint(if_id.out.substr(12,5));
        r_type_ID_EX_reg.funct3 = binToUint(if_id.out.substr(17,3));
        r_type_ID_EX_reg.rd = binToUint(if_id.out.substr(20,5));
        r_type_ID_EX_reg.opcode = binToUint(opcode);
    } else if(opcode == "0010011" || opcode == "0000011" || opcode == "1100111" || opcode == "1110011") {
        inst_type = 'I';
        uint32_t rawImm = binToUint(if_id.out.substr(0,12));
        i_type_ID_EX_reg.imm = signExtend32(rawImm, 12);
        i_type_ID_EX_reg.rs1 = binToUint(if_id.out.substr(12,5));
        i_type_ID_EX_reg.funct3 = binToUint(if_id.out.substr(17,3));
        i_type_ID_EX_reg.rd = binToUint(if_id.out.substr(20,5));
        i_type_ID_EX_reg.opcode = binToUint(opcode);
    } else if(opcode == "0100011") {
        inst_type = 'S';
        uint32_t imm_part1 = binToUint(if_id.out.substr(0,7));
        uint32_t imm_part2 = binToUint(if_id.out.substr(20,5));
        uint32_t rawImm = (imm_part1 << 5) | imm_part2;
        stype.imm = signExtend32(rawImm, 12);
        stype.rs2 = binToUint(if_id.out.substr(7,5));
        stype.rs1 = binToUint(if_id.out.substr(12,5));
        stype.funct3 = binToUint(if_id.out.substr(17,3));
        stype.opcode = binToUint(opcode);
    } else if(opcode == "0110111" || opcode == "0010111") {
        inst_type = 'U';
        u_type_ID_EX_reg.imm = binToUint(if_id.out.substr(0,20));
        u_type_ID_EX_reg.rd = binToUint(if_id.out.substr(20,5));
        u_type_ID_EX_reg.opcode = binToUint(opcode);
    } else if(opcode == "1100011") {
        inst_type = 'B';
        uint32_t imm_part1 = binToUint(if_id.out.substr(0,7));
        uint32_t imm_part2 = binToUint(if_id.out.substr(20,5));
        uint32_t imm12 = (imm_part1 >> 6) & 0x1;
        uint32_t imm10_5 = imm_part1 & 0x3F;
        uint32_t imm11 = (imm_part2 >> 4) & 0x1;
        uint32_t imm4_1 = imm_part2 & 0xF;
        uint32_t rawImm = (imm12 << 12) | (imm11 << 11) | (imm10_5 << 5) | (imm4_1 << 1);
        btype.imm = signExtend32(rawImm, 13);
        btype.rs2 = binToUint(if_id.out.substr(7,5));
        btype.rs1 = binToUint(if_id.out.substr(12,5));
        btype.funct3 = binToUint(if_id.out.substr(17,3));
        btype.opcode = binToUint(opcode);
    } else if(opcode == "1101111") {
        inst_type = 'J';
        uint32_t imm20 = binToUint(if_id.out.substr(0,1));
        uint32_t imm10_1 = binToUint(if_id.out.substr(1,10));
        uint32_t imm11 = binToUint(if_id.out.substr(11,1));
        uint32_t imm19_12 = binToUint(if_id.out.substr(12,8));
        uint32_t rawImm = (imm20 << 20) | (imm19_12 << 12) | (imm11 << 11) | (imm10_1 << 1);
        jtype.imm = signExtend32(rawImm, 21);
        jtype.rd = binToUint(if_id.out.substr(20,5));
        jtype.opcode = binToUint(opcode);
    }
}
void EX(){
    if(inst_type == 'R'){
        auto reg = r_type_ID_EX_reg;
        
    }
    else if(inst_type == 'I')auto temp = i_type_ID_EX_reg;
    else if(inst_type == 'S')auto temp = stype;
    else if(inst_type == 'U')auto temp = u_type_ID_EX_reg;
    else if(inst_type == 'B')auto temp = btype;
    else auto temp = jtype;


}
};

int main(){
    ifstream file("ass_inst.txt");
    vector<string> lines;
    string line;
    if(!file){
        cerr << "No input file taken\n";
        return 1;
    }

    while(getline(file, line)){
        lines.push_back(line);
    }     

    file.close();

    // hex to bin conversion string
    int n = size(lines);
    int i = 0;
    vector<string>binary_mc(n);
    for(auto & l : lines){
        ostringstream oss;
        for (auto c : l){
            if(c != ' ' && c != '\n' && c != '\t' && c != '\0')
            oss << hex_to_bin(c);
        }
        binary_mc[i] = oss.str();
        i++;
    }
    for(auto &l : binary_mc[i]){
        // pass to processer for processing each instruction
    }
    // for(auto&s : lines)cout<<s<<'\n';
    // for(auto &s : binary_mc)cout<<s<<'\n';
}