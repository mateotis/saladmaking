#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <cstdio> 
#include <cstdlib>
#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h> // For read/write and IPC
#include <sys/wait.h> // For wait() and signals

#define SHMSIZE 1024 // Size of the shared memory segment; should be more than enough for our purposes

using namespace std;

int main() {

	int shmid = shmget(IPC_PRIVATE, SHMSIZE, IPC_CREAT | 0640); // Correct permissions (0640 in this case) are super important, shmget() fails otherwise

	if (shmid == -1) {
		cerr << "Could not create shared memory!" << endl;
		return -1;
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

	mem[0] = 12;
	mem[1] = 999;
	mem[2] = -43;
	cout << "Original mem0: " << mem[0] << endl;
	cout << "Original mem1: " << mem[1] << endl;
	cout << "Original mem2: " << mem[2] << endl;

	// Forking saladmaker child

	for(int childNum = 0; childNum < 3; childNum++) {
		sleep(1); // Stagger the kids until I figure out how to make them play nice
		pid_t pid;
		pid = fork();

		if(pid < 0) { // Error handling
			cerr << "Error: could not start worker child process." << endl;
			return -1;
		} 
		else if(pid == 0) { // In child
			// The tried-and-tested int->string->char argument passing procedure (I do wish there was a more convenient way...)
			string saladmakerName = "saladmaker"; // Could just pass it as a string directly to execv() but the compiler would complain; this is cleaner
			string segmentIDStr = to_string(shmid);
			string childNumStr = to_string(childNum);

			char* saladmakerChar = new char[30];
			char* segmentIDChar = new char[30];
			char* childNumChar = new char[30];

			strcpy(segmentIDChar, segmentIDStr.c_str());
			strcpy(saladmakerChar, saladmakerName.c_str());
			strcpy(childNumChar, childNumStr.c_str());

			char* arg[] = {saladmakerChar, segmentIDChar, childNumChar, NULL};
			execv("./saladmaker", arg);	// Start the saladmaker!
		}
	}

	pid_t pid;
	int status = 0;
	int count = 0;
	while ((pid = wait(&status)) != -1) {
		cout << "==SALADMAKER #" << count << "==" << endl;
		cout << "New mem0: " << mem[0] << endl;
		cout << "New mem1: " << mem[1] << endl;
		cout << "New mem2: " << mem[2] << endl;
		count++;
	} // Wait for all saladmakers for finish

	int err = shmdt(mem); // Detach from the segment
	if (err == -1) {
		cerr << "Error detaching from shared memory!" << endl;
	}
	else {
		cout << "Detachment successful, ID " << err << endl;
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