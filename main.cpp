#include "Arduino.h"
#include "SPI.h"
#include "DAC_MCP49xx.h"
#include "Adafruit_MCP23017.h"
#include <TM1637Display.h>

// temp clock vars
#define CLOCK_INTERVAL 500
unsigned long clock_previous_millis = 0;
bool startup_init = true;

// button vars
#define BUTTON_INTERVAL 20
uint8_t button_last_state;
bool button_triggered = false;
unsigned long debounce_previous_millis = 0;

// pin vars
#define DAC_CS 0
#define DIS_CLK 14
#define DIS_DATA 15
#define CLOCK_INPUT_PIN 3

// chip vars
DAC_MCP49xx dac_1(DAC_MCP49xx::MCP4902, DAC_CS);
Adafruit_MCP23017 iochip;
TM1637Display display(DIS_CLK, DIS_DATA);

// clock vars
bool triggered = false;
int analog_clock;

// sequence vars
byte segment[8];
byte innerLoop[32];
byte innerLoopLength = 0;
byte innerLoopPosition = 0;
byte outerLoopLength = 63; // testing - original value was 15
byte outerLoopPosition = 0;
byte currentLevel = 12; // testing - original value was 0
bool fuckMeUp = false;
bool newPitches = false;
short pitchStream = 0;

byte globalPitches[4] = {
  B00000000, 
  B00000000, 
  B00000000, 
  B00000000,
};

byte patterns[11][4] = {
  {B10000000, B10000000, B10000000, B10000000}, // >> 3
  {B10000000, B01000000, B01000000, B01000000}, // >> 2
  {B10000000, B10000000, B01000000, B01000000}, // >> 2
  {B10000000, B10000000, B10000000, B01000000}, // >> 2
  {B10000000, B00100000, B00100000, B00100000}, // >> 1
  {B10000000, B10000000, B00100000, B00100000}, // >> 1
  {B10000000, B10000000, B10000000, B00100000}, // >> 1
  {B10000000, B00010000, B00010000, B00010000},
  {B10000000, B10000000, B00010000, B00010000},
  {B10000000, B10000000, B10000000, B00010000},
  {B10000000, B01000000, B00100000, B00010000},
};

// display vars
struct DisplayParams {
  uint8_t buffer_data[4];
  uint8_t buffer_blank[4];
  char screen[9];
  bool triggered;
  byte clock_counter;
};

DisplayParams dparams = {
  { 0xff, 0xff, 0xff, 0xff },
  { 0x00, 0x00, 0x00, 0x00 },
  "loop_pos",
  false,
  0,
};

void setup() {
  randomSeed(analogRead(0));

  iochip.begin();
  for(byte pin = 0; pin <= 15; pin++){
    iochip.pinMode(pin, INPUT);
    iochip.pullUp(pin, HIGH);
  }

  display.setBrightness(0x0f);

  Serial.begin(38400);
}

void randomisePitches() {
    globalPitches[0] = random(0,45);
    globalPitches[1] = random(globalPitches[0], 90);
    globalPitches[2] = random(globalPitches[1], 135);
    globalPitches[3] = random(globalPitches[2], 255);
}

void shufflePitches() {
  byte rnd, _p1, _p2, _p3, _p4;
  short pitches[4];

  for(int i=0;i<=3;i++)
    pitches[i] = globalPitches[i];

  rnd = random(0,3);
  _p1 = rnd;
  while(rnd == _p1) {
    rnd = random(0,3);
  } _p2 = rnd;
  while(rnd == _p2 || rnd == _p1) {
    rnd = random(0,3);
  } _p3 = rnd;
  while(rnd == _p3 || rnd == _p2 || rnd == _p1) {
    rnd = random(0,3);
  } _p4 = rnd;
  globalPitches[0] = pitches[_p1];
  globalPitches[1] = pitches[_p2];
  globalPitches[2] = pitches[_p3];
  globalPitches[3] = pitches[_p4];
}

