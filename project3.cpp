#include <chrono>
#include <iostream>
#include <fstream>
#include <cstring>
#include "mpi.h"

using namespace std;

const int MAX_LENGTH = 2048; 

// Function to read lines assigned to this process
string* readAssignedLines(int firstLine, int lastLine, int rank, string& dataFile, string* linesBuffer) {
    ifstream dataStream(dataFile);
    string currentLine;
    for (int i = 0; getline(dataStream, currentLine) && i < firstLine; ++i);

    int lineCount = firstLine;
    while (lineCount <= lastLine && getline(dataStream, currentLine)) {
        linesBuffer[lineCount - firstLine] = currentLine;
        lineCount++;
    }
    return linesBuffer;
}

// Function to determine the dimensions of the input file
void getFileDimensions(string& filePath, unsigned long& columns, unsigned long& rows) {
    ifstream dataStream(filePath);
    if (!dataStream) throw runtime_error("Failed to open file: " + filePath);

    string line;
    if (getline(dataStream, line)) {
        columns = line.length();
        rows = 1;
    }

    while (getline(dataStream, line)) rows++;
}

// Function to load the pattern from a file
void loadPattern(string& patternPath, unsigned long& patternWidth, unsigned long& patternHeight, string* patternLines) {
    ifstream patternStream(patternPath);
    if (!patternStream) throw runtime_error("Failed to open pattern file: " + patternPath);

    string line;
    patternHeight = 0;
    while (getline(patternStream, line)) {
        if (patternHeight == 0) patternWidth = line.length();
        if (patternHeight >= 10) break;  // Limit to 10 lines
        patternLines[patternHeight++] = line;
    }
}

// Function to request additional lines from the next process
void requestLinesFromNext(int linesNeeded, int rank, int size, int lineLength, string* extraLines) {
    if (rank + 1 >= size) {
        cerr << "No subsequent process to request lines from." << endl;
        return;
    }

    int nextRank = rank + 1;
    MPI_Ssend(&linesNeeded, 1, MPI_INT, nextRank, 0, MPI_COMM_WORLD);

    char* lineBuffer = new char[linesNeeded * (lineLength + 1)]();
    MPI_Recv(lineBuffer, linesNeeded * (lineLength + 1), MPI_CHAR, nextRank, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    int bufferPos = 0;
    for (int i = 0; i < linesNeeded; ++i) {
        extraLines[i].assign(&lineBuffer[bufferPos]);
        bufferPos += extraLines[i].length() + 1;  // +1 for null terminator
    }

    delete[] lineBuffer;
}

// Function to process pattern matching and request additional lines if needed
bool matchPatternAndRequest(string* lines, int start, int end, int columns, string* pattern, unsigned long patternHeight, int maxMatches, int** matchLocations, int& foundCount, bool& matchFound, int rank, bool& requestNeeded, int size, int& extraLinesNeeded, ofstream& outputFile) {
    int matchIndex = -1;
    matchFound = false;
    size_t patternWidth = pattern[0].length();
    int topRow, leftColumn;

    for (int i = 0; i < end; ++i) {
        for (int j = 0; j <= columns - patternWidth; ++j) {
            matchFound = true;
            for (size_t pi = 0; pi < patternHeight && matchFound; ++pi) {
                for (size_t pj = 0; pj < patternWidth && matchFound; ++pj) {
                    int currentLine = i + pi;
                    if (currentLine < end) {
                        if (lines[currentLine][j + pj] != pattern[pi][pj]) matchFound = false;
                    } else {
                        requestNeeded = true;
                        if (matchIndex < maxMatches - 1) {
                            topRow = i; leftColumn = j;
                            matchIndex++;
                            matchLocations[matchIndex][0] = topRow;
                            matchLocations[matchIndex][1] = leftColumn;
                            extraLinesNeeded = max(extraLinesNeeded, int(patternHeight - (end - i)));
                        }
                        matchFound = false;
                    }
                }
            }
        }
    }

    return requestNeeded;
}

// Main MPI dispatch function
void processMPI(string& filePath, int rank, int size, int firstLine, int lastLine, int columns, string* pattern, unsigned long patternHeight) {
    string* lines = new string[lastLine - firstLine + 1];
    readAssignedLines(firstLine, lastLine, rank, filePath, lines);

    int maxMatches = 100;
    int** matchLocations = new int*[maxMatches];
    for (int i = 0; i < maxMatches; ++i) {
        matchLocations[i] = new int[2];
        matchLocations[i][0] = -1; // Initialize first element
        matchLocations[i][1] = -1; // Initialize second element
    }

    int foundCount = 0;
    bool matchFound = false, requestNeeded = false;
    int extraLinesNeeded = 0;

    if (matchPatternAndRequest(lines, 0, lastLine - firstLine, columns, pattern, patternHeight, maxMatches, matchLocations, foundCount, matchFound, rank, requestNeeded, size, extraLinesNeeded)) {
        // Allocate space for additional lines
        string* extraLines = new string[extraLinesNeeded];

        // Request additional lines from the next process
        requestLinesFromNext(extraLinesNeeded, rank, size, columns, extraLines);

        // Integrate the additional lines into the existing set for continued processing
        // Here you might extend your existing data set or handle the extra lines separately
        string* combinedLines = new string[lastLine - firstLine + 1 + extraLinesNeeded];
        for (int i = 0; i < lastLine - firstLine + 1; i++) {
            combinedLines[i] = lines[i];
        }
        for (int i = 0; i < extraLinesNeeded; i++) {
            combinedLines[lastLine - firstLine + 1 + i] = extraLines[i];
        }

        // Continue the pattern matching on the combined set of lines
        bool continueMatchFound = false, continuePatternComplete = false;
        int continueNumFound = 0, continueNumMatches = 0;
        matchPatternAndRequest(combinedLines, 0, lastLine - firstLine + extraLinesNeeded, columns, pattern, patternHeight, maxMatches, matchLocations, continueNumFound, continueMatchFound, rank, requestNeeded, size, extraLinesNeeded);

        // Clean up
        delete[] extraLines;
        delete[] combinedLines;
    }


    // Cleanup
    for (int i = 0; i < maxMatches; ++i) delete[] matchLocations[i];
    delete[] matchLocations;
    delete[] lines;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    ofstream outputFile("output.txt");
    if (!outputFile.is_open()) {
        cerr << "Failed to open output file." << endl;
        return 1; 
    }

    int size, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (argc != 3) {
        if (rank == 0) cout << "Usage: " << argv[0] << " <input_file> <pattern_file>" << endl;
        MPI_Finalize();
        return 1;
    }

    string filePath = argv[1], patternPath = argv[2];
    unsigned long columns, rows, patternWidth, patternHeight;
    getFileDimensions(filePath, columns, rows);
    string* pattern = new string[10];
    loadPattern(patternPath, patternWidth, patternHeight, pattern);

    int linesPerProcess = rows / size;
    int firstLine = rank * linesPerProcess;
    int lastLine = (rank == size - 1) ? rows - 1 : firstLine + linesPerProcess - 1;

    processMPI(filePath, rank, size, firstLine, lastLine, columns, pattern, patternHeight);

    outputFile.close();

    delete[] pattern;
    MPI_Finalize();
    return 0;
}
