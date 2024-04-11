// Project3.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <mpi.h>
#include <iostream>
#include <fstream>
#include <string>


/**
 * TODO
 * - test with 10+ matches assigned to one node to find
 * - test with 10+ total matches
 *
 * later improvements if time
 * - any other way to get the file length? this will be way too slow for bigger files
 * - when looking at the matches from node 0, ignore if it's the first line for any pattern matches that are not the first line of the pattern
 * - turn dynamic arrays into linked lists?
*/

using namespace std;

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


 //Get the fileinfo struct for this file, containing the number of lines and length of a line.

fileinfo getFileInfo(string filename) {
    ifstream lineCounter(filename);
    int numLines = 0;
    string line;
    while (getline(lineCounter, line)) numLines++;
    lineCounter.close();
    struct fileinfo retVal = { numLines, line.length() };
    return retVal;
}


 //Returns the upper leftmost coordinate of a full match, otherwise return null if no full match.

matchCoordinate searchForRealMatches(matchLocation match, matchLocation** allMatches, int* numMatchesArr, int numPatternLines, int world_size) {
    cout << "-> searchForRealMAtches x:" << match.x << " y: " << match.y << " pl: " << match.pl << endl;
    matchLocation** patternMatchLocations = new matchLocation * [numPatternLines];
    for (int i = 0; i < numPatternLines; i++) patternMatchLocations[i] = nullptr;

    patternMatchLocations[match.pl] = &match;
    for (int i = 0; i < world_size; i++) {
        for (int j = 0; j < numMatchesArr[i]; j++) {
            bool fullMatch = true;
            // for each match...
            if (allMatches[i][j].y == match.y && allMatches[i][j].x != match.x && allMatches[i][j].pl != match.pl) {
                // if it's in the correct column
                for (int k = 0; k < numPatternLines; k++) {
                    if (allMatches[i][j].x == match.x + k && allMatches[i][j].pl == match.pl + k) {
                        // this is a corresponding match!
                        patternMatchLocations[allMatches[i][j].pl] = &allMatches[i][j];
                    }
                    // check if full match 
                    if (patternMatchLocations[k] == nullptr) fullMatch = false;
                }
                if (fullMatch) {
                    struct matchCoordinate retVal = { patternMatchLocations[0]->x, patternMatchLocations[0]->y };
                    delete[] patternMatchLocations;
                    return retVal;
                }
            }
        }
    }
    // return -1, -1 if no match found 
    struct matchCoordinate retVal = { -1, -1 };
    return retVal;
}

