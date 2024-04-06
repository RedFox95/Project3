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
fileinfo getFileInfo(string filename) {
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
matchCoordinate searchForRealMatches(matchLocation match, matchLocation ** allMatches, int * numMatchesArr, fileinfo patternInfo, size_t world_size) {
    cout << "-> searchForRealMAtches x:" << match.x << " y: " << match.y << " pl: " << match.pl << endl;
    matchLocation ** patternMatchLocations = new matchLocation*[patternInfo.numLines];
    for (int i = 0; i < patternInfo.numLines; i++) patternMatchLocations[i] = nullptr;

    patternMatchLocations[match.pl] = &match;
    for (int i = 0; i < world_size; i++) {
        for (int j = 0; j < numMatchesArr[i]; j++) {
            bool fullMatch = true;
            // for each match...
            if (allMatches[i][j].y == match.y && allMatches[i][j].x != match.x && allMatches[i][j].pl != match.pl) {
                cout << "col matches for y val " << match.y << " for i " << i << endl;
                // if it's in the correct column
                for (int k = 0; k < patternInfo.numLines; k++) {
                    if (allMatches[i][j].x == match.x+k && allMatches[i][j].pl == match.pl+k) {
                        cout << "match! x " << allMatches[i][j].x << " pl " << allMatches[i][j].pl << endl;
                        // this is a corresponding match!
                        patternMatchLocations[allMatches[i][j].pl] = &allMatches[i][j]; // do i need to use new here?
                    }
                    // check if full match 
                    if (patternMatchLocations[k] == nullptr) fullMatch = false;
                }
                if (fullMatch) {
                    cout << " FULL MATCH FOUND!" << endl;
                    for (int t = 0; t< patternInfo.numLines; t++) {
                        cout << t  << "of " << patternInfo.numLines<< endl;
                        // cout << patternMatchLocations[t]->x << ", "<< patternMatchLocations[t]->y <<", "<< patternMatchLocations[t]->pl << endl;
                    }
                    struct matchCoordinate retVal = {patternMatchLocations[0]->x, patternMatchLocations[0]->y};
                    return retVal;
                }                
            }
        }
    }
    // return -1, -1 if no match found 
    struct matchCoordinate retVal = {-1, -1};
    return retVal;    
}

int main(int argc, char ** argv) {
    // cout << "STARTING PROGRAM node " << endl;
    /* necessary setup for MPI */
    MPI_Init(NULL,NULL);
    int world_size; // how many total nodes we have
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    int rank; // rank is the node number
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    // cout << "setup done on node " << rank << endl;

    /* vars for storing things that are used in MPI method calls */
    char ** inputLines = nullptr;
    char ** patternLines = nullptr;
    int numLinesPerNode = -1;    
    char ** INInputLines = nullptr; // where IN means individual node (the lines for each node to analyze)
    // char ** INPatternLines = nullptr; // where IN means individual node // not needed for bcast
    size_t numPatternLines;
    size_t numInputLines;
    size_t lenPatternLines;
    size_t lenInputLines;
    // cout << "here1 node" << rank << endl;

    /* one node reads input and pattern files */
    fileinfo inputInfo;
    fileinfo patternInfo;
    if (rank == 0) {
        // cout << "node " << rank << " reading in files" << endl;

        /* get the number of lines in the input file */
        string inputFile = argv[1];
        inputInfo = getFileInfo(inputFile);
        numInputLines = inputInfo.numLines;
        lenInputLines = inputInfo.lineLength;
        // cout << " on node " << rank << " determined numInputLines to be " << numInputLines << endl;

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
        numPatternLines = patternInfo.numLines;
        lenPatternLines = patternInfo.lineLength;
        // cout << " on node " << rank << " determined numPatternLines to be " << numPatternLines << endl;

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
        numLinesPerNode = (inputInfo.numLines / world_size); // TODO account for when not clean division
    }
    // cout << "after file reading section on node " << rank << endl;
    // ensure all nodes know numLinesPerNode value
    MPI_Bcast(&numLinesPerNode, 1, MPI_INT, 0, MPI_COMM_WORLD);
    cout << "after bcast for numlines per node on node " << rank << " numlinespernode is " << numLinesPerNode<< endl; // DO NOT DELETE

    MPI_Bcast(&lenPatternLines, 1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);    
    MPI_Bcast(&lenInputLines, 1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);
    // cout << "after bcast of length of file lines on node " << rank << " input len is " << lenInputLines << " pattern len is " << lenPatternLines << endl;
    /* scatter lines of input file */
    // inputLines is a double pointer (array of array of chars)
    // want to send each node a set of 1 lines (maybe change this later?)
    // MPI_CHAR - char arrays
    // INInputLines - the buffer to place the lines for each node 
    // receive 1 ? 
    // 0 tag for the scatter
    INInputLines = new char*[numLinesPerNode];
    for (int i = 0; i < numLinesPerNode; i++) {
        INInputLines[i] = new char[lenInputLines];
    }
    // MPI_Scatter(inputLines, numLinesPerNode, MPI_CHAR, INInputLines, numLinesPerNode, MPI_CHAR, 0, MPI_COMM_WORLD);
    // size_t 
    if (rank == 0) {
        size_t numPlacedInSelf = 0;
        for (int i = 0; i < numInputLines; i++) {
            // send input lines to alternating nodes
            int destNode = i % world_size;
            // cout << "sending to " << destNode << endl;
            if (destNode != 0) {
                MPI_Send(inputLines[i], lenInputLines, MPI_CHAR, destNode, 6, MPI_COMM_WORLD);
            } else {
                INInputLines[numPlacedInSelf] = inputLines[i];
                numPlacedInSelf++;
            }
        }
    } else {
        for (int i = 0; i < numLinesPerNode; i ++) { // TODO figure out these numbers
            // cout << rank << ": waiting to receive input line: " << endl; 

            MPI_Recv(INInputLines[i], lenInputLines, MPI_CHAR, 0, 6, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            // cout << "TMGGNRISKFD " << endl;
            // cout << rank << ": received input line: " << *INInputLines[i] << endl; 
        }
    }


    // cout << "after fake scatter lines on node " << rank << endl;

    // cout << "TMGGGG node " << rank << " first INInputLine: " << *INInputLines[0] << endl;

    // need to bcast number of lines and length of line for both files
    MPI_Bcast(&numPatternLines, 1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);    
    MPI_Bcast(&numInputLines, 1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);
    // cout << "after bcast of number of file lines on node " << rank << " input lines is " << numInputLines << " pattern lines is " << numPatternLines << endl;

    /* broadcast entire pattern file */
    // broadcast the patternLines (so this var is the same across nodes)
    // count - the number of lines i think
    // MPI_CHAR i think - but it's char array?
    // root node = 0
    // MPI_Bcast(patternLines, numPatternLines, MPI_CHAR, 0, MPI_COMM_WORLD);
    if (rank != 0) {
        patternLines = new char*[numPatternLines];
        // cout << rank << ": created new arr for patternLines" << endl;
        for (int i = 0; i < numPatternLines; i++) {
            patternLines[i] = new char[lenPatternLines];
        }
    }
    for (int i = 0; i < numPatternLines; i++) {
        // cout << rank << ": in loop... patternLines[i] is " << patternLines[i] << endl;
        MPI_Bcast(patternLines[i], lenPatternLines, MPI_CHAR, 0, MPI_COMM_WORLD); // for some reason whatever is broadcasted first is wrong.. 
        // cout << "on node " << rank << " patternLine[" << i << "] is: " << *patternLines[i] << endl;
    }
    // cout << "after bcast of pattern lines on node " << rank << " patternlines is " << patternLines << endl;


    /* for each line in the input file, compare to the pattern file */
    // first convert to strings to make this easier
    string pattern[numPatternLines];
    for (int i = 0; i < numPatternLines; i++) {
        // cout << i <<"in converting char* to strings for pattern node " << rank << endl;
        pattern[i] = string(patternLines[i]);
        // cout << rank << ": pattern is " << pattern[i] << endl;
    }
    // cout << " after pattern converting to strings on node " << rank << endl;
    string input[numLinesPerNode];
    for (int i = 0; i < numLinesPerNode; i++) {
        input[i] = string(INInputLines[i]);
        // cout << rank << ": creating str from char* " << string(INInputLines[i]) << endl;
    }
    // cout << " after converting to strings on node " << rank << endl;
    // now use the find method
    size_t sizeOfMatchArr = 10; // 10 for now... then dynamically increase if needed
    matchLocation * matchArr = new matchLocation[sizeOfMatchArr]; 
    size_t numMatches = 0;
    for (int i = 0; i < numLinesPerNode; i++) { 
        for (int j = 0; j < numPatternLines; j++) {
            cout << "in first big loop for node " << rank << " where i,j is " << i << ", " << j << "  - so i: " << input[i] << " and j: " << pattern[j] << endl;
            size_t pos = 0;
            size_t found = input[i].find(pattern[j], pos);
            while (found != string::npos) {
                cout << rank << ": found at pos " << found << endl;
                if (numMatches >= sizeOfMatchArr) {
                    // increase matchArr size 
                    size_t biggerSize = sizeOfMatchArr * 2;
                    matchLocation * biggerArr = new matchLocation[biggerSize];
                    memcpy(biggerArr,matchArr,sizeof(matchLocation)*numMatches);
                    delete[] matchArr;
                    matchArr = biggerArr;
                }
                // store the match
                struct matchLocation m = {i, found, j}; // where i is the row number (TODO this is currently wrong - is the line num for this node, not the entire file), found is the col number, and j is the pattern line number
                cout << rank << ": newly created match x,y,pl: " << i << "," << found << "," << j << endl;
                matchArr[numMatches] =  m;
                // update pos
                pos = found + 1; // or found + 1?
                // update numMatches
                numMatches++;
                // update found
                found = input[i].find(pattern[j], pos);
            }
        }
    }
    cout << "after finding all matches on node " << rank << endl;
    /* bring all the matches to node 0 for comparison */

    // Define MPI data types
    MPI_Datatype mpi_matchLocation_type;
    MPI_Type_contiguous(3, MPI_UNSIGNED_LONG, &mpi_matchLocation_type);
    MPI_Type_commit(&mpi_matchLocation_type);
    if (rank != 0) {
        cout << rank << ": send num of matches which is " << numMatches << endl;
        // first send the number of matches for this node (tag is 2)
        MPI_Send(&numMatches, 1, MPI_UNSIGNED_LONG, 0, 2, MPI_COMM_WORLD); // SEND NUM OF MATCHES tag is 2, sending to node 0, unisgned long, 1 item
        cout << rank << ": packing up struct..." << endl;

        // Pack the struct into a buffer
        int pack_size;
        MPI_Pack_size(numMatches, mpi_matchLocation_type, MPI_COMM_WORLD, &pack_size);
        char* buffer = new char[pack_size];
        int position = 0;
        MPI_Pack(matchArr, numMatches, mpi_matchLocation_type, buffer, pack_size, &position, MPI_COMM_WORLD);
        cout << rank << ": done packing. now sending..." << endl;
        // at the end of MPI_Pack above, position was set to be the size that we can use in MPI_Send
        // destination is node 0
        // tag for this message is 1
        MPI_Send(buffer, position, MPI_PACKED, 0, 0, MPI_COMM_WORLD);
    } else { // rank == 0
        cout << rank << ": getting ready to receive match locations" << endl;
        // need an array of arrays to receive 
        matchLocation ** allMatchLocations = new matchLocation*[world_size];
        int * numMatchesArr = new int[world_size];
        // first set the values for node 0
        numMatchesArr[0] = numMatches;
        allMatchLocations[0] = matchArr;
        // next iterate through other nodes and store the arrays on recvs 
        for (int i = 1; i < world_size; i++) { // start at 1 since we are node 0 and do not need to recv anything from ourselves
            // cout << "world size:" << world_size << endl;
            cout << rank << ": attempting to receive number of matches from node " << i << endl;
            // first receive the number of matches 
            size_t numMatchesReceived;
            // tag for receiving number of matches is 2 (idk if this has to match the send?) -> yes i think it does have to match 
            MPI_Recv(&numMatchesReceived, 1, MPI_UNSIGNED_LONG, i, 2, MPI_COMM_WORLD, MPI_STATUSES_IGNORE); // RECEIVE NUM OF MATCHES tag is 2, receiving from node i (1)
            cout << rank << ": received nummatches: " << numMatchesReceived << " from node " << i << endl;

            numMatchesArr[i] = numMatchesReceived;
            cout << rank << ": prepping to receive struct from node " << i << endl;

            // Receive the packed buffer
            MPI_Status status;
            int count;
            // 1 is the source, also the tag is 0
            MPI_Probe(1, 0, MPI_COMM_WORLD, &status);
            MPI_Get_count(&status, MPI_PACKED, &count); // i think we want to do this for each arr form each node
            char* buffer = new char[count];
            cout << rank << ": attempting to receive struct from node " << i << "..." << endl;

            MPI_Recv(buffer, count, MPI_PACKED, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // RECEIVING STRUCT count is from get count... 
            cout << rank << ": unpacking buffer into struct" << endl;
            // Unpack the buffer into the struct
            int position = 0;
            matchLocation * matchLocations = new matchLocation[numMatchesReceived];
            MPI_Unpack(buffer, count, &position, matchLocations, numMatchesReceived, mpi_matchLocation_type, MPI_COMM_WORLD); // error here?
            allMatchLocations[i] = matchLocations;
            delete[] buffer;
        }

        /* node 0 compares coordinates to find the full matches */
        size_t sizeOfCoordArr = 10; // 10 for now... then dynamically increase if needed
        matchCoordinate * coordArr = new matchCoordinate[sizeOfCoordArr]; 
        size_t numCoords = 0;        
        for (int i = 0; i < world_size; i++) {
            for (int j = 0; j < numMatchesArr[i]; j++) {
                matchCoordinate coor = searchForRealMatches(allMatchLocations[i][j], allMatchLocations, numMatchesArr, patternInfo, world_size);
                if (coor.x == -1 && coor.y == -1) continue; // not a match
                bool alreadyFound = false;
                for (int k = 0; k < numCoords; k++) {
                    if (coordArr[k].x == coor.x && coordArr[k].y == coor.y) alreadyFound = true;
                }
                if (alreadyFound) continue; // go to next match
                if (numCoords >= sizeOfCoordArr) {
                    // increase coordArr size 
                    size_t biggerSize = sizeOfCoordArr * 2;
                    matchCoordinate * biggerArr = new matchCoordinate[biggerSize];
                    memcpy(biggerArr,coordArr,sizeof(matchCoordinate)*numCoords);
                    delete[] coordArr;
                    coordArr = biggerArr;
                }                
                coordArr[numCoords] = coor;
                numCoords++;
                cout << "MATCH AT: " << coor.x << ", " << coor.y << endl;
                /* write to output file */
                // TODO output coordinates to file instead of printing them (col, row) !!!!! THAT IS BACKWARDS FROM WHAT MAKES SENSE
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