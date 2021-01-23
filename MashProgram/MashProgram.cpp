#include "Arduino.h"
#include "MashProgram.h"

MashProgram::MashProgram(char* aname){
	intervals = new int[0];
	temperatures = new int[0];
	name = new char[16];
	strcpy(name, aname);
	totalTime = 0;
	count = 0;
}

bool MashProgram::addInterval(int seconds, int temperature){
      int* m = new int[count+1];
      int* t = new int[count+1];
      
      if(m == nullptr || t == nullptr){
        if(m != nullptr) {delete[] m;}
        if(t != nullptr) {delete[] t;}
        return false;
      };
        
      //std::copy(intervals, intervals + count, m);
      //std::copy(temperatures, temperatures + count, t);
      
      memcpy(m, intervals, sizeof(intervals[0])*count);
      memcpy(t, temperatures, sizeof(temperatures[0])*count);
      
      //for(int i = 0; i<count-1; i++){
      //  m[i] = intervals[i];
      //  t[i] = temperatures[i];
      //}
      
      m[count] = seconds;
      t[count] = temperature;
      count++;
      totalTime += seconds;
      
      delete[] intervals;
      delete[] temperatures;
      
      intervals = m;
      temperatures = t;
     
      return true;
}

int MashProgram::getTemperature(int secondsPassed){
      int secs = 0;
      for(int i=0; i < count; i++){
        secs += intervals[i];
        if(secs>secondsPassed){
          return temperatures[i];
        }
      }
      return -1;
}

bool MashProgram::setTemperature(int secondsPassed,int temperature){
      int secs = 0;
      for(int i=0; i < count; i++){
        secs += intervals[i];
        if(secs>secondsPassed){
          temperatures[i]=temperature;
          return true;
        }
      }
      return false;
}

bool MashProgram::changeTemperature(int secondsPassed,int temperature){
      int secs = 0;
      for(int i=0; i < count; i++){
        secs += intervals[i];
        if(secs>secondsPassed){
          temperatures[i]+=temperature;
          return true;
        }
      }
      return false;
}

int MashProgram::timeLeft(int secondsPassed){
        if(secondsPassed>totalTime){
          return 0;
        }
        return totalTime - secondsPassed;
}

String MashProgram::dump(){
	String res = "";
	for(int i=0; i<count; i++){
		res += "["+String(intervals[i])+":"+ String(temperatures[i]) +"] ";
	}
	return res;
}
