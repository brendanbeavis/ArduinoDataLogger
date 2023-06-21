/////////////////////////////////////////
//                                     //
//   CAR DATA LOG AND WARNING SYSTEM   //
//     CREATED BY BRENDAN BEAVIS       //
//          COPYRIGHT 2014             //
//                                     //
/////////////////////////////////////////

///////////////////////////////////////
//   Use these pins
///////////////////////////////////////
#define rst  0    //reset pin
#define sclk 13   //TFT DIGITAL
#define mosi 11   //TFT DIGITAL
#define cs   10   //TFT DIGITAL
#define dc   8    //TFT DIGITAL
#define SD_CS 4   //SD card DIGITAL
#define SD_PIN 12 //SD card DIGITAL
#define KEY_PIN 3 //shield keypad ANALOG

/////////////////////////
//    load libraries
/////////////////////////
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library
#include <SPI.h> // no idea what this is for.
#include <avr/pgmspace.h> //USE PROGMEM coz MEMORY YO!
#include <SdFat.h> //SD card even cooler YEAH!
SdFat sdCard;
Adafruit_ST7735 tft = Adafruit_ST7735(cs, dc, rst);  //create screen pointer
#if defined(__SAM3X8E__)
#undef __FlashStringHelper::F(string_literal)
#define F(string_literal) string_literal
#endif

///////////////////////////////
//  VARs and CONSTs FOR YOUR FACE
//////////////////////////////

#define versionNum "1.5"
#define configFileName "config.ini"
#define timeFileName "time.ini"
char menuTitle[11] = "CAR LOGGER";
#define analogSensors 6

//keypad
#define Neutral 0
#define Press 1
#define Up 2
#define Down 3
#define Right 4
#define Left 5

//colors
#define hiLitecolor ST7735_CYAN
//#define hiLitecolor 0xFE80   //orange/yellow
#define menuBGcolor 0x4228   //grey
#define footBGcolor 0x4228   //grey
#define headerBGcolor 0x4228 //grey
#define cursorColor ST7735_RED
#define statOKcolor ST7735_GREEN
#define statBADcolor ST7735_RED 

//ANALOG INPUT CONSTS
char *inputText[] = {"Oil     ", "Volts   ", "Wat T   ", "Wat P   ", "  N/A   ", "  N/A   "};
char *inputUnits[] = {"psi", "v  ", "c  ", "psi", "   ", "   "};
int inputDiv[]  = {10,10,10,10,10,10};  //divide input to get final result
int inputMinAlarm[]  = {10,12,60,0,0,0};  //value at which a warning is triggered
int inputMaxAlarm[]  = {60,15,110,1,100,100};  //value at which a warning is triggered
int inputActive[]  = {1,1,1,1,0,0};  //if input is not active, set to 0

//dateTime consts and VARS
#define timeElements 6
prog_char time0[] PROGMEM = "Hour";
prog_char time1[] PROGMEM = "Min";
prog_char time2[] PROGMEM = "Day";
prog_char time3[] PROGMEM = "Month";
prog_char time4[] PROGMEM = "Year";
prog_char time5[] PROGMEM = "Save";
PROGMEM const char *timeText[] = {time0, time1, time2, time3, time4, time5};
PROGMEM prog_uint16_t maxTime[]  = {23,59,31,12,2050,59};
PROGMEM prog_uint16_t minTime[]  = {0,0,1,1,1970,0};
int thisTime[] = {8,30,6,1,2014,0};
long timeOffset = 0;
const char timeGap[] = {':',',','-','-',','}; //characters printed between each time element

//MENU ITEMS
#define SHOWSTATS 0
#define SETTIME 1
#define NEWLOG 2
#define RESET 3
#define menuSize 4  //Menu qty of options
#define menuGap 25 //gap between each line
#define headerGap 32 //the height of header
#define menuTextH 8
#define menuTextV 36
int sI = 0; //menu navigation - selected Item

//MENU TEXT
prog_char menu00[] PROGMEM = "Data Log";
prog_char menu01[] PROGMEM = "Set Time";
prog_char menu02[] PROGMEM = "New Log";
prog_char menu03[] PROGMEM = "Reset";
PROGMEM const char *menuText[] = {menu00, menu01, menu02, menu03};

