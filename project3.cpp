#include <mpi.h>
#include <iostream>
#include <fstream>
#include <cstring>

using namespace std;

// Function to search for matches in a line
void searchForMatches(const char* line, int lineNumber, const char* pattern, int rank) {
    const char* pos = line;
    while ((pos = strstr(pos, pattern)) != NULL) {
        size_t matchPos = pos - line;
        cout << "Match found at line " << lineNumber << ", position " << matchPos << " on process " << rank << endl;
        pos += strlen(pattern); // Move past the current match to avoid finding overlapping matches
    }
}


int main(int argc, char **argv) {
    MPI_Init(NULL, NULL);
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // Check if there are enough command-line arguments
    if (argc < 3) {
        if (rank == 0) {
            cerr << "Usage: " << argv[0] << " <input_file> <pattern_file>" << endl;
        }
        MPI_Finalize();
        return 1;
    }

    // Read the pattern file
    string pattern;
    ifstream patternFile(argv[2]);
    if (patternFile.is_open()) {
        getline(patternFile, pattern);
        patternFile.close();
    } else {
        cerr << "Unable to open pattern file" << endl;
        MPI_Finalize();
        return 1;
    }

    // Read and search for matches in the input file
    string line;
    ifstream inputFile(argv[1]);
    int lineNumber = 0;
    while (getline(inputFile, line)) {
        // Search for matches in the line
        searchForMatches(line.c_str(), lineNumber, pattern.c_str(), rank);
        lineNumber++;
    }
    inputFile.close();

    MPI_Finalize();
    return 0;
}
