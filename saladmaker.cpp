#include <sys/types.h> // For the time_t and size_t type definitions
#include <sys/ipc.h> // For the IPC_CREAT and IPC_PRIVATE mode bits
#include <sys/shm.h> // For shared memory
#include <semaphore.h> // For semaphores
#include <cstdio> // For the remove() function
#include <cstdlib> // For rand()
#include <iostream> // Who doesn't include this?
#include <string> // For argument parsing and log handling
#include <unistd.h> // For usleep()
#include <ctime> // For the "temporal" part of "temporal log" - also for srand() seeding
#include <iomanip> // For put_time() which enables outputting times to a file
#include <fstream> // For file I/O
#include <chrono> // For highly accurate, C++ timekeeping

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

	string semName = "sem" + smNumStr;

	sem_t *sem = sem_open(semName.c_str(), 0); // Each SM opens the semaphore that was meant for it, using its name as the ID
	sem_t *shmSem = sem_open("/shmSem", 0); // All of them open the two special ones though
	sem_t *outSem = sem_open("/outSem", 0);

	int *mem;
	void* tempMem = (int*)shmat(shmid, NULL, 0); // Pointer magic! Casting the shared memory pointer to void, to then cast it back to int resolves some nasty issues (because for some reason, shmat() returns void in C++)
	if (tempMem == reinterpret_cast<void*>(-1)) { // reinterpret_cast() ensures that we retain the same address when converting from void, which is exactly what we're doing
		cerr << "Could not attach to shared memory!" << endl;
		return -1;
	}
	else {
		mem = reinterpret_cast<int*>(tempMem); // Now we have a proper pointer to an array of ints in shared memory
	}

	remove("saladlog.txt"); // Delete file before each iteration to clear it
	ofstream fout;
	fout.open("saladlog.txt", ios::app); // Opening file in append mode

	int workTimeTotal = 0; // A variable to store the total time the saladmaker spent working
	int waitTimeTotal = 0; // Ditto, for time spent waiting on the semaphore

	// Here we'll be storing the current ingredient measurements
	int currentOnionWeight = 0;
	int currentPepperWeight = 0;
	int currentTomatoWeight = 0;

	srand(time(0));
	while(mem[3] < saladTotal) {

		sem_wait(outSem);
		time_t t = time(0);
		tm tm = *localtime(&t);
		fout << put_time(&tm, "%T") << " [SM #" << smNum << "] Waiting for ingredients...\n";
		sem_post(outSem);
		cout << "[SM #" << smNum << "] Waiting for ingredients..." << endl;

		auto waitTimeStart = chrono::system_clock::now(); // Start tracking the waiting time

		sem_wait(sem); // Sleep peacefully until the chef wakes us up with ingredients

		auto waitTimeEnd = chrono::system_clock::now(); // If we get here, that means we've been awoken, so no more waiting!
		auto waitTime = chrono::duration_cast<chrono::microseconds>(waitTimeEnd - waitTimeStart); // Have to use duration_cast as duration usually gives time in float formats
		waitTimeTotal += waitTime.count(); // Recording time in integers - since we're tracking it with microsecond precision, we don't lose any time even when the times are under one second!

		sem_wait(outSem);
		t = time(0);
		tm = *localtime(&t);
		fout << put_time(&tm, "%T") << " [SM #" << smNum << "] Awake and ready to work!\n";
		sem_post(outSem);
		cout << "[SM #" << smNum << "] Awake and ready to work!" << endl;

		if(mem[3] == saladTotal) { // Need an extra finish check because otherwise some of the saladmakers might go through their loop one more time than needed
			break; // If we're done, you can go home, no need to try to weigh/calculate anything else!
		}

		auto workTimeStart = chrono::system_clock::now();

		// Weigh each SM's always-available ingredient first
		if(smNum == 0) {
			currentOnionWeight = rand() % 12 + 24; // Simple formula I came up with on the fly to get an int in the specified range: rand() % (1.2*w - 0.8*w) + 0.8*w (writing them in actual numbers as C++ can't compile with doubles in the equation, even if they come out to an int)
			if(currentOnionWeight < 30) { // If there is not enough, pick another one, which will be enough for sure - we can do this instantly as we always have one ingredient available to us
				currentOnionWeight += rand() % 12 + 24;
			}
		}
		else if(smNum == 1) {
			currentPepperWeight = rand() % 20 + 40;
			if(currentPepperWeight < 50) { // If there is not enough, pick another one, which will be enough for sure
				currentPepperWeight += rand() % 20 + 40;
			}			
		}
		else if(smNum == 2) {
			currentTomatoWeight = rand() % 32 + 64;
			if(currentTomatoWeight < 80) { // If there is not enough, pick another one, which will be enough for sure
				currentTomatoWeight += rand() % 32 + 64;
			}			
		}

		sem_wait(shmSem);
		mem[smNum + 7] = 1; // Notify chef that SM is busy
		if(smNum == 0) {
			mem[1] -= 1; // Consume resource - in this case, take an ingredient that the chef put down (what we consume depends on which SM we're in, of course)
			mem[2] -= 1;
		}
		else if(smNum == 1) {
			mem[0] -= 1;
			mem[2] -= 1;
		}
		else if(smNum == 2) {
			mem[0] -= 1;
			mem[1] -= 1;
		}
		sem_post(shmSem);

		sem_wait(outSem);
		t = time(0);
		tm = *localtime(&t);
		fout << put_time(&tm, "%T") << " [SM #" << smNum << "] Taken ingredients from chef, now measuring...\n";
		sem_post(outSem);
		cout <<  "[SM #" << smNum << "] Taken ingredients from chef, now measuring..." << endl;

		// Measure the other two ingredients that we got from the chef - the weights are actually decided in the moment of measurement; before that they exist in superposition :)
		if(smNum == 0) {
			currentPepperWeight += rand() % 20 + 40; // Important to use += instead of = to ensure that even if we have to pick an ingredient twice, we save the previous weight
			currentTomatoWeight += rand() % 32 + 64;
			if(currentPepperWeight < 50 || currentTomatoWeight < 80) { // If any of the ingredients are below the required weight, we need to go back to waiting until the chef picks the right pair of ingredients again
				sem_wait(outSem);
				t = time(0);
				tm = *localtime(&t);
				fout << put_time(&tm, "%T") << " [SM #" << smNum << "] The ingredients don't reach the required weight! Need to wait another round\n";
				sem_post(outSem);
				sem_wait(shmSem);
				mem[smNum + 7] = 0; // Set SM to available
				sem_post(shmSem);
				cout << "[SM #" << smNum << "] The ingredients don't reach the required weight! Need to wait another round" << endl;

				auto workTimeEnd = chrono::system_clock::now(); // Making sure we catch all cases, even when the worker goes back to waiting - otherwise, we would lose the time it spent weighing for that round, no matter how insignificant it might be
				auto workTime = chrono::duration_cast<chrono::microseconds>(workTimeEnd - workTimeStart);
				workTimeTotal += workTime.count();
				continue;
			}
		}
		else if(smNum == 1) { // Same exact sequence of events, just with different ingredients
			currentOnionWeight += rand() % 12 + 24;
			currentTomatoWeight += rand() % 32 + 64;
			if(currentOnionWeight < 30 || currentTomatoWeight < 80) {
				sem_wait(outSem);				
				t = time(0);
				tm = *localtime(&t);
				fout << put_time(&tm, "%T") << " [SM #" << smNum << "] The ingredients don't reach the required weight! Need to wait another round\n";
				sem_post(outSem);
				sem_wait(shmSem);
				mem[smNum + 7] = 0; // Set SM to available
				sem_post(shmSem);
				cout << "[SM #" << smNum << "] The ingredients don't reach the required weight! Need to wait another round" << endl;

				auto workTimeEnd = chrono::system_clock::now(); // Making sure we catch all cases, even when the worker goes back to waiting - otherwise, we would lose the time it spent weighing for that round, no matter how insignificant it might be
				auto workTime = chrono::duration_cast<chrono::microseconds>(workTimeEnd - workTimeStart);
				workTimeTotal += workTime.count();				
				continue;
			}
		}
		else if(smNum == 2) { // Same exact sequence of events, just with different ingredients
			currentOnionWeight += rand() % 12 + 24;
			currentPepperWeight += rand() % 20 + 40;
			if(currentPepperWeight < 50 || currentOnionWeight < 30) {
				sem_wait(outSem);
				t = time(0);
				tm = *localtime(&t);
				fout << put_time(&tm, "%T") << " [SM #" << smNum << "] The ingredients don't reach the required weight! Need to wait another round\n";
				sem_post(outSem);
				sem_wait(shmSem);
				mem[smNum + 7] = 0; // Set SM to available
				sem_post(shmSem);
				cout << "[SM #" << smNum << "] The ingredients don't reach the required weight! Need to wait another round" << endl;

				auto workTimeEnd = chrono::system_clock::now(); // Making sure we catch all cases, even when the worker goes back to waiting - otherwise, we would lose the time it spent weighing for that round, no matter how insignificant it might be
				auto workTime = chrono::duration_cast<chrono::microseconds>(workTimeEnd - workTimeStart);
				workTimeTotal += workTime.count();
				continue;
			}
		}

		sem_wait(outSem);
		t = time(0);
		tm = *localtime(&t);
		fout << put_time(&tm, "%T") << " [SM #" << smNum << "] Gathered this much of each ingredient: " << currentPepperWeight << "g pepper, " << currentOnionWeight << "g onion, " << currentTomatoWeight << "g tomato" << "\n";
		sem_post(outSem);
		cout << "[SM #" << smNum << "] Gathered this much of each ingredient: " << currentPepperWeight << "g pepper, " << currentOnionWeight << "g onion, " << currentTomatoWeight << "g tomato" << endl;

		double smTimeMin = 0.8*double(smTime); // As specified in the requirements
		double f = (double)rand() / RAND_MAX; // Same logic as in chef
		double actualSMTime = smTimeMin + f * (double(smTime) - smTimeMin);

		sem_wait(outSem);
		t = time(0);
		tm = *localtime(&t);
		fout << put_time(&tm, "%T") << " [SM #" << smNum << "] Chopping up ingredients for " << to_string(actualSMTime) << "\n";
		sem_post(outSem);

		cout << "[SM #" << smNum << "] Chopping up ingredients for " << to_string(actualSMTime) << endl;
		usleep(actualSMTime*1000000); // I know this looks like the saladmakers are sleeping on the job, but trust me, they're actually hard at work!

		if(mem[3] == saladTotal) { // In extremely rare cases, a saladmaker would get assigned the final salad and get to this point, only for the chef to assign it to another one before the SM finished, making the program hang
			break; // This should fix it
		}

		// Salad done! Update all necessary statistics
		sem_wait(shmSem);
		if(mem[3] < saladTotal) { // An extra check to make sure the count doesn't get incremented extra at the end of execution
			mem[3] += 1; // Increment the total salad counter
			mem[smNum + 4] += 1; // Increment the specific SM's salad counter

			mem[10 + 3*smNum] += currentOnionWeight; // Add ingredient weight statistics to shared memory
			mem[11 + 3*smNum] += currentPepperWeight;
			mem[12 + 3*smNum] += currentTomatoWeight;

			sem_wait(outSem);
			t = time(0);
			tm = *localtime(&t);
			fout << put_time(&tm, "%T") << " [SM #" << smNum << "] Salad #" << mem[smNum + 4] << " finished!\n";
			cout << "[SM #" << smNum << "] Salad #" << mem[smNum + 4] << " finished!" << endl;

			cout << "Total number of salads: " << mem[3] << endl;
			sem_post(outSem);

			currentOnionWeight = 0; // As the salad is done, reset the weights
			currentPepperWeight = 0;
			currentTomatoWeight = 0;

			mem[smNum + 7] = 0; // SM is no longer busy
		}
		mem[smNum + 7] = 0; // Safeguard in case the above if block doesn't trigger and consequently the SM never becomes available
		sem_post(shmSem);

		auto workTimeEnd = chrono::system_clock::now();
		auto workTime = chrono::duration_cast<chrono::microseconds>(workTimeEnd - workTimeStart); // Have to use duration_cast as duration usually gives time in float formats
		workTimeTotal += workTime.count();

		fout.close();
		fout.open("saladlog.txt", ios::app); // This makes it so that the log files are updated on each iteration; it helps with debugging if anyone gets stuck

	}

	cout << "SM #" << smNum << " finished work!" << endl;

	sem_wait(shmSem);
	// Update final time counters - this way, we only need one pass at the shared memory
	mem[19 + 2*smNum] = workTimeTotal / 1000000; // Convert from microseconds back to seconds - the integer division automatically takes care of the remainder
	mem[20 + 2*smNum] = waitTimeTotal / 1000000;
	mem[smNum + 7] = 0; // And JUST IN CASE the busy flag still didn't get reset (has happened before), reset it for one final time after we're all done
	sem_post(shmSem);

	sem_wait(outSem);
	time_t t = time(0);
	tm tm = *localtime(&t);
	fout << put_time(&tm, "%T") << " [SM #" << smNum << "] Cleaning up the kitchen...\n";
	sem_post(outSem);
	cout << "[SM #" << smNum << "] Cleaning up the kitchen..." << endl;

	fout.close();
	
	int err = shmdt(mem); // Detach from the segment - we only remove it in the chef
	if (err == -1) {
		cerr << "Error detaching from shared memory!" << endl;
	}
	else {
		cout << "SM #" << smNum << " detachment successful, ID " << err << endl;
	}
	
	return 0;
}