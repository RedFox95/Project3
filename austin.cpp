#include <chrono>
#include <iostream>
#include <fstream>
#include <istream>
#include "mpi.h"

using namespace std;

const int MAX_COORD_LENGTH = 2048; 
const int MAX_COORDS = 1000; 

string* obtainLines(int startRow, int endRow, int worldRank, string& fileName, string* processLines) {
    ifstream file(fileName);
    string line;
    for (int i = 0; i < startRow && getline(file, line); ++i) {}
    int currentRow = startRow;
    int index = 0;
    while (currentRow <= endRow && getline(file, line)) {
        processLines[index] = line;
        index++;
        currentRow++;
    }
    return processLines;
}

void getNumColumnRow(string& fileName, unsigned long& numColumns, unsigned long& numRows) {
    ifstream inputFile(fileName);
    if (!inputFile) {
        cerr << "Error opening input file." << endl;
    }
    string line;
    if (getline(inputFile, line)) {
        numColumns = line.length();
        numRows++;
    }
    while (getline(inputFile, line)) {
        ++numRows;
    }
    inputFile.close();
}

void processPattern(string& patternName, unsigned long& numColumns, unsigned long& numRows, string* patternMatch) {
    ifstream inputFile(patternName);
    if (!inputFile) {
        cerr << "Error opening pattern file." << endl;
    }
    string line;
    numRows = 0;
    while (getline(inputFile, line)) {
        if (numRows == 0) {
            numColumns = line.length();
        }
        if (numRows < 10) {
            patternMatch[numRows] = line;
        } else {
            cout << "Warning: Pattern file has more than 10 lines; additional lines will be ignored." << endl;
            break;
        }
        numRows++;
    }
}

