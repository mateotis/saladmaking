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

	while(mem[3] < saladTotal) {
		sem_wait(sem);

		cout << "SM #" << smNumStr << " is awake!" << endl;

		sem_wait(shmSem);
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

		if(mem[3] < saladTotal) { // An extra check to make sure the count doesn't get incremented extra at the end of execution
			mem[3] += 1; // Increment the total salad counter
			mem[smNum + 4] += 1; // Increment the specific SM's salad counter

			cout << "SM #" << smNumStr << " made this many salads: " << mem[smNum + 4] << endl;
			cout << "Current value of mem[3]: " << mem[3] << endl;
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