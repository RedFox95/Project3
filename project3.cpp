#include <mpi.h>
#include <iostream>
#include <fstream>
#include <string>


/**
 * later improvements if time
 * - any other way to get the file length? this will be way too slow for bigger files 
 * - when looking at the matches from node 0, ignore if it's the first line for any pattern matches that are not the first line of the pattern
 * - turn dynamic arrays into linked lists?
*/

using namespace std;

struct fileinfo {
    size_t numLines;
    size_t lineLength; // idk if line lenth is needed, if not then delete this and just reutrn the num of linse inthe method
};

struct matchLocation {
    size_t x; // row 
    size_t y; // col
    size_t pl; // pattern line
};

struct matchCoordinate {
    size_t x;
    size_t y;
};

/**
 * Get the fileinfo struct for this file, containing the number of lines and length of a line.
*/
fileinfo getFileInfo(char* filename) {
    ifstream lineCounter(filename);
    size_t numLines = 0;
    string line;
    while (getline (lineCounter, line)) numLines++;
    lineCounter.close();
    struct fileinfo retVal = {numLines, line.length()};
    return retVal;
}

/**
 * Returns the upper leftmost coordinate of a full match, otherwise return null if no full match.
*/
matchCoordinate searchForRealMatches(matchLocation match, matchLocation ** allMatches, int * numMatchesArr, fileinfo patternInfo) {
    matchLocation * patternMatchLocations = new matchLocation[patternInfo.numLines];
    for (int i = 0; i < patternInfo.numLines; i++) patternMatchLocations[i] = nullptr;
    patternMatchLocations[match.pl] = match;
    for (int i = 0; i < world_size; i++) {
        for (int j = 0; j < numMatchesArr[i]; j++) {
            bool fullMatch = true;
            // for each match...
            if (allMatches[i][j].y == match.y) {
                // if it's in the correct column
                for (int k = 0; k < patternInfo.numLines; k++) {
                    if (allMatches[i][j].x == match.x+k && allMatches[i][j].pl == match.pl+k) {
                        // this is a corresponding match!
                        patternMatchLocations[allMatches[i][j].pl] = allMatches[i][j];
                    }
                    // check if full match 
                    if (patternMatchLocations[k] == nullptr) fullMatch = false;
                    if (fullMatch) {
                        matchCoordinate retVal = matchCoordinate{patternMatchLocations[0].x, patternMatchLocations[0].y};
                        return retVal;
                    }
                }
            }
        }
    }
    return nullptr;
}

