#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <cstdio> 
#include <cstdlib>
#include <iostream>
#include <string>
#include <unistd.h> // For read/write and IPC

using namespace std;

int main (int argc, char* args[]) {

	string shmidStr = args[1];
	int shmid = stoi(shmidStr);
	string makerNumStr = args[2];
	int makerNum = stoi(makerNumStr); 

	cout << "Saladmaker shmid: " << shmid << endl;

	int *mem;
	void* tempMem = (int*)shmat(shmid, NULL, 0); // Pointer magic! Casting the shared memory pointer to void, to then cast it back to int resolves some nasty issues
	if (tempMem == reinterpret_cast<void*>(-1)) { // reinterpret_cast() ensures that we retain the same address when converting from void and back, which is exactly what we're doing
		cerr << "Could not attach to shared memory!" << endl;
		return -1;
	}
	else {
		mem = reinterpret_cast<int*>(tempMem); // Now we have a proper int shared memory pointer 
	}
	
	mem[makerNum] = makerNum; /* Give it a different value */
	
	cout << "Changed mem" << makerNum << " is now " << mem[0] << endl;
	
	int err = shmdt(mem); // Detach from the segment
	if (err == -1) {
		cerr << "Error detaching from shared memory!" << endl;
	}
	else {
		cout << "Detachment successful, ID " << err << endl;
	}
	
	return 0;
}