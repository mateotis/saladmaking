#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <stdio.h> 
#include <stdlib.h>

using namespace std;

int main ( int argc , char ** argv ) {
	int id , err;
	int *mem;
	key_t key = 4444;

	mem = shmat (key, NULL,0); /* Attach the segment */
	
	if ((int) mem == -1) 
		perror (" Attachment .");
	else 
		printf (" Attached ");
	
	//*mem = 2; /* Give it a different value */
	cout << "Value 0: " << mem[0] << endl;
	
	//printf (" Changed mem is now %d\n", *mem );
	
	err = shmdt (mem); /* Detach segment */
	
	if (err == -1) 
		perror (" Detachment .");
	else 
		printf (" Detachment %d\n", err );
	
	return 0;
}