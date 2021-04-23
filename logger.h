#ifndef LOGGER_H
#define LOGGER_H

#include <string>

struct LogLine { // Struct that stores all elements of a line in the log file
	int secs;
	int mins;
	int hours;
	std::string entity;
	std::string content;
};

void temporalReordering(std::string fileName);

#endif