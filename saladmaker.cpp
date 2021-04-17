#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <cstdio> 
#include <cstdlib>
#include <iostream>

using namespace std;

int main () {
	int shmid, err;

	shmid = 0;

	int *mem;
	void* tempMem = (int*)shmat(shmid, NULL, 0); // Pointer magic! Casting the shared memory pointer to void, to then cast it back to int resolves some nasty issues
	if (tempMem == reinterpret_cast<void*>(-1)) { // reinterpret_cast() ensures that we retain the same address when converting from void and back, which is exactly what we're doing
		cerr << "Could not attach to shared memory!" << endl;
		return -1;
	}
	else {
		mem = reinterpret_cast<int*>(tempMem); // Now we have a proper int shared memory pointer 
	}

	cout << "Current mem0: " << mem[0] << endl;
	cout << "Current mem1: " << mem[1] << endl;
	
	mem[0] = 2; /* Give it a different value */
	mem[1] = 1337;
	
	cout << "Changed mem0 is now " << mem[0] << endl;
	cout << "Changed mem1 is now " << mem[1] << endl;
	
	err = shmdt(mem); // Detach from the segment
	if (err == -1) {
		cerr << "Error detaching from shared memory!" << endl;
	}
	else {
		cout << "Detachment successful, ID " << err << endl;
	}
	
	return 0;
}