int main(int argc, char** argv) {
    // necessary setup for MPI
    MPI_Init(NULL, NULL);
    int world_size; // how many total nodes we have
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    int rank; // rank is the node number
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    cout << rank << ": starting up" << endl;
    // vars for storing things that are used in MPI method calls
    char** inputLines = nullptr;
    char** patternLines = nullptr;
    int numLinesPerNode = -1;
    char** INInputLines = nullptr; // where IN means individual node (the lines for each node to analyze)
    int* numPatternLinesPtr = (int*)malloc(sizeof(int));
    int* numInputLinesPtr = (int*)malloc(sizeof(int));;
    int* lenPatternLinesPtr = (int*)malloc(sizeof(int));;
    int* lenInputLinesPtr = (int*)malloc(sizeof(int));;

    if (rank == 0) {
        // get info about inout file
        cout << "here1" << endl;
        string inputFile = argv[1];
        cout << "here2" << endl;
        fileinfo inputInfo = getFileInfo(inputFile);
        *numInputLinesPtr = inputInfo.numLines;
        *lenInputLinesPtr = inputInfo.lineLength + 1;
        cout << "here11" << endl;
        // read the input file in line by line and store in array
        ifstream file(inputFile);
        inputLines = new char* [*numInputLinesPtr]; // num rows (lines)
        for (int i = 0; i < *numInputLinesPtr; i++) {
            inputLines[i] = new char[*lenInputLinesPtr]; // num cols (line length)
        }
        cout << "here 22" << endl;
        string line;
        int lineNum = 0; // for indexing into the allLines arr
        while (getline(file, line)) {
            cout << lineNum << endl;
            strcpy_s(inputLines[lineNum], *lenInputLinesPtr, line.c_str());
            lineNum++;
        }
        cout << "here3 len of c str is " << endl;
        // get info about the pattern file
        string patternFile = argv[2];
        fileinfo patternInfo = getFileInfo(patternFile);
        *numPatternLinesPtr = patternInfo.numLines;
        *lenPatternLinesPtr = patternInfo.lineLength + 1;

        // read the pattern file in line by line and store in array
        ifstream patternFileStream(patternFile);
        patternLines = new char* [*numPatternLinesPtr]; // num rows (lines)
        for (int i = 0; i < *numPatternLinesPtr; i++) {
            patternLines[i] = new char[*lenPatternLinesPtr]; // num cols (line length)
        }
        lineNum = 0; // for indexing into the pattern arr
        while (getline(patternFileStream, line)) {
            strcpy_s(patternLines[lineNum], *lenPatternLinesPtr, line.c_str());
            lineNum++;
        }
        cout << "here4 " << endl;
    }

    

    // need to bcast number of lines and length of line for both files
    MPI_Bcast(numPatternLinesPtr, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(numInputLinesPtr, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(lenPatternLinesPtr, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(lenInputLinesPtr, 1, MPI_INT, 0, MPI_COMM_WORLD);
    int numPatternLines = *numPatternLinesPtr;
    int numInputLines = *numInputLinesPtr;
    int lenPatternLines = *lenPatternLinesPtr;
    int lenInputLines = *lenInputLinesPtr;
    cout << rank << ": after bcasting info " << numPatternLines << " " << numInputLines << " " << lenPatternLines << " " << lenInputLines << endl;
    // set the number of lines for this node
    int numLinesPerNodeLower = (int)numInputLines / world_size;
    int remainder = numInputLines % world_size;
    if (rank <= remainder - 1) {
        numLinesPerNode = numLinesPerNodeLower;
    }
    else {
        numLinesPerNode = numLinesPerNodeLower;
    }
    cout << rank << ": tmg" << endl;
    // setup a place for each node to store its assigned lines to process
    INInputLines = new char* [numLinesPerNode];
    cout << rank << ": tmg3" << endl;
    for (int i = 0; i < numLinesPerNode; i++) {
        cout << rank << ": tmg4 i is " << i << endl;
        INInputLines[i] = new char[lenInputLines];
   //     INInputLines[i] = (char*)malloc(sizeof(char)*lenInputLines);
    }
    cout << rank << ": tmg2" << endl;
    // alternate sending the lines to different nodes from node 0
    if (rank == 0) {
        int numPlacedInSelf = 0;
        for (int i = 0; i < numInputLines; i++) {
            // send input lines to alternating nodes
            int destNode = i % world_size;
            if (destNode != 0) {
                cout << "dest node is " << destNode << endl;
                MPI_Send(inputLines[i], lenInputLines, MPI_CHAR, destNode, 6, MPI_COMM_WORLD); // tag is 6
            }
            else {
                // for lines assigned to node 0, just store them directly in the array
                INInputLines[numPlacedInSelf] = inputLines[i];
                numPlacedInSelf++;
            }
        }
        cout << rank << ": NUM PLACED IN SELF IS " << numPlacedInSelf << " of " << numLinesPerNode << endl;
    }
    else {
        cout << rank << ": numlinespernode is " << numLinesPerNode << endl;
        for (int i = 0; i < numLinesPerNode; i++) {
            cout << rank << ": attempting to receive the " << i << " line of data" << endl;
            MPI_Recv(INInputLines[i], lenInputLines, MPI_CHAR, 0, 6, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // tag is 6
            cout << "tmg888" << endl;
        }
    }
    cout << rank <<": after send/recv tag 6" << endl;
    // prepare a place for the nodes to store the pattern file
    if (rank != 0) {
        cout << rank << ": before making patternlines" << endl;
        patternLines = new char* [numPatternLines];
        //patternLines = (char**)malloc(sizeof(char*) * numPatternLines);
        cout << rank << ": in pattenr prep " << endl;
        for (int i = 0; i < numPatternLines; i++) {
            cout << rank << ": prepping " << i << " out of " << numPatternLines << endl;
            cout << "len patern lines is " << lenPatternLines << endl;
            patternLines[i] = new char[lenPatternLines];
        }
    }
    cout << rank <<": after prepping pattern lines" << endl;
    // broadcast the pattern file to each node
    for (int i = 0; i < numPatternLines; i++) {
        MPI_Bcast(patternLines[i], lenPatternLines, MPI_CHAR, 0, MPI_COMM_WORLD);
    }
    cout << rank << ": after bcasting pattern" << endl;

    // convert the pattern file to strings instead of char arrays
 //   string pattern[numPatternLines];
   // for (int i = 0; i < numPatternLines; i++) {
    //    pattern[i] = string(patternLines[i]);
   // }
    // convert the input lines to strings instead of char arrays
  //  string input[numLinesPerNode];
   // for (int i = 0; i < numLinesPerNode; i++) {
    //    input[i] = string(INInputLines[i]);
   // }

    // now use the find method
    int sizeOfMatchArr = 10; // 10 for now... then dynamically increase if needed
    matchLocation* matchArr = new matchLocation[sizeOfMatchArr];
    int numMatches = 0;
    for (int i = 0; i < numLinesPerNode; i++) {
        for (int j = 0; j < numPatternLines; j++) {
            //string iInputLine;
           // string jPatternLine;
           // cout << "in first big loop for node " << rank << " where i,j is " << i << ", " << j << "  - so i: " << input[i] << " and j: " << pattern[j] << endl;
            cout << rank <<": in first big loop for node " << rank << " where i,j is " << i << ", " << j << "  - so i: " << i<< endl;
            int pos = 0;
            //copy(patternLines[j], patternLines[j] + lenPatternLines, jPatternLine.begin());
            string jPatternLine(patternLines[j], patternLines[j] + lenPatternLines);
            cout << rank << ": patternlines " << jPatternLine << " len is " << jPatternLine.length() << endl;

            //copy(INInputLines[i], INInputLines[i] + lenInputLines, iInputLine.begin());
            string iInputLine(INInputLines[i], INInputLines[i] + lenInputLines);
            cout << rank << ": ininputlines " << iInputLine << endl;
      //      int found = string(INInputLines[i]).find(string(patternLines[j]), pos);
            int found = iInputLine.find(jPatternLine, pos);
            while (found != string::npos) {
                cout << rank << ": found at pos " << found << endl;
                if (numMatches >= sizeOfMatchArr) {
                    // increase matchArr size 
                    int biggerSize = sizeOfMatchArr * 2;
                    matchLocation* biggerArr = new matchLocation[biggerSize];
                    memcpy(biggerArr, matchArr, sizeof(matchLocation) * numMatches);
                    delete[] matchArr;
                    matchArr = biggerArr;
                    sizeOfMatchArr = biggerSize;
                }
                // store the match
                int calculatedRealLineNum = i + rank + i * (world_size - 1);
                struct matchLocation m = { calculatedRealLineNum, found, j }; // where i is the row number, found is the col number, and j is the pattern line number
                matchArr[numMatches] = m;
                // update pos, numMatches, and found for the next iteration
                pos = found + 1;
                numMatches++;
                found = iInputLine.find(jPatternLine, pos);
            }
        }
    }
    cout << "after finding all matches on node " << rank << endl;
    // now node 0 needs to collect all the matches to determine which are full matches

    // define MPI data types for sending the matchLocation struct
    MPI_Datatype mpi_matchLocation_type;
    MPI_Type_contiguous(3, MPI_INT, &mpi_matchLocation_type);
    MPI_Type_commit(&mpi_matchLocation_type);
    cout << "after pack prep" << endl;
    if (rank != 0) {
        cout << rank << ": sending to node 0 " << endl;
        // first send the number of matches for this node
        MPI_Send(&numMatches, 1, MPI_INT, 0, 2, MPI_COMM_WORLD); // tag is 2
        cout << rank << ": after send " << endl;
        // pack the struct into a buffer
        int pack_size;
        MPI_Pack_size(numMatches, mpi_matchLocation_type, MPI_COMM_WORLD, &pack_size);
        char* buffer = new char[pack_size];
        int position = 0;
        MPI_Pack(matchArr, numMatches, mpi_matchLocation_type, buffer, pack_size, &position, MPI_COMM_WORLD);
        cout << rank << ": after packing going to send" << endl;
        // at the end of MPI_Pack above, osition was set to be the size that we can use in MPI_Send
        // send the buffer
        MPI_Send(buffer, position, MPI_PACKED, 0, 0, MPI_COMM_WORLD); // tag is 0
        cout << rank <<": after send " << endl;
        delete[] buffer;
    }
    else { // rank == 0
        cout << rank << ": in rank 0 start" << endl;
        // need an array of arrays to store what we receive 
        matchLocation** allMatchLocations = new matchLocation * [world_size];
        int* numMatchesArr = new int[world_size];
        // first set the values for node 0
        numMatchesArr[0] = numMatches;
        allMatchLocations[0] = matchArr;
        // next iterate through other nodes and store the arrays we receive 
        for (int i = 1; i < world_size; i++) { // start at 1 since we are node 0 and do not need to recv anything from ourselves
            cout << rank << ": about to receive nummatches from " << i << endl;
            // first receive the number of matches and store it in the array
            int numMatchesReceived;
            MPI_Recv(&numMatchesReceived, 1, MPI_INT, i, 2, MPI_COMM_WORLD, MPI_STATUSES_IGNORE); // tag is 2
            numMatchesArr[i] = numMatchesReceived;
            cout << rank << ": received num mathces from " << i << endl;
            // preare to receive the packed buffer
            MPI_Status status;
            int count;
            MPI_Probe(i, 0, MPI_COMM_WORLD, &status);
            MPI_Get_count(&status, MPI_PACKED, &count);
            char* buffer = new char[count];
            cout << rank << ": tmggmg" << endl;
            MPI_Recv(buffer, count, MPI_PACKED, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // tag is 0
            cout << rank << ": unpacking ggg " << endl;
            // unpack the buffer into the struct
            int position = 0;
            matchLocation* matchLocations = new matchLocation[numMatchesReceived];
            MPI_Unpack(buffer, count, &position, matchLocations, numMatchesReceived, mpi_matchLocation_type, MPI_COMM_WORLD);
            allMatchLocations[i] = matchLocations;
            delete[] buffer;
        }

        // compare all the matches for lines in a pattern file to find full matches
        int sizeOfCoordArr = 10; // 10 for now... then dynamically increase if needed
        matchCoordinate* coordArr = new matchCoordinate[sizeOfCoordArr];
        int numCoords = 0;
        for (int i = 0; i < world_size; i++) {
            for (int j = 0; j < numMatchesArr[i]; j++) {
                matchCoordinate coor = searchForRealMatches(allMatchLocations[i][j], allMatchLocations, numMatchesArr, numPatternLines, world_size);
                if (coor.x == -1 && coor.y == -1) continue; // not a match
                bool alreadyFound = false;
                for (int k = 0; k < numCoords; k++) {
                    if (coordArr[k].x == coor.x && coordArr[k].y == coor.y) alreadyFound = true;
                }
                if (alreadyFound) continue; // go to next match
                if (numCoords >= sizeOfCoordArr) {
                    // increase coordArr size 
                    int biggerSize = sizeOfCoordArr * 2;
                    matchCoordinate* biggerArr = new matchCoordinate[biggerSize];
                    memcpy(biggerArr, coordArr, sizeof(matchCoordinate) * numCoords);
                    delete[] coordArr;
                    coordArr = biggerArr;
                    sizeOfCoordArr = biggerSize;
                }
                coordArr[numCoords] = coor;
                numCoords++;
                cout << "MATCH AT: " << coor.x << ", " << coor.y << endl;
                // TODO output coordinates to file instead of printing them (col, row) !!!!! THAT IS BACKWARDS FROM WHAT MAKES SENSE
            }
            cout << "fgjeskl" << endl;
        }
        cout << "ffff" << endl;
        // cleanup for just node 0 things
        for (int i = 0; i < world_size; i++) {
            delete[] allMatchLocations[i];
        }
        cout << "h1" << endl;
        delete[] allMatchLocations;
        cout << "h2" << endl;
        cout << "h4" << endl;
        delete[] coordArr;
        cout << "h5" << endl;
        delete[] numMatchesArr;
        cout << "after node 0 stuff" << endl;
    }

    // cleanup for all nodes
    for (int i = 0; i < numPatternLines; i++) {
        delete[] patternLines[i];
    }
    cout << rank << ": h6" << endl;
    delete[] patternLines;
    cout << rank << ": h7" << endl;
    if (rank != 0) { // don't need to do for rank 0 because it was done when we deleted inputLines
        for (int i = 0; i < numLinesPerNode; i++) {
            delete[] INInputLines[i];
        }
        cout << rank << ": h12" << endl;
        delete[] matchArr; // don't need for node 0 bc it's in allmatchlocations
    }
    else {
        for (int i = 0; i < numInputLines; i++) {
            delete[] inputLines[i];
        }
        cout << rank << ": 3" << endl;
        delete[] inputLines;
    }
        cout << rank << ": " << "h8" << endl;
    //delete[] INInputLines; // mem err here

    cout << rank << ": h9" << endl;
    free(lenInputLinesPtr);
    free(lenPatternLinesPtr);
    free(numInputLinesPtr);
    free(numPatternLinesPtr);
    cout <<rank << ": h10" << endl;
    MPI_Type_free(&mpi_matchLocation_type);
    cout <<rank<< ": h11" << endl;
    MPI_Finalize();

    return 0;
}