//screen sizing
#define bottomLine 150 //font 1
#define statScrnRedraw 20 //500milliseconds div by 50

//    FILE VARIABLES//
SdFile dataFile;  //my data file
SdFile configFile; //config file... obviously
SdFile timeFile; //time file
int y=0; //used for filename creation
char filename[13]; //used for data filename creation

//inputs and outputs
int analogInputs[] = {0,1,2,3,4,5};
int alarmOutputs[] = {3,5};

void(* resetFunc) (void) = 0;  //resets the device

//////////////////////
//                  //
//  START THE CODE  //
//                  //
//////////////////////

void setup(void) {
  Serial.begin(9600);
  tft.initR(INITR_BLACKTAB);   // initialize a ST7735S chip, black tab
  tft.fillScreen(ST7735_BLACK); //clear screen
  //show loading screen
  tft.setTextWrap(true);
  tft.setCursor(3, 20);
  tft.setTextColor(ST7735_WHITE);
  tft.setTextSize(2);
  tft.print("Loading...");
  // render screen version number
  tft.setTextSize(1);
  tft.setCursor(78, bottomLine);
  tft.print("Ver:");
  tft.print(versionNum);
  //load SD card
  tft.setCursor(0, 60);
  if (!sdCard.begin(SD_CS, SPI_FULL_SPEED)) tft.println(" SD Card Load Fail");
  else{
    tft.println(" SD Card Loaded");
    //create file for data
    createLogFile();
    //read config file
    tft.println("");
    if(sdCard.exists(configFileName)) readConfig();
    else tft.println(" Config file not found");
    if(sdCard.exists(timeFileName)) readTimeFile();
    else tft.println(" Time file not found");
  }
  //setup pinOuput
  pinMode(alarmOutputs[0], OUTPUT); 
  pinMode(alarmOutputs[1], OUTPUT); 
  //done
  Serial.println("Init Done");

  startupSong();
  delay(500);
  drawHeader();
}

void loop() {
  drawFooter();
  drawMenu();
  int cI=Neutral;
  while(cI==Neutral) cI=getJstk();
  if(cI==Down) sI++; //now action user input
  else if (cI==Up)sI--;
  else if(cI == Press || cI == Right){
    updateTime();
    if(sI == SHOWSTATS) statScreen();
    else if(sI == SETTIME) setTime();
    else if(sI == NEWLOG) newLog(); //call new log function
    else if(sI == RESET) resetFunc();
    sI = 0;
    drawHeader();
  }
  if(sI < 0) sI = menuSize-1;
  if(sI >= menuSize) sI = 0;
}

/////////////////////
//                 //
//   MY FUNCTIONS  //
//                 //
/////////////////////

void createLogFile(){
  for(int x=0;x<1000;x++){
    String(String("data")+x+String(".txt")).toCharArray(filename, 14);
    if(!(sdCard.exists(filename))) {
      dataFile.open(filename, O_WRITE | O_CREAT | O_APPEND);
      tft.println(" Data File Created:");
      tft.println(String(String("  ")+filename));
      dataFile.print("Data Log File ");
      dataFile.println(filename);
      dataFile.close();
      break; 
    }
  }
}

