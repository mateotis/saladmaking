#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>
#include <cstdio> 
#include <cstdlib> // For rand()
#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h> // For read/write and IPC
#include <sys/wait.h> // For wait() and signals
#include <ctime> // For seeding rand() with time()

#define SHMSIZE 1024 // Size of the shared memory segment; should be more than enough for our purposes

using namespace std;

int main(int argc, char* args[]) {

	string saladTotalStr = "";
	string chefTimeStr = "";
	string smTimeStr = "";

	// Default values, in case user omits some params
	int saladTotal = 10;
	int chefTime = 2;
	int smTime = 1;

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
	}

	cout << "SALADMAKING SETTINGS" << endl << "Number of salads to be made: " << saladTotal << endl << "Chef resting base time: " << chefTime << endl << "Saladmaker working base time: " << smTime << endl;

	// Creating one semaphore for each saladmaker
	sem_t *sem0 = sem_open("/sem0", O_CREAT, 0640, 0);
	sem_t *sem1 = sem_open("/sem1", O_CREAT, 0640, 0);
	sem_t *sem2 = sem_open("/sem2", O_CREAT, 0640, 0);

	sem_t *shmSem = sem_open("/shmSem", O_CREAT, 0640, 1); // Special semaphore for accessing the shared memory, only one process can have it at a time

	sem_t* semArray[3] = {sem0, sem1, sem2}; // For ease of access

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


	// Initialising all variables in shared memory
	mem[0] = 0; // Number of onions available
	mem[1] = 0; // Number of peppers available
	mem[2] = 0; // Number of tomatoes available
	mem[3] = 0; // The current number of salads
	mem[4] = 0; // Salads produced by SM #0
	mem[5] = 0; // Salads produced by SM #1
	mem[6] = 0; // Salads produced by SM #2


	// Forking saladmaker child

	int count = 0;
	for(int childNum = 0; childNum < 3; childNum++) {
		sleep(0.1); // Stagger the kids until I figure out how to make them play nice
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

	for(int i = 0; i < saladTotal; i++) {
		// Before each serving of ingredients, the chef rests for a randomly determined time based on user input
		srand(time(0));
		double chefTimeMin = 0.5*double(chefTime); // As specified in the requirements
		double f = (double)rand() / RAND_MAX;
		double actualChefTime = chefTimeMin + f * (double(chefTime) - chefTimeMin);

		cout << "Chef resting for " << actualChefTime << endl;
		sleep(actualChefTime);

		int choice = rand() % 3; // Simulating the chef picking two ingredients at random (three possible combinations)

		sem_wait(shmSem);
		if(choice == 0) { // Picking tomatoes and peppers for SM #0
			mem[1] = 1;
			mem[2] = 1;
		}
		else if(choice == 1) { // Picking tomatoes and onions for SM #1
			mem[0] = 1;
			mem[2] = 1;
		}
		else if(choice == 2) { // Picking onions and peppers for SM #2
			mem[0] = 1;
			mem[1] = 1;
		}
		sem_post(shmSem);
		cout << "Waking up SM #" << choice << endl;
		sem_post(semArray[choice]);
		//mem[3] += 1;

	}

	sleep(2);
	for(int i = 0; i < 3; i++) { // Unlock all semaphores so all saladmakers have a chance to gracefully finish
		sem_post(semArray[i]);
	}

	pid_t pid;
	int status = 0;
	while ((pid = wait(&status)) != -1) {}

	cout << "final status: " << endl;
	for(int i = 0; i < 7; i++) {
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