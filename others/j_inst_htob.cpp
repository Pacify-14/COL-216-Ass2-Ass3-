# include <bits/stdc++.h>
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
    string i1 = "ff1ff06f";
    string i2 = "ff3ff06f";
    vector<string>lines;
    lines.push_back(i1);
    lines.push_back(i2);
    vector<string>binary_mc(2);
    int i=0;
    for(auto & l : lines){
        ostringstream oss;
        for (auto c : l){
            if(c != ' ' && c != '\n' && c != '\t' && c != '\0')
            oss << hex_to_bin(c);
        }
        binary_mc[i] = oss.str();
        i++;
    }
    for(auto&l : binary_mc)cout<<l<<'\n';
}