void requestAdditionalLines(int neededLines, int worldRank, int worldSize, int numColumns, string* receivedLines) {
    if (worldRank + 1 >= worldSize) {
        cout << "[" << worldRank << "] No next process to request lines from. Exiting." << endl;
        return;
    }
    int nextProcess = worldRank + 1;
    MPI_Ssend(&neededLines, 1, MPI_INT, nextProcess, 7, MPI_COMM_WORLD);

    const int maxLineSize = numColumns + 1;
    char* buffer = new char[neededLines * maxLineSize];
    memset(buffer, 0, size_t(neededLines) * maxLineSize - 1);
    MPI_Recv(buffer, neededLines * maxLineSize, MPI_CHAR, worldRank + 1, 10, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    int bufferIndex = 0;
    for (int i = 0; i < neededLines; i++) {
        receivedLines[i].assign(&buffer[bufferIndex]);
        bufferIndex += receivedLines[i].length() + 1;
    }

    delete[] buffer;
}

void addCoords(string& outputCoords) {
    ofstream outFile("output.txt");
    outFile << outputCoords << endl;
    outFile.close();
}

void convertAndGatherMasterCoords(const string& processCoords, int worldRank, int worldSize) {
    char localCoords[MAX_COORD_LENGTH];
    memset(localCoords, 0, MAX_COORD_LENGTH);
    strncpy(localCoords, processCoords.c_str(), MAX_COORD_LENGTH - 1);
    localCoords[MAX_COORD_LENGTH - 1] = '\0';

    char* allCoords = nullptr;
    if (worldRank == 0) {
        allCoords = new char[MAX_COORD_LENGTH * worldSize];
    }
    MPI_Gather(localCoords, MAX_COORD_LENGTH, MPI_CHAR, allCoords, MAX_COORD_LENGTH, MPI_CHAR, 0, MPI_COMM_WORLD);

    if (worldRank == 0) {
        string uniqueCoords[MAX_COORDS]; // Array to store unique coordinates
        int uniqueCount = 0; // Number of unique coordinates found

        for (int i = 0; i < worldSize; ++i) {
            // Extract coordinates from the current process's data
            char* procCoords = allCoords + i * MAX_COORD_LENGTH;
            char* coord = strtok(procCoords, "\n"); // Assuming coordinates are newline-separated

            while (coord != nullptr) {
                // Check if this coordinate is already in uniqueCoords
                bool isUnique = true;
                for (int j = 0; j < uniqueCount; ++j) {
                    if (strcmp(uniqueCoords[j].c_str(), coord) == 0) {
                        isUnique = false;
                        break;
                    }
                }

                if (isUnique) {
                    // If it's unique, add it to the list of unique coordinates
                    if (uniqueCount < MAX_COORDS) {
                        uniqueCoords[uniqueCount++] = string(coord);
                    } else {
                        cerr << "Reached maximum number of unique coordinates allowed." << endl;
                        break;
                    }
                }

                coord = strtok(nullptr, "\n");
            }
        }
        string combinedUniqueCoords;
        for (int i = 0; i < uniqueCount; ++i) {
            if (i > 0) combinedUniqueCoords += "\n";
            combinedUniqueCoords += uniqueCoords[i];
        }
        addCoords(combinedUniqueCoords);

        delete[] allCoords;
    }
}

void findMatchInAddedRows(string* lines, int end, string* patternMatch, unsigned long rowLength, int** foundPatterns, int columnLength, int origRowLeng, int numFound, int worldRank, int numMatches, int inputRowLeng) {
    int index = numFound - 1;
    cout << "[" << worldRank << "] Num found: " << numFound << endl;
    while (index < numMatches) {
        if (foundPatterns[index][0] > 0) {
            bool patternComplete = false;
            bool patternFound = true;
            int startRow = (inputRowLeng - foundPatterns[index][0]) + 1;
            int startCol = foundPatterns[index][1] - 1;

            int currentRow = 0;
            for (size_t pi = startRow; pi < rowLength - 1 && patternFound && !patternComplete; pi++) {
                for (size_t pj = 0; pj < columnLength && patternFound && !patternComplete; pj++) {
                    if (currentRow < end) {
                        if (lines[currentRow][startCol + pj] == patternMatch[pi][pj]) {
                            if (pi == rowLength - 1 && pj == columnLength - 1) {
                                patternComplete = true;
                            }
                        } else {
                            foundPatterns[index][0] = -1;
                            patternFound = false;
                        }
                    } else {
                        patternFound = false;
                    }
                }
                currentRow++;
            }
            index++;
            if (!patternComplete) {
                foundPatterns[index][0] = -1;
            }
        } else {
            index++;
        }
    }
}

bool searchPatternAndRequest(string* lines, int& start, int end, int numColumns, string* patternMatch, unsigned long rowLength, int maxMatches, int** foundPatterns, int& numFound, bool& patternFound, bool& patternComplete, int worldRank, bool& rowNotRequested, int worldSize, int& numRowsNeeded, int& numMatches) {
    int index = -1;
    patternFound = true;
    patternComplete = false;
    size_t columnLength = patternMatch[0].length();
    int topRow, topCol = 0;
    int lastRow = 0;
    int receivedLines;
    bool firstNotComplete = true;
    bool needsMoreLines = false;
    bool newPattern = false;
    int counter = 0;
    int needLines = 0;

    cout << "entering loop..." << endl;
    for (int i = 0; i < end; i++) {
        for (int j = 0; j <= numColumns - columnLength; j++) {
            patternFound = true; // Reset for each new starting position
            patternComplete = false;
            for (size_t pi = start; pi < rowLength && patternFound; pi++) {
                for (size_t pj = 0; pj < columnLength && patternFound; pj++) {
                    int currentRow = i + pi; // Adjust row index based on the subset of lines
                    if (currentRow < end) {
                        cout << "[" << worldRank << "] COMPARING: " << lines[currentRow][j + pj]
                            << " to " << patternMatch[pi][pj] << " on LINE: " << currentRow << endl;
                        if (lines[currentRow][j + pj] == patternMatch[pi][pj]) {
                            if (pi == 0 && pj == 0) {
                                topRow = i + 1; // Adjusting indexing as necessary
                                topCol = j + 1;
                                newPattern = true;
                            }
                            if (pi == rowLength - 1 && pj == columnLength - 1) {
                                patternComplete = true;
                                index++;
                                foundPatterns[index][0] = topRow; // Adjusting indexing as necessary
                                foundPatterns[index][1] = topCol;
                                numMatches++;
                            }
                        } else {
                            patternFound = false;
                        }
                    } else if (worldRank != worldSize - 1 && patternFound && !patternComplete) {
                        int temp = (rowLength - (currentRow - end)) - 1;
                        needsMoreLines = true;
                        if (index < maxMatches && newPattern) {
                            index++;
                            foundPatterns[index][0] = topRow; // Adjusting indexing as necessary
                            foundPatterns[index][1] = topCol;
                            cout << "[" << worldRank << "] FOUND COORD OUTSIDE BOUND: " << foundPatterns[index][0] << "," << foundPatterns[index][1] << endl;
                            numMatches++;
                            if (temp > numRowsNeeded) {
                                numRowsNeeded = temp;
                            }
                            index++;
                            newPattern = false;
                        } else if (index >= maxMatches) {
                            cout << "[" << worldRank << "] Error: Exceeded maxMatches. Not adding more patterns." << endl;
                        }
                        if (firstNotComplete) {
                            numFound = index;
                            firstNotComplete = false;
                        }
                    } else {
                        patternFound = false;
                    }
                }
                lastRow = pi;
            }

            if (worldRank != 0 && rowNotRequested) {
                counter++;
                MPI_Recv(&needLines, 1, MPI_INT, worldRank - 1, 18, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                rowNotRequested = false;

                if (needLines == 1) {
                    MPI_Recv(&receivedLines, 1, MPI_INT, worldRank - 1, 7, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    int totalSize = 0;
                    for (int index = 0; index < receivedLines; index++) {
                        totalSize += lines[index].length() + 1; // +1 for '\n' or '\0'
                    }
                    char* sendLines = new char[totalSize + 1];
                    sendLines[0] = '\0'; // Initialize the buffer
                    int currentPosition = 0;
                    for (int index = 0; index < receivedLines; index++) {
                        // Calculate the length of the current line
                        size_t lineLength = lines[index].length();

                        // Calculate remaining buffer size, ensuring space for newline and null terminator
                        size_t remainingSize = totalSize - currentPosition - 1; // -1 to reserve space for final '\0'

                        if (lineLength > remainingSize) {
                            // If the line won't fit, truncate it (or handle the error as you see fit)
                            lineLength = remainingSize;
                        }
                        strncpy(sendLines + currentPosition, lines[index].c_str(), lineLength);
                        currentPosition += lineLength;
                        sendLines[currentPosition] = '\0';
                        if (currentPosition < totalSize - 1) {
                            sendLines[currentPosition] = '\n';
                            currentPosition++; // Move past the newline
                        }
                        sendLines[currentPosition] = '\0';
                    }
                    sendLines[currentPosition] = '\0'; // Null-terminate the last line if required
                    int messageSize = currentPosition;
                    MPI_Ssend(sendLines, messageSize, MPI_CHAR, worldRank - 1, 10, MPI_COMM_WORLD);
                    delete[] sendLines;
                    needLines = 0;
                }
            }
        }

        if (i == end - 1 && patternComplete && patternMatch[0] == patternMatch[end - 1]) {
            index++;
            needsMoreLines = true;
            foundPatterns[index][0] = end - 1; // Adjusting indexing as necessary
            foundPatterns[index][1] = topCol;
            numMatches++;
            if (firstNotComplete) {
                numFound = index;
                firstNotComplete = false;
            }
        }
    }

    if (needsMoreLines) {
        cout << "[" << worldRank << "] 0" << endl;
        return true;
    } else {
        cout << "[" << worldRank << "] 3" << endl;
        return false;
    }
}

void acquireMasterCoords(int** coordArray, string& masterCoords, int startRow, int worldRank, int maxMatches) {
    if (!coordArray) {
        cout << "[" << worldRank << "] Error: coordArray is NULL." << endl;
        return; // Early return if coordArray is NULL
    }
    for (int index = 0; index < maxMatches; ++index) {
        if (!coordArray[index]) {
            cout << "[" << worldRank << "] Error: coordArray entry is NULL at index " << index << endl;
            break; // Break if a NULL entry is encountered
        }

        // Use the sentinel value to terminate the loop safely
        if (coordArray[index][0] > 0) {
            masterCoords += to_string(startRow + coordArray[index][0]) + "," + to_string(coordArray[index][1]) + "\n";
        }
    }
}

void dispatchMPI(string& fileName, int worldRank, int worldSize, int startRow, int endRow, int numColumns, string* patternMatch, unsigned long rowLength) {
    int counter = 0;
    int origRowLeng = endRow - startRow;
    string masterCoords = "";

    string* processLines = new string[origRowLeng + 1];
    processLines = obtainLines(startRow, endRow, worldRank, fileName, processLines);

    size_t columnLength = patternMatch[0].length();
    string coordOutput;
    int start = 0;
    int topCol = 0;
    int receivedLines;
    int numRowsNeeded = 0;
    int needLines = 0;
    bool rowNotRequested = true;
    bool needAdditionalRows = true;
    int moreLinesNeeded;
    bool useArray = false;
    int numMatches = 0;

    bool patternFound = true;
    bool patternComplete = false;

    int maxMatches = (numColumns - (numColumns - columnLength)) * (1 + (origRowLeng - rowLength)); // Example maximum
    int** foundPatterns = new int* [maxMatches];
    for (int i = 0; i < maxMatches; ++i) {
        foundPatterns[i] = new int[2]; // Each sub-array holds two integers for row and column
        foundPatterns[i][0] = -1;
        foundPatterns[i][1] = -1;
    }
    int numFound = -1;

    needAdditionalRows = searchPatternAndRequest(processLines, start, origRowLeng, numColumns, patternMatch,
        rowLength, maxMatches, foundPatterns, numFound, patternFound, patternComplete, worldRank, rowNotRequested, worldSize, numRowsNeeded, numMatches);

    rowNotRequested = true;

    if (worldRank != worldSize - 1) {
        if (needAdditionalRows) {
            moreLinesNeeded = 1;
            useArray = true;

            MPI_Ssend(&moreLinesNeeded, 1, MPI_INT, worldRank + 1, 18, MPI_COMM_WORLD);

            rowNotRequested = false;

            string* additionalLines = new string[numRowsNeeded];
            requestAdditionalLines(numRowsNeeded, worldRank, worldSize, numColumns, additionalLines);
            findMatchInAddedRows(additionalLines, numRowsNeeded, patternMatch, rowLength,
                foundPatterns, columnLength, origRowLeng, numFound, worldRank, numMatches, origRowLeng);
        } else {
            moreLinesNeeded = 0;
            MPI_Ssend(&moreLinesNeeded, 1, MPI_INT, worldRank + 1, 18, MPI_COMM_WORLD);
            rowNotRequested = false;
        }
    }

    acquireMasterCoords(foundPatterns, masterCoords, startRow, worldRank, numMatches);

    for (int i = 0; i < maxMatches; ++i) {
        delete[] foundPatterns[i];
    }
    delete[] foundPatterns;

    MPI_Barrier(MPI_COMM_WORLD);
    cout << "[" << worldRank << "] masterCoords: " << masterCoords << endl;
    convertAndGatherMasterCoords(masterCoords, worldRank, worldSize);

    cout << "[" << worldRank << "] Process complete. Cleaning up." << endl;
    delete[] processLines; // Clean up after processing all lines
}

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int worldSize, worldRank;
    MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
    MPI_Comm_rank(MPI_COMM_WORLD, &worldRank);

    if (argc != 3) {
        if (worldRank == 0) { // Only the master process should print the error
            std::cerr << "Usage: " << argv[0] << " <input_file> <pattern_file>" << std::endl;
        }
        MPI_Finalize();
        return 1;
    }

    string fileName = argv[1]; // Input file name from command line
    string patternName = argv[2]; // Pattern file name from command line
    
    unsigned long numColumns, numRows;
    getNumColumnRow(fileName, numColumns, numRows);

    string* patternMatch = new string[10]; // Assuming max 10 lines in pattern
    unsigned long patternNumColumns, patternNumRows;
    processPattern(patternName, patternNumColumns, patternNumRows, patternMatch);

    int startRow = 0; // Example start row
    int endRow = numRows; // Example end row, assuming processing the entire file
    dispatchMPI(fileName, worldRank, worldSize, startRow, endRow, numColumns, patternMatch, patternNumRows);

    delete[] patternMatch; // Clean up dynamically allocated memory

    MPI_Finalize();
    return 0;
}