void readConfig(){
  if (!configFile.open(configFileName, O_READ)) tft.print("Error opening config.");
  int readVal;
  int xInput=0;
  int xinText=0;
  int xinUnit=0;
  int xinMinAl=0;
  int xinMaxAl=0;
  int xinActive=0;
  int xinDiv=0;
  int xAlarmOutput=0;
  char character;
  String description = "";
  String value="";
  // read from the file until there's nothing else in it:
  while(description!="END") {
    if(description=="END") break;
    character = configFile.read();
    if(isalnum(character)) description.concat(character);  // Add a character to the description
    else if(character =='=') {  // start checking the value for possible results
      character = configFile.read();
      while(character != '\n') {
        value.concat(character);
        character = configFile.read();     
      }
      //write to variables
      if(description=="menuTitle") {
        value.toCharArray(menuTitle,11);
      }
      else if(description=="analogInput") {
        analogInputs[xInput]=value.toInt();
        xInput++;
      }
      else if(description=="alarmOutputs") {
        alarmOutputs[xAlarmOutput]=value.toInt();
        xAlarmOutput++;
      }
      else if(description=="inputText") {
        value.toCharArray(inputText[xinText],9);
        xinText++;
      }
      else if(description=="inputUnits") {
        value.toCharArray(inputUnits[xinUnit],4);
        xinUnit++;
      }
      else if(description=="inputMinAlarm") {
        inputMinAlarm[xinMinAl]=value.toInt();
        xinMinAl++;
      }
      else if(description=="inputMaxAlarm") {
        inputMaxAlarm[xinMaxAl]=value.toInt();
        xinMaxAl++;
      }
       else if(description=="inputActive") {
        inputActive[xinActive]=value.toInt();
        xinActive++;
      }
      else if(description=="inputDiv") {
        inputDiv[xinDiv]=value.toInt();
        xinDiv++;
      }

      value="";
      description="";
    }
  }
  // close the file:
  configFile.close();
  tft.println(" Config file loaded");
}

void readTimeFile(){
  if (!timeFile.open(timeFileName, O_READ)) tft.print("Error opening timeFile.");
  char character;
  String value;
  // read from the file until there's nothing else in it:
  character = timeFile.read();
  for(int x=0;character != 'X';x++) {
    value="";
    while(character != ',') {
      value.concat(character);
      character = timeFile.read();
    }
    thisTime[x]=value.toInt();
    character = timeFile.read(); 
  }
  // close the file:
  timeFile.close();
  tft.println(" Time file loaded");
}

void drawFooter(){
  tft.drawRect(0, 146, 128, 14, ST7735_WHITE);
  tft.fillRect(1, 147, 126, 12, footBGcolor);
  tft.setCursor(3, bottomLine);
  tft.print(String("Up:"+String(millis()/3600000)+"h"+String((millis()/60000)-(millis()/3600000)/60)+"m"));
  tft.setCursor(80, bottomLine);
  tft.print("Ver:");
  tft.print(versionNum);
}

void drawHeader(){
  tft.fillScreen(ST7735_BLACK);
  tft.drawRect(0, 0, 128, 24, ST7735_WHITE);
  tft.fillRect(1, 1, 126, 22, headerBGcolor);
  tft.setCursor(5, 5);
  tft.setTextSize(2);
  tft.print(menuTitle);
  tft.setTextSize(1);
}

void newLog(){
  drawHeader();
  drawBack();
  int cI=Neutral;
  tft.setCursor(8, 46); //need to spit out content here
  createLogFile(); //get new file
  while(cI==Neutral){ //wait for user to click back
    cI = getJstk();
    delay(75);
  }
}

void updateTime(){
  int extraHours = (millis()-timeOffset)/3600000;
  int extraMins = ((millis()-timeOffset)/60000)-(extraHours*60);
  thisTime[4]+=extraHours;
  thisTime[5]+=extraMins;
  thisTime[6]+=((millis()-timeOffset)/1000)-(extraMins*60);
  
  if(thisTime[6]>pgm_read_word_near(maxTime + 6)) {
    thisTime[6] -=60;
    thisTime[5]++;
  }
  if(thisTime[5]>pgm_read_word_near(maxTime + 5)) {
    thisTime[5] -=60;
    thisTime[4]++;
  }
  if(thisTime[4]>pgm_read_word_near(maxTime + 4)) thisTime[4] -=24;
  timeOffset = millis();
}

String timeString(){
  String theTimeString="";
  updateTime();
  for(int x=0;x<5;x++){
    if (thisTime[x] < 10) theTimeString.concat('0');
    theTimeString.concat(thisTime[x]);
    theTimeString.concat(timeGap[x]);
  }
  return theTimeString;
}

