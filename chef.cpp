#include <sys/types.h> // For the time_t and size_t type definitions
#include <sys/ipc.h> // For the IPC_CREAT and IPC_PRIVATE mode bits
#include <sys/shm.h> // For shared memory
#include <semaphore.h> // For semaphores
#include <fcntl.h> // For the O_CREAT flag
#include <cstdio> // For the remove() function
#include <cstdlib> // For rand()
#include <iostream> // Who doesn't include this?
#include <string> // For argument parsing and log handling
#include <cstring> // For strcmp() and strcpy()
#include <unistd.h> // For POSIX stuff
#include <sys/wait.h> // For wait()
#include <ctime> // For the "temporal" part of "temporal log" - also for srand() seeding
#include <iomanip> // For put_time() which enables outputting times to a file
#include <fstream> // For file I/O

#include "chef.h"

#define SHMSIZE 128 // Size of the shared memory segment; a little more than what is necessary for all our variables
#define SHMVARNUM 25 // Number of variables I use in shared memory

using namespace std;

void temporalReordering(string fileName) { // Algorithm that sorts the main log file in strict temporal order
	ifstream log;
	log.open(fileName);

	int lineCount = 0;
	string line;
	while(getline(log, line)) { // Get number of lines in the log file
		lineCount++;
	}
	log.close();

	LogLine logArray[lineCount]; // Initialise an array of LogLine structs
	int cnt = 0;
	log.open(fileName); // Open again, this time to copy the file contents into main memory

	while(getline(log, line)) {

		logArray[cnt].hours = stoi(line.substr(0, 2)); // Since each line has the same general structure, we can use actual numbers to slice the line
		logArray[cnt].mins = stoi(line.substr(3, 2));
		logArray[cnt].secs = stoi(line.substr(6, 2));

		if(line.substr(10, 1) == "C") { // This means that the entity of this log line is the chef, which is four letters (and thus has to be treated separately, because all other entities are five letters)
			logArray[cnt].entity = "CHEF";
		}
		else {
			logArray[cnt].entity = line.substr(10, 5);
		}

		size_t contentStart = line.find("]") + 2; // Find the start of the content string (two characters from the closing bracket)
		logArray[cnt].content = line.substr(contentStart);

		cnt++;
	}

	int currentMinHour = 24; // Initialise to impossibly high values to make sure we trigger something in the selection sort
	int currentMinMin = 61; // Ha!
	int currentMinSec = 61;
	int currentMinLoc = -1;

	string fixedArray[lineCount]; // The final, sorted array

	// Behold! A three-factor selection sort that orders the temporal log in ascending order based on time (hours, minutes, seconds) - unfortunately, even this doesn't order the log perfectly, as there are often many entries in the same second which this algorithm can't differentiate between - would need to write a small AI for that capacity :)
	for(int i = 0; i < lineCount; i++) {
		for(int j = 0; j < lineCount; j++) {
			if(logArray[j].hours <= currentMinHour) { // <= instead of < because values can be the same
				if(logArray[j].mins <= currentMinMin) {
					if(logArray[j].secs <= currentMinSec) {
						currentMinHour = logArray[j].hours; // Update current min values
						currentMinMin = logArray[j].mins;
						currentMinSec = logArray[j].secs;
						currentMinLoc = j; // Save the location of the lowest entry so we can take it out of consideration at the end
					}
				}

			}
		}

		fixedArray[i] = to_string(currentMinHour) + ":" + to_string(currentMinMin) + ":" + to_string(currentMinSec) + " [" + logArray[currentMinLoc].entity + "] " + logArray[currentMinLoc].content; // Reconstruct the line and add it to the final string array

		// Reset values, prepare for the next iteration
		logArray[currentMinLoc].hours = 24;
		logArray[currentMinLoc].mins = 61;
		logArray[currentMinLoc].secs = 61;
		currentMinHour = 24;
		currentMinMin = 61;
		currentMinSec = 61;
		currentMinLoc = -1;
	}

	remove("saladlog-ordered.txt");
	ofstream fout;
	fout.open("saladlog-ordered.txt");

	for(int i = 0; i < lineCount; i++) { // Simply fill up the file based on our final array 
		fout << fixedArray[i] << "\n";
	}

	fout.close();

	return;

}

