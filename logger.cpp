#include <sys/types.h> // For the time_t and size_t type definitions
#include <sys/ipc.h> // For the IPC_CREAT and IPC_PRIVATE mode bits
#include <sys/shm.h> // For shared memory
#include <cstdio> // For the remove() function
#include <iostream> // Who doesn't include this?
#include <string> // For argument parsing and log handling
#include <cstring> // For strcmp()
#include <unistd.h> // For POSIX stuff
#include <ctime> // For the "temporal" part of "temporal log"
#include <iomanip> // For put_time() which enables outputting times to a file
#include <fstream> // For file I/O
#include <chrono> // For highly accurate, C++ timekeeping

#include "logger.h"

using namespace std;

int main (int argc, char* args[]) {

	// Getting parameter from chef
	string shmidStr = args[1];
	int shmid = stoi(shmidStr);
	string saladTotalStr = args[2];
	int saladTotal = stoi(saladTotalStr);

	bool timeFix = false;
	if(strcmp(args[3], "true") == 0) {
		timeFix = true;
	}

	int *mem;
	void* tempMem = (int*)shmat(shmid, NULL, 0); // Pointer magic! Casting the shared memory pointer to void, to then cast it back to int resolves some nasty issues
	if (tempMem == reinterpret_cast<void*>(-1)) { // reinterpret_cast() ensures that we retain the same address when converting from void and back, which is exactly what we're doing
		cerr << "Could not attach to shared memory!" << endl;
		return -1;
	}
	else {
		mem = reinterpret_cast<int*>(tempMem); // Now we have a proper int shared memory pointer 
	}

	cout << "Logger activated!" << endl;

	string outFile = "concurrentlog.txt";
	remove(outFile.c_str());
	ofstream fout;
	fout.open(outFile, ios::app); // Opening file in append mode

	bool timerGoing = false;
	time_t t = time(0);
	tm tmStart = *localtime(&t); // Set up initial values outside the loop so we can access the most recent times from sides of the if/else
	tm tmEnd = *localtime(&t);

	while(mem[3] < saladTotal) { // Check for busyness
		if((mem[7] + mem[8] + mem[9] > 1)) { // Measure concurrent time periods - aka when at least two saladmakers are busy
			if(timerGoing == false) { // If the timer is not running already, start it
				t = time(0);
				tmStart = *localtime(&t);
				fout << put_time(&tmStart, "%T") << " - "; // Record start time of concurrent period
				timerGoing = true; // So that we know we have a running timer				
			}
			//sleep(1); // Check if it's still concurrent after a second to avoid spamming of the log file
		}
		else {
			if(timerGoing == true) { // Record end time of concurrent period
				t = time(0);
				tmEnd = *localtime(&t);
				if(tmEnd.tm_sec <= tmStart.tm_sec + 1) { // Don't reset the timer if less than a second has passed since it was started, since we're not tracking millisecond changes
					continue;
				}
				fout << put_time(&tmEnd, "%T") << "\n";
				fout.close();
				fout.open("concurrentlog.txt", ios::app); // This makes it so that the log files are updated on each iteration; it helps with debugging if anyone gets stuck
				timerGoing = false;
			}
		}
	}

	if(timerGoing == true) { // In case the work finished while in concurrent execution and the timer never got stopped - this closes the final period
		time_t t = time(0);
		tm tm = *localtime(&t);
		fout << put_time(&tm, "%T") << "\n";
		timerGoing = false;
	}

	fout.close();

	int err = shmdt(mem); // Detach from the segment
	if (err == -1) {
		cerr << "Error detaching from shared memory!" << endl;
	}
	else {
		cout << "Logger detachment successful, ID " << err << endl;
	}

	return 0;
}