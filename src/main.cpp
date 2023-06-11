#include <Arduino.h>
#include <BleKeyboard.h>
//#include <TM1637Display.h>

#define CLK GPIO_NUM_2
#define DIO GPIO_NUM_5

enum NumState {Empty, Incrementing, Incremented, Skipping, Skipped};
const char* numStateNames[] = {"Empty", "Incrementing", "Incremented", "Skipping", "Skipped"};

BleKeyboard bleKeyboard;
//TM1637Display display = TM1637Display(CLK, DIO);

const gpio_num_t buttons[] = {GPIO_NUM_4, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_21};

const int pressTime = 130;
const int roll1Time = 1550;
const int roll10Time = 6300;
const int delayAfterCheck = 300;
const int delayAfterRoll = 1200;

int8_t lock[] = {0, 0, 0, 0};

int lockSize = 0;

bool roll = false;
bool isPause = false;

int selected = 0;

NumState state = Empty;

bool hold = false;
uint32_t pauseTime = 0;

void setup() {
  
  for (int i = 0; i < 4; i++) {
    pinMode(buttons[i], INPUT);
    gpio_set_pull_mode (buttons[i], GPIO_PULLDOWN_ONLY);
  }

  Serial.begin(115200);
  bleKeyboard.begin(); 

  //display.setBrightness(5);
}

byte cl[] = {0,0,0,0};

bool buttonsState[] = {false, false, false, false};
uint32_t buttonsTimers[] = {0, 0, 0, 0};

void readButtons()
{
  for(int i = 0; i < 4; i++) {
    bool btnState = digitalRead(buttons[i]);
    if (btnState && !buttonsState[i] && millis() - buttonsTimers[i] > 100) {

      buttonsState[i] = true;
      buttonsTimers[i] = millis();

      Serial.printf("%d: Button #%d Enable\n", millis(), i + 1);    
    } 
    
    if (!btnState && buttonsState[i] && millis() - buttonsTimers[i] > 100) {
      
      buttonsState[i] = false;
      buttonsTimers[i] = millis();

      Serial.printf("%d: Button #%d Disable\n", millis(), i + 1);      
    }
  }
}

void down(int _delay){
  hold = true;
  isPause = true;
  pauseTime = millis() + _delay;
  Serial.printf("%d: Down F pause: %d\n", millis(), _delay);

  if (bleKeyboard.isConnected())
    bleKeyboard.press(70);
}

void up(int _delay){
  hold = false;
  isPause = true;
  pauseTime = millis() + _delay;
  Serial.printf("%d: Up F pause: %d\n", millis(), _delay);

  if (bleKeyboard.isConnected())
    bleKeyboard.release(70);
}

bool overflow(){
  for (int i = selected + 1; i < lockSize - 1; i++){
    
    if (lock[i] != 9) {

      return false;
    }
  }

  return true;
}

void loop() {
  
  readButtons();

  if (!roll && buttonsState[0]) {
    roll = true;
    lockSize = 3;
    selected = 2;    
  }

  if (!roll && buttonsState[1]) {
    roll = true; 
    lockSize = 4;
    selected = 3;    
  }

  if (!roll && buttonsState[2] && 
      (lock[0] != 0 || lock[1] != 0 || lock[2] != 0 || lock[3] != 0)) {
    selected = 0;
    lock[0] = lock[1] = lock[2] = lock[3] = 0;     
  }

  if (roll && lockSize > 0 && buttonsState[3]) {
    
    state = Empty;
    roll = false;
    selected = lockSize - 1;
    
    if (hold) {
      up(100); 
    }
  }

  if (roll)
  {
    if (isPause)
    {
      if (millis() <= pauseTime)
        return;
      
      isPause = false;
    }

    Serial.printf("%d: lock: %d%d%d%d size: %d selected: %d state: %s\n", millis(), lock[0], lock[1], lock[2], lock[3], lockSize, selected, numStateNames[state]);

    switch (state)
    {
    case Empty:

      if (!hold) {

        if (selected == lockSize - 1) {

          state = Incrementing;
          down(roll10Time);
        } else if (selected == lockSize - 2 || overflow()) {

          state = Incrementing;
          down(roll1Time);

          lock[selected] = (lock[selected] + 1) % 10;                   

        } else {

          state = Incremented;
        }

      }

      break;
    
    case Incrementing:

      if (hold) {

        state = Incremented;
        up(delayAfterRoll);
      }

      break;

    case Incremented:

      if (!hold) {

        state = Skipping;
        down(pressTime);
      }

      break;

    case Skipping:

      if (hold) {

        state = Skipped;
        up(delayAfterCheck);
      }

      break;

    case Skipped:

      if (!hold) {
        
        state = Empty;
        selected = (selected + 1) % lockSize;
      }

      break;

    default:
      break;
    }
  }
}