int main(int argc, char* args[]) {

	string saladTotalStr = "";
	string chefTimeStr = "";
	string smTimeStr = "";

	// Default values, in case user omits some params
	int saladTotal = 10;
	int chefTime = 2;
	int smTime = 3;
	bool resetMode = false;
	bool timeFix = false;

	// Processing user input
	for(int i = 0; i < argc; i++) {
		if(strcmp(args[i], "-n") == 0) { // Parameter parsing code adapted from my mvote program, except we have many more params here
			saladTotalStr = args[i+1];
			saladTotal = stoi(saladTotalStr);
		}
		else if(strcmp(args[i], "-m") == 0) {
			chefTimeStr = args[i+1];
			chefTime = stoi(chefTimeStr); // Since the variable was defined outside the loop, this redefinition will apply globally too 
		}
		else if(strcmp(args[i], "-t") == 0) {
			smTimeStr = args[i+1];
			smTime = stoi(smTimeStr);
		}
		else if(strcmp(args[i], "-rm") == 0) {
			resetMode = true;
		}
		else if(strcmp(args[i], "-tf") == 0) {
			timeFix = true;
		}
	}

	cout << "SALADMAKING SETTINGS" << endl;
	if(resetMode == true) {
		cout << "Reset mode: On" << endl;
	}
	else if(timeFix == true) {
		cout << "Number of salads to be made: " << saladTotal << endl << "Chef resting base time: " << chefTime << endl << "Saladmaker working base time: " << smTime << endl << "Log time fix: On" << endl;
	}
	else {
		cout << "Number of salads to be made: " << saladTotal << endl << "Chef resting base time: " << chefTime << endl << "Saladmaker working base time: " << smTime << endl;
	}
	cout << "\n";

	if(resetMode == true) { // A fallback in case the execution is interrupted - activated by the user with the -rm parameter (sadly we can't remove a leftover shared memory segment this way, as that requires knowing the ID, which we can't at this point of the program)
		sem_unlink("/sem0");
		sem_unlink("/sem1");
		sem_unlink("/sem2");
		sem_unlink("/shmSem");
		sem_unlink("/outSem");
		exit(0);
	}

	// Creating one semaphore for each saladmaker
	sem_t *sem0 = sem_open("/sem0", O_CREAT, 0640, 0); // These semaphores all start locked, which ensures that the saladmakers wait until the chef activates each of them
	sem_t *sem1 = sem_open("/sem1", O_CREAT, 0640, 0);
	sem_t *sem2 = sem_open("/sem2", O_CREAT, 0640, 0);

	sem_t *shmSem = sem_open("/shmSem", O_CREAT, 0640, 1); // Special semaphore for accessing the shared memory, only one process can have it at a time - starts open
	sem_t *outSem = sem_open("/outSem", O_CREAT, 0640, 1); // Controls access to output log file, same principle as shmSem

	sem_t* semArray[3] = {sem0, sem1, sem2}; // For ease of access

	int shmid = shmget(IPC_PRIVATE, SHMSIZE, IPC_CREAT | 0640); // Correct permissions (0640 in this case) are super important, shmget() fails otherwise

	if (shmid == -1) {
		cerr << "Could not create shared memory!" << endl;
		return -1;
	}

	int *mem;
	void* tempMem = (int*)shmat(shmid, NULL, 0); // Pointer magic! Casting the shared memory pointer to void, to then cast it back to int resolves some nasty issues (because for some reason, shmat() returns void in C++)
	if (tempMem == reinterpret_cast<void*>(-1)) { // reinterpret_cast() ensures that we retain the same address when converting from void, which is exactly what we're doing
		cerr << "Could not attach to shared memory!" << endl;
		return -1;
	}
	else {
		mem = reinterpret_cast<int*>(tempMem); // Now we have a proper pointer to an array of ints in shared memory  
	}

	for(int i = 0; i < SHMVARNUM; i++) { // Initialise all values in shared memory to 0 (according to the specification, this should be done automatically, but it never hurts to make sure)
		mem[i] = 0;
	}

	remove("saladlog.txt"); // Delete file to clear it out before each run
	ofstream fout;
	fout.open("saladlog.txt", ios::app); // Opening file in append mode

	// Set up logger before we set up the saladmakers
	pid_t pid;
	pid = fork();

	if(pid < 0) { // Error handling
		cerr << "Error: could not start logger child process." << endl;
		return -1;
	} 
	else if(pid == 0) { // In logger
		// Set up parameters to send to execv()
		string loggerName = "logger";
		string segmentIDStr = to_string(shmid);

		char* loggerChar = new char[30];
		char* segmentIDChar = new char[30];
		char* saladTotalChar = new char[30];

		strcpy(loggerChar, loggerName.c_str());
		strcpy(segmentIDChar, segmentIDStr.c_str());
		strcpy(saladTotalChar, saladTotalStr.c_str());

		char* arg[] = {loggerChar, segmentIDChar, saladTotalChar, NULL};
		execv("./logger", arg);	// Start the logger!
	}

	// Forking saladmaker children
	int count = 0;
	for(int childNum = 0; childNum < 3; childNum++) {
		sleep(0.1); // Stagger the kids a little to ensure a smooth start
		pid_t pid;
		pid = fork();

		if(pid < 0) { // Error handling
			cerr << "Error: could not start saladmaker child process." << endl;
			return -1;
		} 
		else if(pid == 0) { // In saladmaker
			// The tried-and-tested int->string->char argument passing procedure from myhie (I do wish there was a more convenient way...)
			string saladmakerName = "saladmaker"; // Could just pass it as a string directly to execv() but the compiler would complain; this is cleaner
			string segmentIDStr = to_string(shmid);
			string childNumStr = to_string(childNum);

			char* saladmakerChar = new char[30]; // As I explained back in myhie, there is no need to explicitly delete these new'd chars, as execv() replaces the entire memory segment occupied by them, getting rid of them automatically
			char* segmentIDChar = new char[30];
			char* childNumChar = new char[30];
			char* saladTotalChar = new char[30];
			char* smTimeChar = new char[30];

			strcpy(segmentIDChar, segmentIDStr.c_str());
			strcpy(saladmakerChar, saladmakerName.c_str());
			strcpy(childNumChar, childNumStr.c_str());
			strcpy(saladTotalChar, saladTotalStr.c_str());
			strcpy(smTimeChar, smTimeStr.c_str());

			char* arg[] = {saladmakerChar, segmentIDChar, childNumChar, saladTotalChar, smTimeChar, NULL};
			execv("./saladmaker", arg);	// Start the saladmaker!
		}
	}

	srand(time(0)); // Seed the random number generator - should only do it once in the entire program, otherwise weird things can happen!
	while(mem[3] < saladTotal) { // Accessing shared memory for just a read is not a race condition, hence we don't need to lock this check down with semaphores

		if(mem[3] == saladTotal) { // To prevent any unneeded passes at the end
			break;
		}		

		// Before each serving of ingredients, the chef rests for a randomly determined time based on user input
		double chefTimeMin = 0.5*double(chefTime); // As specified in the requirements
		double f = (double)rand() / RAND_MAX; // Get a random value between 0 and 1
		double actualChefTime = chefTimeMin + f * (double(chefTime) - chefTimeMin); // Our final number is acquired by taking the minimum time and adding to it the range multiplied by a random number between 0 and 1

		// It's a little convoluted, but this is the only way I found to make a time format into an outputtable string (from the C++ reference)
		sem_wait(outSem);
		time_t t = time(0);
		tm tm = *localtime(&t); // "tm" is a time struct which contains all manners of time data
		fout << put_time(&tm, "%T") << " [CHEF] Resting for " << actualChefTime << "\n"; // %T is the shorthand for the classic hour:minute:second format
		sem_post(outSem);

		cout << "[CHEF] Resting for " << actualChefTime << endl;
		sleep(actualChefTime);

		int choice = rand() % 3; // Simulating the chef picking two ingredients at random (three possible combinations)

		sem_wait(shmSem); // Now we are actually modifying shared memory values, making this a critical section

		if(choice == 0) { // Picking tomatoes and peppers for SM #0
			mem[1] += 1;
			mem[2] += 1;
		}
		else if(choice == 1) { // Picking tomatoes and onions for SM #1
			mem[0] += 1;
			mem[2] += 1;
		}
		else if(choice == 2) { // Picking onions and peppers for SM #2
			mem[0] += 1;
			mem[1] += 1;
		}
		sem_post(shmSem);

		cout << "[CHEF] Picked up ingredients for SM #" << choice << endl;
		sem_wait(outSem);
		t = time(0);
		tm = *localtime(&t);
		fout << put_time(&tm, "%T") << " [CHEF] Picked up ingredients for SM #" << choice << "\n";
		sem_post(outSem);

		while(mem[choice + 7] == 1) { // If the selected SM is busy, wait until it becomes available (busy waiting is a simpler and less error-prone solution here than trying to get into any SM's semaphore)
			sleep(0.1);
		}

		cout << "[CHEF] Telling SM #" << choice << " to take its ingredients" << endl;
		sem_wait(outSem);
		t = time(0);
		tm = *localtime(&t);
		fout << put_time(&tm, "%T") << " [CHEF] Telling SM #" << choice << " to take its ingredients\n";
		sem_post(outSem);

		sem_post(semArray[choice]); // Wake up the SM that we picked the ingredients for, telling it that it can pick them up from the table

		fout.close();
		fout.open("saladlog.txt", ios::app); // This makes it so that the log files are updated on each iteration; it helps with debugging if anyone gets stuck
	}

	// Sometimes, the chef leaves one final round of ingredients on the table which go unused - to avoid food waste, we remove those ingredients (and presumably store them for the next iteration!)
	sem_wait(shmSem);
	mem[0] = 0;
	mem[1] = 0;
	mem[2] = 0;
	sem_post(shmSem);

	sem_wait(outSem);
	time_t t = time(0);
	tm tm = *localtime(&t);
	fout << put_time(&tm, "%T") << " [CHEF] " << saladTotal << " salads finished! Asking saladmakers to help clean up the kitchen...\n";
	sem_post(outSem);

	cout << "[CHEF] " << saladTotal << " salads finished! Asking saladmakers to help clean up the kitchen...\n";	

	for(int i = 0; i < 3; i++) { // Unlock all semaphores so all saladmakers have a chance to gracefully finish
		sem_post(semArray[i]);
	}

	pid_t pidChild;
	int status = 0;
	while ((pidChild = wait(&status)) != -1) {} // Wait for all the children to finish

	// Report final results (only displaying relevant stats by default; uncomment the rest of the lines if you want to see more)
	cout << "\nFINAL RESULTS" << endl;
	//cout << "Number of onions available: " << mem[0] << endl;
	//cout << "Number of peppers available: " << mem[1] << endl;
	//cout << "Number of tomatoes available: " << mem[2] << endl;
	cout << "The final number of salads: " << mem[3] << endl;
	cout << "Salads produced by SM #0: " << mem[4] << endl;
	cout << "Salads produced by SM #1: " << mem[5] << endl;
	cout << "Salads produced by SM #2: " << mem[6] << endl;
	//cout << "SM #0 idle (0) or busy (1): " << mem[7] << endl;
	//cout << "SM #1 idle (0) or busy (1): " << mem[8] << endl;
	//cout << "SM #2 idle (0) or busy (1): " << mem[9] << endl;
	cout << "Onion weight for SM #0: " << mem[10] << endl;
	cout << "Pepper weight for SM #0: " << mem[11] << endl;
	cout << "Tomato weight for SM #0: " << mem[12] << endl;
	cout << "Onion weight for SM #1: " << mem[13] << endl;
	cout << "Pepper weight for SM #1: " << mem[14] << endl;
	cout << "Tomato weight for SM #1: " << mem[15] << endl;
	cout << "Onion weight for SM #2: " << mem[16] << endl;
	cout << "Pepper weight for SM #2: " << mem[17] << endl;
	cout << "Tomato weight for SM #2: " << mem[18] << endl;
	cout << "Total work time for SM #0: " << mem[19] << endl;
	cout << "Total wait time for SM #0: " << mem[20] << endl;
	cout << "Total work time for SM #1: " << mem[21] << endl;
	cout << "Total wait time for SM #1: " << mem[22] << endl;
	cout << "Total work time for SM #2: " << mem[23] << endl;
	cout << "Total wait time for SM #2: " << mem[24] << endl;
	cout << "\n";

	fout.close();

	// Display the work of the concurrent logger by reading from the log it created
	ifstream fin;
	fin.open("concurrentlog.txt");
	if(!fin) { // Error check, though it's not the end of the world - if the logger messed up somehow, just don't bother with the file
		cerr << "Error: concurrent log file could not be opened." << endl;
	}
	else {
		string line;

		cout << "Concurrent periods of execution" << endl;
		while(getline(fin, line)) {
			cout << line << endl;
		}
		cout << "\n";

		fin.close();
	}

	if(timeFix == true) { // If the user requested the log timings to be "fixed", run the algorithm
		temporalReordering("saladlog.txt");
	}

	// Clean up all semaphores
	sem_close(sem0);
	sem_unlink("/sem0");
	sem_close(sem1);
	sem_unlink("/sem1");
	sem_close(sem2);
	sem_unlink("/sem2");
	sem_close(shmSem);
	sem_unlink("/shmSem");
	sem_close(outSem);
	sem_unlink("/outSem");

	// Clean up shared memory
	int err = shmdt(mem); // Detach from the segment
	if (err == -1) {
		cerr << "Error detaching from shared memory!" << endl;
	}
	else {
		cout << "Chef detachment successful, ID " << err << endl;
	}

	err = shmctl(shmid, IPC_RMID, NULL); // Remove the segment
	if (err == -1) {
		cerr << "Error removing shared memory!" << endl;
	}
	else {
		cout << "Shared memory segment removed, ID " << err << endl;
	}

	return 0;
}