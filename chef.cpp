#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <cstdio> 
#include <cstdlib>
#include <iostream>

#define SHMSIZE 1024 // Size of the shared memory segment; should be more than enough for our purposes

using namespace std;

int main() {

	int err = 0;

	int shmid = shmget(IPC_PRIVATE, 1024, IPC_CREAT | 0640); // Correct permissions (0640 in this case) are super important, shmget() fails otherwise

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

	mem[0] = 4;
	mem[1] = 999;
	cout << "Original mem0: " << mem[0] << endl;
	cout << "Original mem1: " << mem[1] << endl;

	cout << "Open other process" << endl;
	getchar();

	cout << "New mem0: " << mem[0] << endl;
	cout << "New mem1: " << mem[1] << endl;

	err = shmdt(mem); // Detach from the segment
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