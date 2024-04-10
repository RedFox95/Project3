#include <chrono> #include <iostream> #include <fstream> #include <istream> #include "mpi.h"using namespace std;const int MAX_COORD_LENGTH = 2048; // Adjust as necessarystring* obtainLines(int startRow, int endRow, int worldRank, string& fileName, string* processLines) { ifstream file(fileName); // Skip lines until reaching the start row for this process string line; for (int i = 0; i < startRow && getline(file, line); ++i) {}// Read and process the assigned rows
int currentRow = startRow;
int index = 0;
while (currentRow <= endRow && getline(file, line)) {
	// Process the line here
	// cout << "Process " << worldRank << " reading row " << currentRow << endl;
	processLines[index] = line;
	index++;
	currentRow++;
}
return processLines;
} void getNumColumnRow(string& fileName, unsigned long& numColumns, unsigned long& numRows) { ifstream inputFile(fileName); //Make sure file is valid if (!inputFile) { cerr << "Error opening input file." << endl; }string line;
//determine num of columns for input file
if (getline(inputFile, line)) {
	numColumns = line.length();
	numRows++;
}
//determine num of rows for input file
while (getline(inputFile, line)) {
	++numRows;
}
inputFile.close();
}void processPattern(string& patternName, unsigned long& numColumns, unsigned long& numRows, string* patternMatch) { ifstream inputFile(patternName); if (!inputFile) { cerr << "Error opening pattern file." << endl; } string line; numRows = 0; // Initialize numRows to 0// Read lines from the file and assign each to an index of patternMatch
while (getline(inputFile, line)) {
	if (numRows == 0) {
		// Determine the number of columns from the first line
		numColumns = line.length();
	}
	if (numRows < 10) { // Ensure we don't exceed the array bounds
		patternMatch[numRows] = line;
	}
	else {
		cout << "Warning: Pattern file has more than 10 lines; additional lines will be ignored." << endl;
		break; // Exit the loop if there are more than 10 lines
	}
	numRows++;
}
} int globalCount = 0; void requestAdditionalLines(int neededLines, int worldRank, int worldSize, int numColumns, string* receivedLines) { globalCount++; //cout << "[" << worldRank << "] Requesting additional lines..." << endl; if (worldRank + 1 >= worldSize) { cout << "[" << worldRank << "] No next process to request lines from. Exiting." << endl; // No next process to request lines from return; }int nextProcess = worldRank + 1;

// cout << "[" << worldRank << "] Sending request for " << neededLines << " lines to process " << nextProcess << " (" << globalCount << ")" << endl;

 // Non-blocking send to request lines
MPI_Ssend(&neededLines, 1, MPI_INT, nextProcess, 7, MPI_COMM_WORLD);
// cout << "[" << worldRank << "] RECEIVED request " << endl;
 // Allocate and initialize buffer for receiving lines
const int maxLineSize = numColumns + 1;
//cout << "[" << worldRank <<  "] NUM COLUMNS~~ " << numColumns << endl;
char* buffer = new char[neededLines * maxLineSize];
memset(buffer, 0, size_t(neededLines) * maxLineSize - 1);
// cout << "[" << worldRank << "] WAITING to receive!" <<  " (" << globalCount << ")" << endl;
// cout << "RECV message size: " << neededLines * maxLineSize + 1 << endl;
MPI_Recv(buffer, neededLines * maxLineSize, MPI_CHAR, worldRank + 1, 10, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
// cout << "[" << worldRank << "] RECEIVED more Lines!" << " (" << globalCount << ")" << endl;
 // Convert buffer to strings and assign to receivedLines
int bufferIndex = 0;
for (int i = 0; i < neededLines; i++) {
	receivedLines[i].assign(&buffer[bufferIndex]);
	bufferIndex += receivedLines[i].length() + 1; // Move past the current string and '\0'
}

// Cleanup
delete[] buffer;
}void addCoords(string& outputCoords) { // Output to file ofstream outFile("output.txt"); outFile << outputCoords << endl; outFile.close(); }void convertAndGatherMasterCoords(const string& processCoords, int worldRank, int worldSize) { // Convert masterCoords to a char array char localCoords[MAX_COORD_LENGTH]; memset(localCoords, 0, MAX_COORD_LENGTH); // Initialize with zeros strncpy_s(localCoords, MAX_COORD_LENGTH, processCoords.c_str(), _TRUNCATE); localCoords[MAX_COORD_LENGTH - 1] = '\0';// Allocate memory for gathering all coords at process 0
char* allCoords = nullptr;
if (worldRank == 0) {
	allCoords = new char[MAX_COORD_LENGTH * worldSize];
	cout << "all coords created!" << endl;
}
// Gather all localCoords to process 0
//  cout << "[" << worldRank <<  "] gathering!" << endl; MPI_Gather(localCoords, MAX_COORD_LENGTH, MPI_CHAR, allCoords, MAX_COORD_LENGTH, MPI_CHAR, 0, MPI_COMM_WORLD); // cout << "[" << worldRank << "] GATHERED!" << endl; // Process 0 combines all gathered coords if (worldRank == 0) { string combinedCoords; for (int i = 0; i < worldSize; ++i) { combinedCoords += string(allCoords + i * MAX_COORD_LENGTH); } //  cout << "COMBINED ALL COORDS!" << endl; addCoords(combinedCoords); // Assuming addCoords can take a std::string cout << "AlL THE COORDS: " << combinedCoords << endl;	delete[] allCoords;
}
} void findMatchInAddedRows(string* lines, int end, string* patternMatch, unsigned long rowLength, int** foundPatterns, int columnLength, int origRowLeng, int numFound, int worldRank, int numMatches, int inputRowLeng) { int index = numFound - 1; cout << "[" << worldRank << "] Num found: " << numFound << endl; while (index < numMatches) { if (foundPatterns[index][0] > 0) { bool patternComplete = false; bool patternFound = true; // Reset for each new starting position //  cout << "[" << worldRank << "] startRowIndex: " << foundPatterns[index][0] << endl; int startRow = (inputRowLeng - foundPatterns[index][0]) + 1; int startCol = foundPatterns[index][1] - 1; //  cout << "[" << worldRank << "] START ROW: " << startRow << " START COL: " << startCol << endl; if (worldRank == 1) { //cout << "[" << worldRank << "] CHECKING: " << foundPatterns[index][0] << "," << foundPatterns[index][1] << endl; } int currentRow = 0; // Adjust row index based on the subset of lines		//for (int j = startCol; j < startCol * 2; j++) {
		for (size_t pi = startRow; pi < rowLength - 1 && patternFound && !patternComplete; pi++) {
			for (size_t pj = 0; pj < columnLength && patternFound && !patternComplete; pj++) {
				if (currentRow < end) {
				//	cout << "[" << worldRank << "] CurrRow: " << currentRow << endl;
					//cout << "[" << worldRank << "] InputRow: " << string(lines[currentRow]) << " PatternRow: " << patternMatch[pi] << endl;
					////cout << "[" << worldRank << "] COMPARING: " << lines[currentRow][startCol + pj]
					//	<< " to " << patternMatch[pi][pj] << " on LINE: " << currentRow << endl;
					if (lines[currentRow][startCol + pj] == patternMatch[pi][pj]) {

						if (pi == rowLength - 1 && pj == columnLength - 1) {
							patternComplete = true;
						//	cout << "[" << worldRank << "] FINISHED PATTERN ON: " << foundPatterns[index][0] << "," << foundPatterns[index][1] << endl;
							// Logic to record the found pattern coordinates
						}
						//cout << "[" << worldRank << "] MATCH PI:" << pi << " PJ: " << pj << " ROW LENG: " << rowLength << endl;

					}
					else {
						//cout << "[" << worldRank << "] NO MATCH... Changing " << foundPatterns[index][0] << "," << foundPatterns[index][1] << " to - 1!" << endl;

						foundPatterns[index][0] = -1;
						patternFound = false;
					}
				}
				else {
					patternFound = false;
				}
			}
			currentRow++;
		}
		index++;
		if (!patternComplete) {
			foundPatterns[index][0] = -1;
		}
		// }
	}
	else {
		index++;
	}
}
} bool searchPatternAndRequest(string* lines, int& start, int end, int numColumns, string* patternMatch, unsigned long rowLength, int maxMatches, int** foundPatterns, int& numFound, bool& patternFound, bool& patternComplete, int worldRank, bool& rowNotRequested, int worldSize, int& numRowsNeeded, int& numMatches) { int index = -1; // Initialize the count of found patterns    bool patternFound = true; patternComplete = false; size_t columnLength = patternMatch[0].length(); int topRow, topCol = 0; int lastRow = 0; int receivedLines; bool firstNotComplete = true; bool needsMoreLines = false; bool newPattern = false;int counter = 0;
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
							//    cout << "[" << worldRank << "]  index -- " << index << endl;

								// Logic to record the found pattern coordinates
						}
					}
					else {
						patternFound = false;
					}

				}
				else {
					if (worldRank != worldSize - 1 && patternFound && !patternComplete)
					{
						int temp = (rowLength - (currentRow - end)) - 1;
						//  cout << "[" << worldRank << "] 0.1" << endl;
					   //   cout << "row leng: " << rowLength << " end: " << end << endl;

						needsMoreLines = true;
						// cout << "INDEX: " << index << " MAX MATCHES:" << maxMatches <<  endl;
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
							//  cout << "[" << worldRank << "]  index -- " << index << endl;

							newPattern = false;
							//  cout << "[" << worldRank << "] requesting.. " << numRowsNeeded << " rows!" << endl;
							  // Additional logic...
						}
						else if (index >= maxMatches) {
							// Handle the case when index is out of bounds - maybe resize the array or skip adding.
							cout << "[" << worldRank << "] Error: Exceeded maxMatches. Not adding more patterns." << endl;
						}

						if (firstNotComplete) {
							numFound = index;
							//      cout << "[" << worldRank << "] NUM FOUND SET: " << numFound << " COORD: " << foundPatterns[index][0] << endl;
							firstNotComplete = false;
						}
					}
					else {
						patternFound = false;
					}
				}
			}
			lastRow = pi;
			if (worldRank != 0 && rowNotRequested) {
				counter++;

				// cout << "[" << worldRank << " ] RECV PEND " << counter << " ====" << endl;
				MPI_Recv(&needLines, 1, MPI_INT, worldRank - 1, 18, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				//  cout << "PROCESS " << worldRank << " RECV STATUS (18) : " << statuss <<  endl;
			   // cout << "[" << worldRank << " ] RECEIVED: " << needLines << " from [" << worldRank - 1 << "] ======" << endl;
				rowNotRequested = false;
				//  cout << "[" << worldRank - 1 << "] sent request: " << needLines << endl;
				if (needLines == 1) {
					//  cout << "[" << worldRank - 1 << "] NEEDS LINES. Preparing to send request. " << endl;

					MPI_Recv(&receivedLines, 1, MPI_INT, worldRank - 1, 7, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
					//   cout << "[" << worldRank << "] received request for... " << receivedLines << " LINES! (" << counter << ")" << endl;

					int totalSize = 0;
					for (int index = 0; index < receivedLines; index++) {
						totalSize += lines[index].length() + 1; // +1 for '\n' or '\0'
					}
					//  cout << "Total Size: " << totalSize << endl;

					  // Allocate buffer size with extra space for newlines and null terminator
					char* sendLines = new char[totalSize + 1];
					sendLines[0] = '\0';  // Initialize the buffer

					int currentPosition = 0;
					for (int index = 0; index < receivedLines; index++) {
						// Calculate the remaining size of the buffer
						size_t remainingSize = totalSize - currentPosition + 1; // +1 for final null terminator

						// Use remainingSize for the size parameter in strcpy_s
						strcpy_s(sendLines + currentPosition, remainingSize, lines[index].c_str());
						currentPosition += lines[index].length();
						sendLines[currentPosition] = '\n'; // Add newline as separator
						currentPosition++; // Move past the newline
					}
					sendLines[currentPosition] = '\0'; // Null-terminate the last line if required

					//   cout << "[" << worldRank << "] SENDING Lines. (" << counter << ")" << endl;
					int messageSize = currentPosition;
					//    cout << "Message size: " << messageSize << endl;
					MPI_Ssend(sendLines, messageSize, MPI_CHAR, worldRank - 1, 10, MPI_COMM_WORLD);
					//    cout << "[" << worldRank << "] SENT Lines: " << sendLines << endl;
					 //   cout << "[" << worldRank << "] SENDING Lines COMPLETE." << endl;


					delete[] sendLines;

					needLines = 0;
				}
			}
		}

	}
	if (i == end - 1 && patternComplete && patternMatch[0] == patternMatch[end - 1]) {
		cout << "[" << worldRank << "] 0.2" << endl;
		index++;
		needsMoreLines = true;
		foundPatterns[index][0] = end - 1; // Adjusting indexing as necessary
		foundPatterns[index][1] = topCol;
		numMatches++;
		cout << "[" << worldRank << "]  index -- " << index << endl;

		if (firstNotComplete) {
			numFound = index;
			firstNotComplete = false;
		}
	}

}
if (needsMoreLines) {
	cout << "[" << worldRank << "] 0" << endl;
	return true;
}

