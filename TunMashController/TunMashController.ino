#include <MashProgram.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal.h>
#include <DFR_Key.h>

//releu conectat la 

#define ON   0
#define OFF  1
#define PUMP 11
#define HEAT 12
#define ONE_WIRE_BUS 3
#define BUZZER 13

#define UP 3
#define DOWN 4
#define LEFT 2
#define RIGHT 5
#define SELECT 1

enum Menus { TEMPERATURE, TIMER, PUMPON, PUMPOFF, THRESHOLD, PRESETS, BACK, START, PANIC, MENU_COUNT };



LiquidCrystal lcd(8,9,4,5,6,7);
/*
Key Codes (in left-to-right order):

None   - 0
Select - 1
Left   - 2
Up     - 3
Down   - 4
Right  - 5
*/
DFR_Key keypad(1); 
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensor(&oneWire);

// default parameters
//int timerInterval = 1;
//int targetTemperature = 30;
int pumpIntervalOff = 10;
int pumpIntervalOn = 30;
int tempReadInterval = 2000;
float thresholdTemperature = 0.5;
int beepTimes = 0;
unsigned long lastBeep = 0;


bool heat = false;
bool pumpStatus = false;
bool panic = false;
unsigned long lastPumpSwitch = 0;
unsigned long pumpSwitchInterval;
unsigned long totalPanic = 0;
unsigned long panicStart;

int localKey;

float temperature;
unsigned long lastTempRead = 0;

long startTimer = 0;
unsigned long originalStartTime = 0;
String elapsedTime;
bool timerStarted = false;

bool inMenu = true;
int currentMenuId = 0;
Menus currentMenu = TEMPERATURE; 
String line1;
String line2;
unsigned long lastLcdRefresh;

#define _PRESETS_COUNT 5
MashProgram program0("45m:67\337\17615m:75\337");
MashProgram program1("2m:20\337\17630\337\17640\337");
MashProgram program2("50m:65\337\17610m:75\337");
MashProgram program3("1h:67\337");
MashProgram program4("45m:67\337");

MashProgram *programs[_PRESETS_COUNT] = {&program0, &program1, &program2, &program3, &program4};
int currentProgram = 2;


void setup()
{ 
  Serial.begin(9600);
  pinMode(BUZZER,OUTPUT);
  sensor.begin();
  initLcdDisplay();
  loadPresets();
  refreshLcd();
  keypad.setRate(50);
  relay_init();//initializarea releului
}

void loadPresets(){
  program0.addInterval(45*60,67);
  program0.addInterval(15*60,75);
  
  program1.addInterval(2*60,20);
  program1.addInterval(2*60,30);
  program1.addInterval(2*60,40);
  
  program2.addInterval(50*60,65);
  program2.addInterval(15*60,75);

  program3.addInterval(60*60,67);

  program4.addInterval(45*60,67);
  
  Serial.println(programs[currentProgram]->dump());
}


void initLcdDisplay(){
  byte pumpChar[8]={
    B11111,
    B00100,
    B00100,
    B01110,
    B01110,
    B01110,
    B01110,
    B11111
  };
  
  byte heatChar[8]={
    B00000,
    B11001,
    B00110,    
    B11001,
    B00110,
    B11001,
    B00110,
    B00000
  };
  
  lcd.begin(16,2);
  lcd.createChar(0,pumpChar);
  lcd.createChar(1,heatChar);
}

void writeLcdLine(String str,int row){
  lcd.setCursor(0,row);
  str = str.substring(0,15);
  int len = lcd.print(str);
  for(byte i = len; i < 15; i++){
    lcd.write(' ');
  }
}

void setLcdHeat(bool aHeat){
  lcd.setCursor(15,0);
  if(aHeat){
     lcd.write(byte(1));
  } else {
      lcd.print(" ");
  }
}

void setLcdPump(){
  lcd.setCursor(15,1);
  if(pumpStatus){
    lcd.write(byte(0));
  } else {
    lcd.print(" ");
  }
}

void setPumpStatus(bool aPump){
  pumpStatus = aPump;
  relay_SetStatus(PUMP, pumpStatus);//Releu 2 ON
  Serial.print(", Pump: ");
  Serial.println(pumpStatus);
  setLcdPump();
}


