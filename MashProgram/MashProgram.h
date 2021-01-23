#ifndef MashProgram_H
#define MashProgram_H

#include "Arduino.h"

class MashProgram
{
	public:
		MashProgram(char* aname);
		char* name;
		bool addInterval(int seconds, int temperature);
		int getTemperature(int secondsPassed);
		bool setTemperature(int secondsPassed,int temperature);
		bool changeTemperature(int secondsPassed,int temperature);
		int timeLeft(int secondsPassed);
		int totalTime;
		String dump();
	private:
		int* intervals;
		int* temperatures;
		int count;
};

#endif