void setTime(){
  drawHeader();
  int leftRight=0;
  int topBottom=0;
  int statusWidth=63;
  int statusHeight=37;
  int leftRightOffset=65;
  int topBottomOffset=40;
  int cI=0;
  char outputHeading[10];
  boolean noExit = true;
  
  while(noExit){
    topBottom=0;
    leftRight=0;
    for (int x=0;x<timeElements;x++){
      if (sI==x) {
        tft.setTextColor(ST7735_BLACK);
        tft.fillRect(leftRight*leftRightOffset, 30+topBottom*topBottomOffset, statusWidth, statusHeight, hiLitecolor);
      }
      else tft.fillRect(leftRight*leftRightOffset, 30+topBottom*topBottomOffset, statusWidth, statusHeight, menuBGcolor);
      strcpy_P(outputHeading, (char*)pgm_read_word(&(timeText[x])));
      tft.setCursor(10+leftRight*leftRightOffset, 32+topBottom*topBottomOffset);
      if(x!=5) tft.println(outputHeading);
      tft.setTextSize(2);
      tft.setCursor(12+leftRight*leftRightOffset, 46+topBottom*topBottomOffset);
      if(x!=5)tft.println(thisTime[x]);
      else tft.println(outputHeading);
      tft.setTextSize(1);
      //reset screen position for drawing to TFT
      if(leftRight==0) leftRight=1;
      else {
        leftRight=0;
        topBottom++;
      }
      tft.setTextColor(ST7735_WHITE);
    }
    cI=Neutral;
    //wait for user to click back
    while(cI==Neutral) cI=getJstk();

    switch (cI) {
      case Down:
        if (thisTime[sI]<=(pgm_read_word_near(minTime + sI))) thisTime[sI]=pgm_read_word_near(maxTime + sI);
        else thisTime[sI]--;
        break;
      case Up:
	if (thisTime[sI]>=pgm_read_word_near(maxTime + sI)) thisTime[sI]=pgm_read_word_near(minTime + sI);
        else thisTime[sI]++;
        break;
      case Left:
        sI--;
        if(sI < 0) sI = 5;
        break;
      case Right:
        sI++;
        if(sI > 5) sI = 0;
        break;
      default:
        if (sI=5) noExit=false;
    }   
  }
  timeOffset=millis();
  writeTimeFile();
}

void writeTimeFile(){
  timeFile.open(timeFileName, O_WRITE | O_CREAT);
  for(int x=0;x<7;x++) timeFile.print(String(String(thisTime[x])+","));
  timeFile.println("X");
  timeFile.close();
}

void drawBack(){  //draw back button at bottom
  tft.setTextColor(ST7735_BLACK);
  tft.fillRect(0, 146, 128, 15, hiLitecolor);
  tft.setCursor(52, 149);
  tft.println("Back");
  tft.setTextColor(ST7735_WHITE);
}

void statScreen(){
  drawHeader();
  drawBack();
  sI=0;
  int inputValue;
  int sensorValue;
  boolean flipColor = true;
  boolean alarmActive = false; //if any input is alarmin
  boolean inputAlarm = false; //if the current input is alarming
  boolean alarmFlip = false;
  int statusWidth = 63;
  int statusHeight = 39;
  int leftRight = 0;
  int topBottom = 0;
  int leftRightOffset = 65;
  int topBottomOffset = 40;
  int cI=Neutral;
  
  //wait for user to click back
  while(cI==Neutral){
    alarmActive = false;
    topBottom=0;
    leftRight=0;
    
    //open file 
    dataFile.open(filename, O_WRITE | O_CREAT | O_APPEND);
    dataFile.println();
    dataFile.print(timeString());
    //read in data
    for (int x=0;x<analogSensors;x++){
      inputValue = analogRead(analogInputs[x]);
      sensorValue = (inputValue/inputDiv[x])*inputActive[x];
      //check for alarm
      if(sensorValue<inputMinAlarm[x] || sensorValue>inputMaxAlarm[x]) {
        inputAlarm = true;
        alarmActive = true;
      }
      else inputAlarm = false; 
      //draw color boxes
      if(!inputAlarm) {
        tft.fillRect(leftRight*leftRightOffset, 26+topBottom*topBottomOffset, statusWidth, statusHeight, statOKcolor);
        tft.setTextColor(ST7735_BLACK);
      }
      else if(alarmFlip) tft.fillRect(leftRight*leftRightOffset, 26+topBottom*topBottomOffset, statusWidth, statusHeight, statBADcolor);
      else {
        tft.fillRect(leftRight*leftRightOffset, 26+topBottom*topBottomOffset, statusWidth, statusHeight, ST7735_BLACK);
        tft.drawRect(leftRight*leftRightOffset, 26+topBottom*topBottomOffset, statusWidth, statusHeight, statBADcolor);
      }
      //draw titles, values, units
      tft.setCursor(8+leftRight*leftRightOffset, 28+topBottom*topBottomOffset);
      for(int z=((9-strlen(inputText[x]))/1.6);z>0;z--)tft.print(" ");
      tft.print(inputText[x]);
      tft.setTextSize(2);
      tft.setCursor(6+leftRight*leftRightOffset, 42+topBottom*topBottomOffset);
      if(sensorValue<100)tft.print(" ");
      tft.print(sensorValue);
      tft.setTextSize(1);
      tft.print(inputUnits[x]);
      //reset screen position for drawing to TFT
      if(leftRight==0) leftRight=1;
      else {
        leftRight=0;
        topBottom++;
      }
      //output to file
      dataFile.print(inputText[x]);
      dataFile.print(":");
      dataFile.print(sensorValue);
      dataFile.print(inputUnits[x]);
      dataFile.print(",");
      tft.setTextColor(ST7735_WHITE);
    }
    dataFile.close(); //close file after write
    //alarm settings
    if(alarmActive&&alarmFlip) buzzNow(true); 
    else alarmFlip = true;

    for(int redrawCount=0;redrawCount<statScrnRedraw;redrawCount++){  //now wait for user input a moment before redrawing
      cI=getJstk();
      if(cI!=Neutral) break;
    }
    if(alarmActive) buzzNow(false);
  }
}

