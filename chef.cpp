#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>
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

	sem_t *sem0 = sem_open("/sem0", O_CREAT, 0640, 0);
	sem_t *sem1 = sem_open("/sem1", O_CREAT, 0640, 0);
	sem_t *sem2 = sem_open("/sem2", O_CREAT, 0640, 0);

	sem_t *shmSem = sem_open("/shmSem", O_CREAT, 0640, 1); // Special semaphore for accessing the shared memory, only one process can have it at a time

	sem_t* semArray[3] = {sem0, sem1, sem2};

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

/*	mem[0] = 12;
	mem[1] = 999;
	mem[2] = -43;
	cout << "Original mem0: " << mem[0] << endl;
	cout << "Original mem1: " << mem[1] << endl;
	cout << "Original mem2: " << mem[2] << endl;*/

	// Forking saladmaker child

	int count = 0;
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
/*		else {

			pid_t pid;
			int status = 0;
			while ((pid = wait(&status)) != -1) {
				cout << "==SALADMAKER #" << count << "==" << endl;
				cout << "New mem0: " << mem[0] << endl;
				cout << "New mem1: " << mem[1] << endl;
				cout << "New mem2: " << mem[2] << endl;
				count++;
				cout << "Count: " << count << endl;
			} // Wait for all saladmakers for finish			
		}*/
	}

	for(int i = 0; i < 3; i++) {
		sleep(1);
		sem_wait(shmSem);
		if(i == 0) {
			mem[1] = 1;
			mem[2] = 1;
		}
		else if(i == 1) {
			mem[0] = 1;
			mem[2] = 1;
		}
		else if(i == 2) {
			mem[0] = 1;
			mem[1] = 1;
		}
		sem_post(shmSem);

		cout << "Waking up SM #" << i << endl;
		sem_post(semArray[i]);
	}

	pid_t pid;
	int status = 0;
	while ((pid = wait(&status)) != -1) {}

	cout << "final status: " << endl;
	for(int i = 0; i < 3; i++) {
		cout << "mem" << i << ":" << mem[i] << endl;
	}

	sem_close(sem0);
	sem_unlink("/sem0");
	sem_close(sem1);
	sem_unlink("/sem1");
	sem_close(sem2);
	sem_unlink("/sem2");

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