#include <mpi.h>
#include <iostream>
#include <fstream>
#include <cstring>

const int MAX_LINE_LENGTH = 256;
const int MAX_LINES = 10000;

struct fileinfo {
    int numLines;
    int lineLength;
};

struct matchLocation {
    int x; // row 
    int y; // col
    int pl; // pattern line
};

struct matchCoordinate {
    int x;
    int y;
};

fileinfo getFileInfo(const char* filename) {
    std::ifstream file(filename);
    std::string line;
    fileinfo info = {0, 0};
    while (getline(file, line)) {
        info.numLines++;
        info.lineLength = line.length() > info.lineLength ? line.length() : info.lineLength;
    }
    file.close();
    return info;
}

matchCoordinate searchForRealMatches(matchLocation match, matchLocation** allMatches, int* numMatchesArr, int numPatternLines, int world_size) {
    for (int i = 0; i < world_size; i++) {
        for (int j = 0; j < numMatchesArr[i]; j++) {
            if (allMatches[i][j].y == match.y) {
                bool matchFound = true;
                for (int k = 0; k < numPatternLines; k++) {
                    if (allMatches[i][j].x + k != match.x || allMatches[i][j].pl + k != match.pl) {
                        matchFound = false;
                        break;
                    }
                }
                if (matchFound) {
                    matchCoordinate coord = {match.x, match.y};
                    return coord;
                }
            }
        }
    }
    matchCoordinate coord = {-1, -1};
    return coord;
}

int main(int argc, char** argv) {
    MPI_Init(NULL, NULL);
    int world_size, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    char inputLines[MAX_LINES][MAX_LINE_LENGTH] = {0};
    char patternLines[MAX_LINES][MAX_LINE_LENGTH] = {0};
    int numInputLines = 0, numPatternLines = 0;

    if (rank == 0) {
        fileinfo inputInfo = getFileInfo(argv[1]);
        fileinfo patternInfo = getFileInfo(argv[2]);
        numInputLines = inputInfo.numLines;
        numPatternLines = patternInfo.numLines;

        std::ifstream inputFile(argv[1]), patternFile(argv[2]);
        std::string line;

        int lineNum = 0;
        while (std::getline(inputFile, line)) {
            strncpy(inputLines[lineNum], line.c_str(), MAX_LINE_LENGTH);
            lineNum++;
        }

        lineNum = 0;
        while (std::getline(patternFile, line)) {
            strncpy(patternLines[lineNum], line.c_str(), MAX_LINE_LENGTH);
            lineNum++;
        }
    }

    MPI_Bcast(&numInputLines, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&numPatternLines, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Calculate sendCounts and displs for each process
    int* sendCounts = new int[world_size]();
    int* displs = new int[world_size]();

    for (int i = 0; i < world_size; ++i) {
        sendCounts[i] = (numInputLines / world_size) * MAX_LINE_LENGTH;
        if (i < numInputLines % world_size) {
            sendCounts[i] += MAX_LINE_LENGTH; // Handle the remainder
        }
        displs[i] = (i == 0 ? 0 : displs[i - 1] + sendCounts[i - 1]);
    }

    // Calculate linesPerProc based on sendCounts for the current process
    int linesPerProc = sendCounts[rank] / MAX_LINE_LENGTH;

    // Dynamically allocate space for myInputLines
    char** myInputLines = new char*[linesPerProc];
    for (int i = 0; i < linesPerProc; ++i) {
        myInputLines[i] = new char[MAX_LINE_LENGTH]();
    }


    char inputLinesFlat[MAX_LINES * MAX_LINE_LENGTH];
    if (rank == 0) {
        for (int i = 0; i < numInputLines; i++) {
            memcpy(inputLinesFlat + i * MAX_LINE_LENGTH, inputLines[i], MAX_LINE_LENGTH);
        }
    }


    MPI_Scatterv(inputLinesFlat, sendCounts, displs, MPI_CHAR, &myInputLines[0][0], linesPerProc * MAX_LINE_LENGTH, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Define an MPI datatype for matchLocation
    MPI_Datatype matchLocationType;
    MPI_Type_contiguous(3, MPI_INT, &matchLocationType);
    MPI_Type_commit(&matchLocationType);

    // Pattern matching and result gathering logic
    matchLocation* localMatches = new matchLocation[MAX_LINES]; // Space for potential matches
    int localMatchCount = 0;
    int startingLineIndex = displs[rank] / MAX_LINE_LENGTH;

    for (int i = 0; i <= linesPerProc - numPatternLines; ++i) {
        for (int j = 0; j < numPatternLines; ++j) {
            if (strstr(myInputLines[i + j], patternLines[j]) == NULL) {
                break;
            }
            if (j == numPatternLines - 1) {
                // Full pattern matched
                localMatches[localMatchCount].x = startingLineIndex + i; // Adjust row index based on the process
                localMatches[localMatchCount].y = 0; // Assuming pattern starts from the beginning of the line
                localMatches[localMatchCount].pl = 0; // Pattern line, not used in this context
                localMatchCount++;
            }
        }
    }



    // Gather the total count of matches from all processes
    int* allMatchCounts = new int[world_size]();
    MPI_Gather(&localMatchCount, 1, MPI_INT, allMatchCounts, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Calculate displacements for gathering matches
    int* displacements = new int[world_size]();
    int totalMatches = 0;
    if (rank == 0) {
        for (int i = 0; i < world_size; ++i) {
            displacements[i] = totalMatches;
            totalMatches += allMatchCounts[i];
        }
    }

    // Gather all matches to the root process
    matchLocation* allMatches = new matchLocation[totalMatches];
    MPI_Gatherv(localMatches, localMatchCount, matchLocationType, allMatches, allMatchCounts, displacements, matchLocationType, 0, MPI_COMM_WORLD);

    // Root process prints all match locations
    if (rank == 0) {
        for (int i = 0; i < totalMatches; ++i) {
            int lineStart = displs[allMatches[i].x / MAX_LINE_LENGTH] % MAX_LINE_LENGTH;
            std::cout << "Match found at row " << allMatches[i].x + displs[allMatches[i].x / MAX_LINE_LENGTH] / MAX_LINE_LENGTH << ", column " << allMatches[i].y + lineStart << std::endl;
        }
    }


    // Clean up
    delete[] localMatches;
    delete[] allMatchCounts;
    delete[] displacements;
    if (rank == 0) {
        delete[] allMatches;
    }


    // Free dynamically allocated memory
    for (int i = 0; i < linesPerProc; ++i) {
        delete[] myInputLines[i];
    }
    delete[] myInputLines;
    delete[] sendCounts;
    delete[] displs;

    MPI_Type_free(&matchLocationType);

    MPI_Finalize();
    return 0;
}
