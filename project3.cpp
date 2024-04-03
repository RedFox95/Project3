#include <iostream>
#include <fstream>
#include <cstring>
using namespace std;

void readFile(const char* filename, char* &data, int& rows, int& cols) {
    ifstream file(filename);
    string line;
    rows = 0;
    cols = 0;

    // First, find out the number of rows and cols
    while (getline(file, line)) {
        if (rows == 0) {
            cols = line.length();
        }
        rows++;
    }

    // Allocate memory for data
    data = new char[rows * cols];

    // Go back to the beginning of the file to actually read the data
    file.clear();
    file.seekg(0, ios::beg);
    int row = 0;
    while (getline(file, line)) {
        memcpy(data + row * cols, line.c_str(), cols);
        row++;
    }
}

bool checkMatch(char* grid, int row, int col, char* pattern, int gridCols, int patternCols, int patternRows) {
    for (int i = 0; i < patternRows; ++i) {
        for (int j = 0; j < patternCols; ++j) {
            if (grid[(row + i) * gridCols + (col + j)] != pattern[i * patternCols + j]) {
                return false;
            }
        }
    }
    return true;
}

void searchPattern(char* grid, int gridRows, int gridCols, char* pattern, int patternRows, int patternCols, ofstream& outputFile) {
    for (int row = 0; row <= gridRows - patternRows; ++row) {
        for (int col = 0; col <= gridCols - patternCols; ++col) {
            if (checkMatch(grid, row, col, pattern, gridCols, patternCols, patternRows)) {
                outputFile << "Pattern found at: " << row << "," << col << endl;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <input_file> <pattern_file> <output_file>" << endl;
        return 1;
    }

    char *grid = nullptr, *pattern = nullptr;
    int gridRows, gridCols, patternRows, patternCols;

    // Read the grid and pattern from files
    readFile(argv[1], grid, gridRows, gridCols);
    readFile(argv[2], pattern, patternRows, patternCols);

    // Open the output file
    ofstream outputFile(argv[3]);

    // Search for the pattern in the grid and write matches to the output file
    searchPattern(grid, gridRows, gridCols, pattern, patternRows, patternCols, outputFile);

    // Clean up
    delete[] grid;
    delete[] pattern;

    return 0;
}