short manglePattern(short index, bool reverse, bool shift, short shiftby, short pattern){
  short notes = random(0,3);
  
  if(reverse == true){
    for(short i = notes; i >= 0; i--){
      if(shift == true)
        segment[index] = patterns[pattern][i] >> shiftby;
      else 
        segment[index] = patterns[pattern][i];             
      index++;
    }
  } else {
    for(short i = 0; i <= notes; i++){
      if(shift == true)
        segment[index] = patterns[pattern][i] >> shiftby;
      else 
        segment[index] = patterns[pattern][i];             
      index++;            
    }
  }

  return index;
}

short gimmeAPattern() {
  short rest = true;
  short index = 0;
  bool reverse = false;
  bool shift = false;
  short shiftby = 0;
  short pattern = 0;

  if(random(1, 10) <= 4)
    reverse = true;
  
  if(random(1,10) <= 3)
    shift = true;

  if(rest){
    if(random(1, 10) <= 3){
      segment[index] = B00000010;
      index++;
    }
    if(random(1, 10) == 1){
      segment[index] = B00000010;
      index++;
    }
  }

  switch(random(0,10)) {
    case 0:
      pattern = 0;
      shiftby = random(0,3);
      index = manglePattern(index, reverse, shift, shiftby, pattern);
      break;
    case 1:
      pattern = 1;
    case 2:
      pattern = 2;
    case 3:
      pattern = 3;
      shiftby = random(0,2);
      index = manglePattern(index, reverse, shift, shiftby, pattern);
      break;
    case 4:
      pattern = 4;
    case 5:
      pattern = 5;
    case 6:
      pattern = 6;
      shiftby = random(0,1);
      index = manglePattern(index, reverse, shift, shiftby, pattern);
      break;
    case 7:
      pattern = 7;
    case 8:
      pattern = 8;
    case 9:
      pattern = 9;
    case 10:
      pattern = 10;
      shift = false;
      index = manglePattern(index, reverse, shift, shiftby, pattern);
      break;
    default:
      break;
  }

  if(rest){
    if(random(1, 10) <= 3){
      segment[index] = B00000010;
      index++;
    }
    if(random(1, 10) == 1){
      segment[index] = B00000010;
      index++;
    }
  }

  return index;
}

short updateInnerLoop() {
  innerLoopLength = 0;
  short patternLength = 0;
  short numberOfPatterns = 0;

  switch(currentLevel){
    case 4:
    case 5:
    case 6:
    case 7:
      numberOfPatterns = 1;
      break;
    case 8:
    case 9:
    case 10:
    case 11:
      numberOfPatterns = 2;
      break;
    case 12:
    case 13:
    case 14:
    case 15:
      numberOfPatterns = 3;
        break;
    default:
      break;
  }

  for(int i = 0; i <= numberOfPatterns; i++) {
    patternLength = gimmeAPattern();
    patternLength -= 1; // patternLength points to the first empty array element
    for(int i = 0; i <= patternLength; i++){
      innerLoop[innerLoopLength] = segment[i];
      innerLoopLength++;
    }
  }
}

// This is just a test function. Depending on where I go with this project I may need to write a proper button handler
// class, especially if i'm to implement preset load/save functionality
void readInput(){
  uint8_t button_state = iochip.readGPIO(0);
  if(button_state == B11111111 && button_triggered == true){
    Serial.println("button_triggered == false");
    button_triggered = false;
  }

  if(button_state != B11111111){
    unsigned long debounce_millis = millis();
    if(button_state == button_last_state){
      if(debounce_millis - debounce_previous_millis >= BUTTON_INTERVAL && button_triggered == false){
        Serial.println("button_triggered == true");
        button_triggered = true;
        debounce_previous_millis = millis();
        switch(button_state){
          case B11111101:
            Serial.println("Decrease Random");
            break;
          case B11111011:
            Serial.println("Increase Random");
            break;
          case B11110111:
            Serial.println("Randomize Pattern");
            break;
          case B11101111:
            Serial.println("Randomize Pitches");
            break;
          case B11011111:
            Serial.println("Shuffle Pitches");
            break;
          case B10111111:
            Serial.println("Increase Outer Loop");
            break;
          case B01111111:
            Serial.println("Decrease Outer Loop");
            break;
          default:
            Serial.println("Default Case");
            break;
        }
      } 
    } else {
      Serial.println("button_triggered == false");
      button_triggered = false;
      button_last_state = button_state;
    }
  }
  /*
  if(button_state == B11111101)
    Serial.println("Decrease Random");    

  if(button_state == B11111011)
    Serial.println("Increase Random");

  if(button_state == B11110111)
    Serial.println("Randomize Pattern");

  if(button_state == B11101111)
    Serial.println("Randomize Pitches");

  if(button_state == B11011111)
    Serial.println("Shuffle Pitches");

  if(button_state == B10111111)
    Serial.println("Increase Outer Loop");

  if(button_state == B01111111) {
    Serial.println("Decrease Outer Loop");
  }
  */

}

