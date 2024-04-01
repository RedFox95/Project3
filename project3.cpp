#include <mpi.h>
#include <iostream>
#include <fstream>
#include <cstring>
using namespace std;

void readFile(const char* filename, char* data, int& rows, int& cols) {
    ifstream file(filename);
    string line;
    rows = 0;
    cols = 0;

    while (getline(file, line)) {
        if (rows == 0) {
            // Determine the number of columns from the first line
            cols = line.length();
        }
        // Copy the line contents into the data array
        memcpy(data + rows * cols, line.c_str(), cols);
        rows++;
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

void searchPattern(char* grid, int gridRows, int gridCols, char* pattern, int patternRows, int patternCols, int rank) {
    for (int row = 0; row <= gridRows - patternRows; ++row) {
        for (int col = 0; col <= gridCols - patternCols; ++col) {
            if (checkMatch(grid, row, col, pattern, gridCols, patternCols, patternRows)) {
                cout << "Pattern found by process " << rank << " at: " << row << "," << col << endl;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    if (argc < 3) {
        if (world_rank == 0) {
            cerr << "Usage: " << argv[0] << " <input_file> <pattern_file>" << endl;
        }
        MPI_Finalize();
        return 1;
    }

    // Initialize sizes
    int gridRows = 0, gridCols = 0, patternRows = 0, patternCols = 0;

    // Allocate memory for the grid and pattern on the root process
    char* grid = nullptr;
    char* pattern = nullptr;
    if (world_rank == 0) {
        // Placeholder for determining the size of the grid and pattern
        // This should be replaced with actual logic to determine sizes
        gridRows = 10; // Example size, replace with actual logic
        gridCols = 10; // Example size, replace with actual logic
        patternRows = 2; // Example size, replace with actual logic
        patternCols = 2; // Example size, replace with actual logic

        grid = new char[gridRows * gridCols];
        pattern = new char[patternRows * patternCols];

        // Placeholder for reading the grid and pattern from files
        // Replace with actual file reading logic
        readFile(argv[1], grid, gridRows, gridCols);
        readFile(argv[2], pattern, patternRows, patternCols);
    }

    // Broadcast the sizes to all processes
    MPI_Bcast(&gridRows, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&gridCols, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&patternRows, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&patternCols, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Allocate memory for local grid and pattern in all processes
    int rowsPerProcess = gridRows / world_size;
    char* localGrid = new char[rowsPerProcess * gridCols];

    // Scatter the grid to all processes
    MPI_Scatter(grid, rowsPerProcess * gridCols, MPI_CHAR, localGrid, rowsPerProcess * gridCols, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Broadcast the pattern to all processes
    if (world_rank != 0) {
        // Non-root processes also need to allocate memory for the pattern
        pattern = new char[patternRows * patternCols];
    }
    MPI_Bcast(pattern, patternRows * patternCols, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Each process searches its portion of the grid
    searchPattern(localGrid, rowsPerProcess, gridCols, pattern, patternRows, patternCols, world_rank);

    // Clean up dynamically allocated memory
    delete[] localGrid;
    if (pattern != nullptr) {
        delete[] pattern;
    }
    if (world_rank == 0 && grid != nullptr) {
        delete[] grid;
    }

    MPI_Finalize();
    return 0;
}