int main(int argc, char ** argv) {
    /* necessary setup for MPI */
    MPI_Init(NULL,NULL);
    int world_size; // how many total nodes we have
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    int rank; // rank is the node number
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /* vars for storing things that are used in MPI method calls */
    char ** inputLines = nullptr;
    char ** patternLines = nullptr;
    int numLinesPerNode = -1;    
    char ** INInputLines = nullptr; // where IN means individual node
    // char ** INPatternLines = nullptr; // where IN means individual node // not needed for bcast

    /* one node reads input and pattern files */
    fileinfo inputInfo;
    fileinfo patternInfo;
    if (rank == 0) {
        /* get the number of lines in the input file */
        string inputFile = argv[1];
        inputInfo = getFileInfo(inputFile);

        /* read the input file in line by line and store in array */
        ifstream file(inputFile);
        // string allLines[inputInfo.numLines + 1];
        inputLines = new char*[inputInfo.numLines]; // num rows (lines)
        for (int i = 0; i < inputInfo.numLines; i++) {
            inputLines[i] = new char[inputInfo.lineLength]; // num cols (line length)
        }
        string line;
        size_t lineNum = 0; // for indexing into the allLines arr
        while(getline(file, line)) {
            // allLines[lineNum] = line;
            strcpy(inputLines[lineNum], line.c_str());
            lineNum++;
        }

        /* get the number of lines in the pattern file */
        string patternFile = argv[2];
        patternInfo = getFileInfo(patternFile);

        /* read the pattern file in line by line and store in array */
        ifstream patternFileStream(patternFile);
        // string pattern[numPatternLines + 1];
        patternLines = new char*[patternInfo.numLines]; // num rows (lines)
        for (int i = 0; i < patternInfo.numLines; i++) {
            patternLines[i] = new char[patternInfo.lineLength]; // num cols (line length)
        }        
        lineNum = 0; // for indexing into the pattern arr
        while(getline(patternFileStream, line)) {
            // pattern[lineNum] = line;
            strcpy(patternLines[lineNum], line.c_str());
            lineNum++;
        }

        // divide lines by num of nodes so you know how big to prep array for receiving input lines
        numLinesPerNode = (inputInfo.numLines / world_size) + 1; // add 1 to account for weird integer division stuff
    }

    /* scatter lines of input file */
    // inputLines is a double pointer (array of array of chars)
    // want to send each node a set of 1 lines (maybe change this later?)
    // MPI_CHAR - char arrays
    // INInputLines - the buffer to place the lines for each node 
    // receive 1 ? 
    // 0 tag for the scatter
    INInputLines = new char*[numLinesPerNode];
    for (int i = 0; i < numLinesPerNode; i++) {
        INInputLines[i] = new char[inputInfo.lineLength];
    }
    MPI_Scatter(inputLines, 1, MPI_CHAR, INInputLines, 1, MPI_CHAR, 0, MPI_COMM_WORLD);


    /* broadcast entire pattern file */
    // broadcast the patternLines (so this var is the same across nodes)
    // count - the number of lines i think
    // MPI_CHAR i think - but it's char array?
    // root node = 0
    MPI_Bcast(patternLines, patternInfo.numLines, MPI_CHAR, 0, MPI_COMM_WORLD);

    /* for each line in the input file, compare to the pattern file */
    // first convert to strings to make this easier
    string pattern[patternInfo.numLines];
    for (int i = 0; patternInfo.numLines; i++) {
        pattern[i] = string(patternLines[i]);
    }

    string input[inputInfo.numLines];
    for (int i = 0; inputInfo.numLines; i++) {
        input[i] = string(inputLines[i]);
    }

    // now use the find method
    size_t sizeOfMatchArr = 10; // 10 for now... then dynamically increase if needed
    matchLocation * matchArr = new matchLocation[sizeOfMatchArr]; 
    size_t numMatches = 0;
    for (int i = 0; i < inputInfo.numLines; i++) {
        for (int j = 0; j < patternInfo.numLines; j++) {
            size_t pos = 0;
            size_t found = input[i].find(pattern[j], pos);
            while (found != string::pos) {
                if (numMatches >= sizeOfMatchArr) {
                    // increase matchArr size 
                    sizt_t biggerSize = sizeOfMatchArr * 2;
                    matchLocation * biggerArr = new matchLocation[biggerSize];
                    memcpy(biggerArr,matchArr,sizeof(matchLocation)*numMatches);
                    delete[] matchArr;
                    matchArr = biggerArr;
                }
                // store the match
                matchArr[numMatches] = matchLocation {i, found, j}; // where i is the row number, found is the col number, and j is the pattern line number
                // update pos
                pos = found; // or found + 1?
                // update numMatches
                numMatches++;
                // update found
                found = input[i].find(pattern[j], pos);
            }
        }
    }

    /* bring all the matches to node 0 for comparison */

    // Define MPI data types
    MPI_Datatype mpi_matchLocation_type;
    MPI_Type_contiguous(3, MPI_UNSIGNED_LONG, &mpi_matchLocation_type);
    MPI_Type_commit(&mpi_matchLocation_type);
    if (rank != 0) {
        // TODO first send the number of matches (numMAtches)
        // Pack the struct into a buffer
        int pack_size;
        MPI_Pack_size(1, mpi_matchLocation_type, MPI_COMM_WORLD, &pack_size);
        char* buffer = new char[pack_size];
        int position = 0;
        MPI_Pack(matchArr, 1, mpi_matchLocation_type, buffer, pack_size, &position);
        // at the end of MPI_Pack above, position was set to be the size that we can use in MPI_Send
        // destination is node 0
        // tag for this message is 1
        MPI_Send(buffer, position, MPI_PACKED, 0, 1, MPI_COMM_WORLD);
    } else { // rank == 0
        // need an array of arrays to receive 
        matchLocation ** allMatchLocations = new matchLocation*[world_size];
        int * numMatchesArr = new int[world_size];
        // iterate through and store the arrays on recvs 
        for (int i = 0; i < world_size; i++) {
            // TODO first receive the number of matches 
            int numMatchesReceived;
            numMatchesArr[i] = numMatchesReceived;
            // Receive the packed buffer
            MPI_Status status;
            int count;
            // 1 is the source, also the tag is 1
            MPI_Probe(1, 1, MPI_COMM_WORLD, &status);
            MPI_Get_count(&status, MPI_PACKED, &count); // i think we want to do this for each arr form each node
            char* buffer = new char[count];
            MPI_Recv(buffer, count, MPI_PACKED, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            // Unpack the buffer into the struct
            int position = 0;
            Point points[num_points];
            matchLocation * matchLocations = new matchLocation[numMatchesReceived];
            MPI_Unpack(buffer, count, &position, matchLocations, numMatchesReceived, mpi_matchLocation_type, MPI_COMM_WORLD);
            allMatchLocations[i] = matchLocations;
            delete buffer;
        }

        /* node 0 compares coordinates to find the full matches */
        size_t sizeOfCoordArr = 10; // 10 for now... then dynamically increase if needed
        matchCoordinate * coordArr = new matchCoordinate[sizeOfCoordArr]; 
        size_t numCoords = 0;        
        for (int i = 0; i < world_size; i++) {
            for (int j = 0; j < numMatchesArr[i]; j++) {
                matchCoordinate coor = searchForRealMatches(allMatchLocations[i][j], allMatchLocations, numMatchesArr, patternInfo);
                bool alreadyFound = false;
                for (int k = 0; k < numCoords; k++) {
                    if (coordArr[k].x == coor.x && coordArr[k].y == coor.y) alreadyFound = true;
                }
                if (alreadyFound) continue; // go to next match
                if (numCoords >= sizeOfCoordArr) {
                    // increase coordArr size 
                    sizt_t biggerSize = sizeOfCoordArr * 2;
                    matchCoordinate * biggerArr = new matchCoordinate[biggerSize];
                    memcpy(biggerArr,coordArr,sizeof(matchCoordinate)*numCoords);
                    delete[] coordArr;
                    coordArr = biggerArr;
                }                
                coordArr[numCoords] = coor;
                numCoords++;
                cout << coor.x << ", " << coor.y << endl;
                /* write to output file */
                // TODO output coordinates to file instead of printing them
            }
        }



        // cleanup at end of this
        for (int i = 0; i < world_size; i++) {
            delete[] allMatchLocations[i];
        }
        delete[] allMatchLocations;
    }

    /* necessary teardown */
    // TODO free anything we malloc'd (make sure to use delete with [] if necessary)
    MPI_Finalize();
    return 0;
}