// Invoked by clockPulseIn(), should be decoupled if possible. Event queues would be nice but i don't know if the 328p will 
// have enough resources to handle events well enough for this project
void incrementPosition(){
  if(innerLoopPosition < innerLoopLength)
    innerLoopPosition++;
  else
    innerLoopPosition = 0;

  if(outerLoopPosition < outerLoopLength)
    outerLoopPosition++;
  else {
    outerLoopPosition = 0;
    innerLoopPosition = 0;
  }

}

// If CLOCK_INPUT_PIN has been pulled high, increment the sequencer position by one step. This is pretty yuck and needs to be
// refactored ASAP to work with interrupts
void clockPulseIn(){
  analog_clock = analogRead(CLOCK_INPUT_PIN);

  if(analog_clock >= 512){
    if(triggered == false){
      incrementPosition();
      triggered = true;
      return;
    }
  } else {
    triggered = false;
  }
}

// This function will need to be extended to support portamento/legato, if we have the cycles to spare
void updatePitchStream() {
  if(innerLoop[innerLoopPosition] & B00000010) {
    return;
  }

  if((innerLoop[innerLoopPosition] & B10000000)) {
    pitchStream = globalPitches[0];
    return;
  }

  if((innerLoop[innerLoopPosition] & B01000000)) {
    pitchStream = globalPitches[1];
    return;
  }
  
  if((innerLoop[innerLoopPosition] & B00100000)) {
    pitchStream = globalPitches[2];
    return;
  }
  
  if((innerLoop[innerLoopPosition] & B00010000)) {
    pitchStream = globalPitches[3];
    return;
  }

  return;
}

// Output a trigger pulse
void outputBlip() {
  if(innerLoop[innerLoopPosition] & B00000010 == 1)
    return;
}

// Update the DAC with the current value of pitchStream
void updateDAC() {
  dac_1.outputA(pitchStream);
}

void updateDisplay() {
  dparams.buffer_data[0] = display.encodeDigit(((outerLoopPosition+1)/10)%10);
  dparams.buffer_data[1] = display.encodeDigit((outerLoopPosition+1)%10);
  dparams.buffer_data[2] = display.encodeDigit(((outerLoopLength+1)/10)%10);
  dparams.buffer_data[3] = display.encodeDigit((outerLoopLength+1)%10);
}

void refreshDisplay() {
  display.setSegments(dparams.buffer_data);
}

void loop() {
  if(startup_init == true){
    startup_init = false;
    randomisePitches();
    updateInnerLoop();
    updatePitchStream();
    updateDAC();
    outputBlip();
    updateDisplay();
    /*
    for(int i = 0; i <= innerLoopLength; i++) {
      Serial.println(innerLoop[i]);
    }
    */
  }

  readInput();

  unsigned long clock_current_millis = millis();
  if(clock_current_millis - clock_previous_millis >= CLOCK_INTERVAL) {
    clock_previous_millis = millis();
    incrementPosition();
    updatePitchStream();
    updateDAC();
    outputBlip();
    updateDisplay();
    //Serial.println(pitchStream);
  }
  refreshDisplay();

  // Serial.println(pitchStream);
  /*Serial.println(innerLoopLength);
  Serial.println("---");
  for(int i = 0; i <= innerLoopLength; i++) {
    Serial.println(innerLoop[i]);
  }*/

  // randomisePitches();
  // updateInnerLoop();

}