void loop() {
  if(millis() - lastTempRead > tempReadInterval){
    sensor.requestTemperatures();
    float ltmp = sensor.getTempCByIndex(0);
    lastTempRead = millis();
    if(abs(temperature - ltmp) > 0.1){
      temperature = float(round(ltmp*10))/10;
      Serial.print("Celsius temperature: ");
      Serial.print(temperature); 
      Serial.print(" ->");
      Serial.println(programs[currentProgram]->getTemperature(secondsPassed()));
      Serial.print("isMenu: ");
      Serial.println(inMenu);
    }
    refreshLcd();
    setRelays();
  }

  if(!timerStarted && beepTimes>0){
    if(millis()-lastBeep > 3000){
      lastBeep = millis();      
      int i;
      for(i=0;i<20;i++){
        digitalWrite(BUZZER,HIGH);
        delay(1);
        digitalWrite(BUZZER,LOW);
        delay(1);
      }
      beepTimes--;
    }
  }  

  if(millis()-lastLcdRefresh>1000){
    refreshLcd();
  }

  if(timerStarted && secondsLeft()<=0){
    endProgramScenario();
  }
  
  localKey = keypad.getKey();
  if (localKey != SAMPLE_WAIT && localKey != NO_KEY)
  {
    Serial.println(localKey);
    if(inMenu){
      switch(localKey){
        case DOWN:
          tryChangeValue(-1);
          break;
        case UP:
          tryChangeValue(1);
          break;
        case RIGHT: 
          tryChangeMenuId(1);
          break;
        case LEFT:
          tryChangeMenuId(-1);
          break;
        case SELECT:
          trySelectMenu();
          break;
      }
    } else {
      switch(localKey){
        case DOWN:
          programs[currentProgram]->changeTemperature(secondsPassed(),-1);
          break;
        case UP:
          programs[currentProgram]->changeTemperature(secondsPassed(),1);
          break;
        case SELECT:
          inMenu = true;
          currentMenuId = BACK;
          break;
        case RIGHT: 
          //shorten time with 1 minute = set starttimer earlier 1 minute
          startTimer -= 60000;
          delay(100);
          break;
        case LEFT: 
          //increase time with 1 minute = set starttimer later 1 minute
          startTimer = min(startTimer + 60000,millis());
          delay(100);
          break;
      }
    }
    refreshLcd();
  }
    
  delay(50);
}

void endProgramScenario(){
  timerStarted = false;
  beepTimes=3;
  elapsedTime = secondsToTime((millis()-originalStartTime)/1000);
  if(panic){
    totalPanic += (millis() - panicStart);
    panic = false;    
  }
  refreshLcd();
}

void tryChangeValue(int directie){
  switch (currentMenu) {
    case TEMPERATURE:
      programs[currentProgram]->changeTemperature(secondsPassed(),directie);
      break;
    case TIMER:
      //timerInterval += directie;
      if(timerStarted){
        startTimer += directie*60*1000;
      };
      break;
    case PUMPON:
      pumpIntervalOn += directie;
      break;
    case PUMPOFF:
      pumpIntervalOff += directie;
      break;
    case THRESHOLD:
      thresholdTemperature += float(directie)/2;
      break;
    case PRESETS:
      if(! timerStarted) {
        currentProgram = (currentProgram + directie + _PRESETS_COUNT) % _PRESETS_COUNT;
      }
      break;
  }
}

void trySelectMenu(){
  switch(currentMenu){
    case BACK:
      inMenu = false;
      break;
    case START:
      if(! timerStarted){
        startTimer = millis();
        originalStartTime = startTimer;
        timerStarted = true;
        inMenu = false;
      } else {
        endProgramScenario();
      }
      break;
    case PANIC:
      panic = !panic;
      if(timerStarted){
        if(panic){
          // pornit
          panicStart = millis();
        } else {
          totalPanic += (millis() - panicStart);
        }
      }
      break;
  } 
}

void tryChangeMenuId(int directie){
    currentMenuId = (currentMenuId + directie + MENU_COUNT) % MENU_COUNT;
}

