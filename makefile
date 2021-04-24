all: chef saladmaker logger

chef: chef.o
	g++ chef.o -o chef -lpthread
saladmaker: saladmaker.o
	g++ saladmaker.o -o saladmaker -lpthread
logger: logger.o
	g++ logger.o -o logger -lpthread
logger.o: logger.cpp
	g++ -c logger.cpp -o logger.o
saladmaker.o: saladmaker.cpp
	g++ -c saladmaker.cpp -o saladmaker.o
chef.o: chef.cpp chef.h
	g++ -c chef.cpp -o chef.o
clean:
	rm *.o *.txt chef saladmaker logger