all: chef saladmaker

chef: chef.o
	g++ chef.o -o chef -lpthread
saladmaker: saladmaker.o
	g++ saladmaker.o -o saladmaker -lpthread
saladmaker.o: saladmaker.cpp
	g++ -c saladmaker.cpp -o saladmaker.o
chef.o: chef.cpp
	g++ -c chef.cpp -o chef.o
clean:
	rm *.o chef saladmaker