else {
	cout << "[" << worldRank << "] 3" << endl;
	return false;
}
}
void acquireMasterCoords(int** coordArray, string& masterCoords, int startRow, int worldRank, int maxMatches) { if (!coordArray) { cout << "[" << worldRank << "] Error: coordArray is NULL." << endl; return; // Early return if coordArray is NULL }for (int index = 0; index < maxMatches; ++index) {
	if (!coordArray[index]) {
		cout << "[" << worldRank << "] Error: coordArray entry is NULL at index " << index << endl;
		break; // Break if a NULL entry is encountered
	}

	// Use the sentinel value to terminate the loop safely
	if (coordArray[index][0] > 0) {
		masterCoords += to_string(startRow + coordArray[index][0]) + "," + to_string(coordArray[index][1]) + "\n";
	}
}
}/** *@param numThreads@param fileName@param mainTableSteps:assign each mpi a certain amount of linesparse each row to find a pattern for that row, if match, index same spot in next rowif next row isnt within bounds of mpi rank, pass index to next inline (index being column num and row++)if match, add to output list*/void dispatchMPI(string& fileName, int worldRank, int worldSize, int startRow, int endRow, int numColumns, string* patternMatch, unsigned long rowLength) {int counter = 0;
int origRowLeng = endRow - startRow;
string masterCoords = "";

string* processLines = new string[origRowLeng + 1];
processLines = obtainLines(startRow, endRow, worldRank, fileName, processLines);
//cout << "Start row: " << startRow << " End Row: " << endRow << endl;
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
/////////
//cout << "Block columns: " << numColumns << " Pattern columns: " << columnLength << " Blocl rows: " << origRowLeng << " Pattern rows: " << rowLength << endl;
int maxMatches = (numColumns - (numColumns - columnLength)) * (1 + (origRowLeng - rowLength)); // Example maximum
int** foundPatterns = new int* [maxMatches];
for (int i = 0; i < maxMatches; ++i) {
	foundPatterns[i] = new int[2]; // Each sub-array holds two integers for row and column
	foundPatterns[i][0] = -1;
	foundPatterns[i][1] = -1;
}
int numFound = -1;
// cout << "[" << worldRank << "] CALLING SEARCH" << endl;
 // Call the function
needAdditionalRows = searchPatternAndRequest(processLines, start, origRowLeng, numColumns, patternMatch,
	rowLength, maxMatches, foundPatterns, numFound, patternFound, patternComplete, worldRank, rowNotRequested, worldSize, numRowsNeeded, numMatches);

rowNotRequested = true;
//cout << "finished search!" << endl;
if (worldRank != worldSize - 1) {
	if (needAdditionalRows) {

		moreLinesNeeded = 1;
		useArray = true;

		MPI_Ssend(&moreLinesNeeded, 1, MPI_INT, worldRank + 1, 18, MPI_COMM_WORLD);
		// cout << "[" << worldRank << "]" << " requested MORE LINES " << endl;
		rowNotRequested = false;

		string* additionalLines = new string[numRowsNeeded]; // Corrected variable name
		requestAdditionalLines(numRowsNeeded, worldRank, worldSize, numColumns, additionalLines);
		findMatchInAddedRows(additionalLines, numRowsNeeded, patternMatch, rowLength,
			foundPatterns, columnLength, origRowLeng, numFound, worldRank, numMatches, origRowLeng);


	}
	else {
		moreLinesNeeded = 0;
		//  cout << "[" << worldRank << "] DOESNT need more lines!" << endl;
		MPI_Ssend(&moreLinesNeeded, 1, MPI_INT, worldRank + 1, 18, MPI_COMM_WORLD);
		// cout << "[" << worldRank << "] DOESNT need more lines CONFIRMED!" << endl;

		rowNotRequested = false;
	}
}

// cout << "[" << worldRank << "] Compiling master coords " << endl;
acquireMasterCoords(foundPatterns, masterCoords, startRow, worldRank, numMatches);
// cout << "[" << worldRank << "] Finished processing. Adding coords if applicable." << endl;
 // Remember to deallocate after use
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