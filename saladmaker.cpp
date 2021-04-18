#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h>
#include <semaphore.h>
#include <cstdio> 
#include <cstdlib>
#include <iostream>
#include <string>
#include <unistd.h> // For read/write and IPC

using namespace std;

int main (int argc, char* args[]) {

	// Getting parametres from chef
	string shmidStr = args[1];
	int shmid = stoi(shmidStr);
	string smNumStr = args[2];
	int smNum = stoi(smNumStr); 
	string saladTotalStr = args[3];
	int saladTotal = stoi(saladTotalStr);
	string smTimeStr = args[4];
	int smTime = stoi(smTimeStr);

	cout << "Saladmaker shmid: " << shmid << endl;

	string semName = "sem" + smNumStr;

	sem_t *sem = sem_open(semName.c_str(), 0);
	sem_t *shmSem = sem_open("/shmSem", 0);

	int *mem;
	void* tempMem = (int*)shmat(shmid, NULL, 0); // Pointer magic! Casting the shared memory pointer to void, to then cast it back to int resolves some nasty issues
	if (tempMem == reinterpret_cast<void*>(-1)) { // reinterpret_cast() ensures that we retain the same address when converting from void and back, which is exactly what we're doing
		cerr << "Could not attach to shared memory!" << endl;
		return -1;
	}
	else {
		mem = reinterpret_cast<int*>(tempMem); // Now we have a proper int shared memory pointer 
	}

	//int saladCount = 0; // How many salads this maker has made

	int currentOnionWeight = 0;
	int currentPepperWeight = 0;
	int currentTomatoWeight = 0;

	while(mem[3] < saladTotal) {
		sem_wait(sem);

		cout << "SM #" << smNumStr << " is awake!" << endl;

		// Weigh each SM's always-available ingredient first
		srand(time(0));
		if(smNum == 0) {
			currentOnionWeight = rand() % 12 + 24; // Simple formula I came up with on the fly to get an int in the specified range: rand() % (1.2*w - 0.8*w) + 0.8*w (writing them in actual numbers as C++ can't compile with doubles in the equation, even if they come out to an int)
		}
		else if(smNum == 1) {
			currentPepperWeight = rand() % 20 + 40;
		}
		else if(smNum == 2) {
			currentTomatoWeight = rand() % 32 + 64;
		}

		sem_wait(shmSem);
		mem[smNum + 7] = 1; // Notify chef that SM is busy
		if(smNum == 0) {
			mem[1] = 0;
			mem[2] = 0;
		}
		else if(smNum == 1) {
			mem[0] = 0;
			mem[2] = 0;
		}
		else if(smNum == 2) {
			mem[0] = 0;
			mem[1] = 0;
		}
		
		sem_post(shmSem);

		// Measure the other two ingredients that we got from the chef
		if(smNum == 0) {
			currentPepperWeight = rand() % 20 + 40;
			currentTomatoWeight = rand() % 32 + 64;
		}
		else if(smNum == 1) {
			currentOnionWeight = rand() % 12 + 24;
			currentTomatoWeight = rand() % 32 + 64;
		}
		else if(smNum == 2) {
			currentOnionWeight = rand() % 12 + 24;
			currentPepperWeight = rand() % 20 + 40;
		}

		cout << "SM #" << smNum << " has this much of each ingredient: " << currentPepperWeight << " pepper, " << currentOnionWeight << " onion, " << currentTomatoWeight << " tomato" << endl;

		srand(time(0));
		double smTimeMin = 0.8*double(smTime); // As specified in the requirements
		double f = (double)rand() / RAND_MAX;
		double actualSMTime = smTimeMin + f * (double(smTime) - smTimeMin);

		cout << "SM #" << smNum << " working for: " << actualSMTime << endl;
		sleep(actualSMTime); // I know this looks like the saladmakers are sleeping on the job, but trust me, they're actually hard at work!

		sem_wait(shmSem);
		if(mem[3] < saladTotal) { // An extra check to make sure the count doesn't get incremented extra at the end of execution
			mem[3] += 1; // Increment the total salad counter
			mem[smNum + 4] += 1; // Increment the specific SM's salad counter

			cout << "SM #" << smNum << " made this many salads: " << mem[smNum + 4] << endl;
			cout << "Current value of mem[3]: " << mem[3] << endl;

			mem[smNum + 7] = 0; // SM is no longer busy
		}
		sem_post(shmSem);

	}


	//exit(0);
	
	//mem[smNum] = smNum; /* Give it a different value */
	
	//cout << "Changed mem" << smNum << " is now " << mem[0] << endl;
	
	int err = shmdt(mem); // Detach from the segment
	if (err == -1) {
		cerr << "Error detaching from shared memory!" << endl;
	}
	else {
		cout << "Detachment successful, ID " << err << endl;
	}
	
	return 0;
}