void buzzNow(boolean Loud){
  int mult = 3;
  if(Loud) mult=1;
  for (int i=0; i < 400*mult; i++){
    digitalWrite(alarmOutputs[0], HIGH);
    digitalWrite(alarmOutputs[1], HIGH);
    delayMicroseconds(330/mult);
    digitalWrite(alarmOutputs[0], LOW);
    digitalWrite(alarmOutputs[1], LOW);
    delayMicroseconds(330/mult);
  }
}

void drawMenu(){
  char buffer[12];
  tft.setTextSize(2);
  //draw lines
  for(int x=0;x<menuSize;x++) tft.fillRect(1, headerGap+menuGap*x, 126, 24, menuBGcolor);
  tft.fillRect(2, 1+headerGap+menuGap*sI, 124, 22, hiLitecolor); //highlight current selection
  for(int x=0;x<menuSize;x++){ //write the words
    tft.setCursor(menuTextH, menuTextV+menuGap*x);
    if(x==sI) tft.setTextColor(ST7735_BLACK);
    strcpy_P(buffer, (char*)pgm_read_word(&(menuText[x])));
    tft.println(buffer);
    tft.setTextColor(ST7735_WHITE);
  }
  tft.setTextSize(1);
}

void startupSong()
{
  int melody[]={758,758,758,955,758,638,1276};
  int tempo[]={6, 12, 12, 6, 12, 24, 18};
  long tempoCount;
  long valueC = 25000;
  for (int i=0; i<7; i++) {
    int note = melody[i];
    long tvalue = tempo[i]*valueC;
    delay(10);
    for(tempoCount = 0;tempoCount < tvalue;tempoCount += note) {
      digitalWrite(alarmOutputs[0], HIGH);
      digitalWrite(alarmOutputs[1], HIGH);
      delayMicroseconds(note/2);
      digitalWrite(alarmOutputs[0], LOW);
      digitalWrite(alarmOutputs[1], LOW);
      delayMicroseconds(note/2);
    }
  }
}

// Check the joystick position
int getJstk()
{
  delay(75);
  int jState = analogRead(KEY_PIN);
  if (jState > 650) return Neutral;
  for (int i=0; i < 100; i++){
    digitalWrite(alarmOutputs[0], HIGH);
    digitalWrite(alarmOutputs[1], HIGH);
    delayMicroseconds(100);
    digitalWrite(alarmOutputs[0], LOW);
    digitalWrite(alarmOutputs[1], LOW);
    delayMicroseconds(100);
  }
  if (jState < 50) return Down;
  if (jState < 150) return Right;
  if (jState < 250) return Press;
  if (jState < 500) return Up;
  if (jState < 650) return Left;

}


