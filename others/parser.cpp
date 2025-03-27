#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

int main() {
    std::ifstream inputFile("whole.txt");
    std::ofstream outFile1("inp1.txt");
    std::ofstream outFile2("inp2.txt");
    
    if (!inputFile || !outFile1 || !outFile2) {
        std::cerr << "Error opening file!" << std::endl;
        return 1;
    }
    
    std::string line;
    while (std::getline(inputFile, line)) {
        std::istringstream iss(line);
        std::string col1, col2;
        
        // Read the first two columns
        if (!(iss >> col1 >> col2)) {
            continue; // Skip if line doesn't have enough columns
        }
        
        std::string restOfLine;
        std::getline(iss >> std::ws, restOfLine); // Read the rest of the line
        
        outFile1 << col2 << "\n";
        outFile2 << restOfLine << "\n";
    }
    
    inputFile.close();
    outFile1.close();
    outFile2.close();
    
    return 0;
}