void setLcdLines(int meniu){
  int dummy = programs[currentProgram]->getTemperature(secondsPassed());
  switch(meniu){
    case 0: //Temperatura
      line1 = "Temperatura";
      line2 = String(dummy) +" \337C";
      currentMenu = TEMPERATURE;
      break;
    case 1:
      line1 = "Temporizator";
      //line2 = String(startTimer) + " min";
      line2 = String(programs[currentProgram]->totalTime/60) + " min";
      currentMenu = TIMER;
      break;
    case 2:
      line1 = "Int. PompaON";
      line2 = String(pumpIntervalOn) +" s";
      currentMenu = PUMPON;
      break;
    case 3:
      line1 = "Int. PompaOFF";
      line2 = String(pumpIntervalOff) +" s";
      currentMenu = PUMPOFF;
      break;
    case 4:
      line1 = "Prag temperatura";
      line2 = String(thresholdTemperature,1) + " \337C";
      currentMenu = THRESHOLD;
      break;
    case 5:
      line1 = "Presetate";
      if(timerStarted){
        line1 = "# " + line1 + " #";
      }
      line2 = String(programs[currentProgram]->name);
      currentMenu = PRESETS;
      break;
    case 6:
      line1 = "Inapoi";
      line2 = "T:" + String(temperature,1) + "\337C (" +dummy + ")";
      currentMenu = BACK;
      break;
    case 7:
      if(timerStarted) {
        line1 = "STOP";
      } else {
        line1 = "Start";
      }
      //line2 = String(targetTemperature) + "\337C / " +timerInterval + "min";
      line2 = String(dummy) + "\337C / " +programs[currentProgram]->totalTime/60 + "min";
      currentMenu = START;
      break;
    case 8:
      line1 = "PANIC";
      line2 = String(panic);
      currentMenu = PANIC;
      break;
  }
}

String secondsToTime(long secs){
  if(secs<=0){
    return "0s";
  }
  int ore = secs / 3600;
  int minute = (secs - ore * 3600) / 60;
  int secunde = secs - ore * 3600 - minute * 60;
  
  String res = String(secunde) +"s";
  
  if(ore > 0 || minute > 0){
    res = String(minute) + "m" + res;
  }

  if(ore > 0){
    res = String(ore) + "h";
  }
  return res;
}

int secondsPassed(){
  if(timerStarted){
    return (millis() - startTimer) / 1000;
  } else {
    return 0;
  }
}

int secondsLeft(){
  //return timerInterval*60 - (millis() - startTimer) / 1000;
  return programs[currentProgram]->timeLeft(secondsPassed());
  
}

String timerLeft(){
  int dummy = secondsLeft();
  if(dummy<=0){
    return "done";
  }
  return secondsToTime(dummy);
}

void refreshLcd(){
  lastLcdRefresh = millis();
  if(inMenu){
    setLcdLines(currentMenuId);
    writeLcdLine(line1,0);
    writeLcdLine(line2,1);
  } else {
    //int dummy = currentProgram->getTemperature(secondsPassed());
    writeLcdLine("T:"+String(temperature,1)+"\337C ("+String(programs[currentProgram]->getTemperature(secondsPassed()))+")",0);
    if(timerStarted){
      writeLcdLine("Left:"+timerLeft(),1);
    } else {
      writeLcdLine("END "+ elapsedTime + "/" + secondsToTime(totalPanic/1000) ,1);
    }
  }
}

void relay_init(void)//initializarea releu
{
  //set all the relays OUTPUT
  pinMode(PUMP, OUTPUT);
  pinMode(HEAT, OUTPUT);
  setPumpStatus(false);
  relay_SetStatus(HEAT, heat); //OFF RELEU 1 si 2
}

//setare on/off releu
void relay_SetStatus( int id, bool activ)
{
  if(activ){
    digitalWrite(id, ON);
  } else {
    digitalWrite(id, OFF);
  }
}

void setRelays(){
  bool newHeat = heat;
  bool newPump = pumpStatus;
  float lTrgTemp = programs[currentProgram]->getTemperature(secondsPassed());
  if(! heat){
    lTrgTemp = lTrgTemp - thresholdTemperature;
  }
  
  if(temperature < lTrgTemp){
    // turn on both
    newHeat = true;
    newPump = true;
    // after temperature change should keep pump on an interval
  } else {
    newHeat = false;  
    if(heat){
      //temp reached target, keep pump alive an interval
      lastPumpSwitch = millis();
    }
    int lSwitch = pumpStatus ? pumpIntervalOn : pumpIntervalOff;
    if(millis()-lastPumpSwitch > lSwitch*1000){
      newPump = ! pumpStatus;  
      lastPumpSwitch = millis();
    }
  }

  newHeat = newHeat && timerStarted && !panic;
  newPump = newPump && timerStarted && !panic;

  if(newHeat != heat){
    heat = newHeat;
    relay_SetStatus(HEAT, heat);//Releu 1 ON
    Serial.print("Heat: ");
    Serial.print(heat);
    setLcdHeat(heat);
  }
  if(newPump != pumpStatus){
    setPumpStatus(newPump);
  }

}
