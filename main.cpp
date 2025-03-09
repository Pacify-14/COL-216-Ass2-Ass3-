# include<bits/stdc++.h>
# include <fstream>

using namespace std;

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
    for(auto&s : lines)cout<<s<<'\n';
    for(auto &s : binary_mc)cout<<s<<'\n';
}