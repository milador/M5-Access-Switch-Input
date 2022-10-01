/** ************************************************************************
 * File Name: M5_Access_Switch_Input_Software.ino 
 * Title: M5 Switch Input Interface Software
 * Developed by: Milador
 * Version Number: 1.0 (24/9/2022)
 * Github Link: https://github.com/milador/M5-Access-Switch-Input
 ***************************************************************************/

#include <EEPROM.h>
#include <BleKeyboard.h>
#include "EasyMorse.h"
#include <StopWatch.h>
#include <M5StickC.h>

//***CAN BE CHANGED***/
#define MORSE_TIMEOUT                 1000                //Maximum timeout (1000ms =1s)
#define MORSE_REACTION_TIME           10                  //Minimum time for a dot or dash switch action ( level 10 : (1.5^1)x10 =15ms , level 1 : (1.5^10)x10=570ms )
#define SWITCH_REACTION_TIME          50                  //Minimum time for each switch action ( level 10 : 1x50 =50ms , level 1 : 10x50=500ms )
#define SWITCH_MODE_CHANGE_TIME       4000                //How long to hold switch 4 to change mode 
#define SWITCH_DEFAULT_MODE           1
#define SWITCH_DEFAULT_REACTION_LEVEL 6
#define TO_SLEEP_TIME                 60                  //Go to sleep if no data was sent in 60 seconds
#define TO_WAKE_TIME                  180                 //Wake up every 3 minutes 
#define S_TO_MS_FACTOR                1000
#define US_TO_S_FACTOR                1000000     

//***DO NOT CHANGE***//
#define UPDATE_SWITCH_DELAY            200                 //100ms
#define EEPROM_SIZE                    3                   //Define the number of bytes you want to access


//***EEPROM MEMORY***// - DO NOT CHANGE
#define EEPROM_isConfigured         0                       // int:0; 255 on fresh Arduino
#define EEPROM_modeNumber           1                       // int:1; 
#define EEPROM_reactionLevel        2                       // int:2; 

#define SWITCH_A_PIN 26
#define SWITCH_B_PIN 25

// Variable Declaration

String g_switchMessage;        //Custom message 

//Declare switch state variables for each switch
bool g_switchAState,g_switchBState;

int g_pageNumber;

//Stopwatches array used to time switch presses
StopWatch g_timeWatcher[3];
StopWatch g_switchATimeWatcher[1];

//Initialize time variables for morse code
unsigned msMin = MS_MIN_DD;
unsigned msMax = MS_MAX_DD;
unsigned msEnd = MS_END;
unsigned msClear = MS_CL;

//Declare Switch variables for settings 
int g_switchIsConfigured;
int g_switchReactionLevel;
int g_switchMode;

int g_switchReactionTime;
int g_morseReactionTime;

 //Mode structure 
typedef struct { 
  uint8_t modeNumber;
  String modeName;
  uint16_t modeColor;
} modeStruct;


 //Switch structure 
typedef struct { 
  uint8_t switchNumber;
  uint8_t switchChar;
  uint8_t switchMacChar;
  String  switchMacText;
  String  switchMorseText;
  String switchSettingsText;
  uint8_t switchMorseTimeMulti;
} switchStruct;



//Switch properties 
const switchStruct switchProperty[] {
    {1,'a',KEY_F1,"F1","DOT","- Reaction",1},                             //{1=dot,"DOT",'a',  KEY_F1,5=blue,1=1xMORSE_REACTION}
    {2,'b',KEY_F2,"F2","DASH","+ Reaction",3}                              //{2=dash,"DASH",'b',KEY_F2,6=red,3=3xMORSE_REACTION}
};


//Mode properties 
const modeStruct modeProperty[] {
    {1,"Switch",WHITE},
    {2,"Switch Mac",PINK},
    {3,"Morse Keyboard",RED},
    {4,"Settings",YELLOW}
};


unsigned long currentTime = 0, switchAPressTime = 0, switchBPressTime = 0;

EasyMorse morse;
//BleKeyboard bleKeyboard;
BleKeyboard bleKeyboard("M5SwitchInput", "Me", 100);

void setup() {

  Serial.begin(115200);                                                        //Starts Serial
  M5.begin();                                                                  //Starts M5Stack
  bleKeyboard.setName("M5SwitchInput");                                        //Set name of BLE keyboard device 
  bleKeyboard.begin();                                                         //Starts BLE keyboard emulation
  EEPROM.begin(EEPROM_SIZE);                                                   //Starts EEPROM emulation
  delay(1000);
  switchSetup();                                                               //Setup switch
  delay(5);
  showIntro();                                                                 //Show intro page
  delay(5);   
  morseSetup();                                                                //Setup morse
  delay(5);   
  showMode(); 
  delay(5);
  initBatterySaver();
};

void loop() {

  batterySaver();
  //Perform input action based on page number 
  switch (g_pageNumber) {
    case 0:
      introLoop();
      break;
    case 1:
      modeLoop();
      break;
    default:
      // statements
      break;
  }

  delay(5);
  
}


//***INITIALIZE BATTERY SAVER FUNCTION***//
void initBatterySaver() {

  //Wake-up using Switch A and Switch B
  pinMode(GPIO_NUM_26, INPUT_PULLUP);
  esp_sleep_enable_ext1_wakeup(BIT64(GPIO_NUM_26), ESP_EXT1_WAKEUP_ALL_LOW);
  //Wake-up using Button A and Button B
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_37,LOW);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_39,LOW);
  currentTime = switchAPressTime = switchBPressTime = millis();
}

//***BATTERY SAVER FUNCTION***//
void batterySaver() {
  currentTime = millis();
  long currentPressTime = max(switchAPressTime,switchBPressTime);
  if (currentTime > (currentPressTime + (TO_SLEEP_TIME * S_TO_MS_FACTOR))) {
    M5.Axp.DeepSleep(SLEEP_SEC(TO_WAKE_TIME));
  }
}

//*** SHOW INTRO PAGE***//
void showIntro() {

  g_pageNumber = 0;                                  //Set intro page number 

  M5.Lcd.setRotation(3);

  M5.Lcd.fillScreen(WHITE);
  delay(200);
  M5.Lcd.fillScreen(RED);
  delay(200);
  M5.Lcd.fillScreen(GREEN);
  delay(200);
  M5.Lcd.fillScreen(BLUE);
  delay(200);
  M5.Lcd.fillScreen(BLACK);
  
  M5.Lcd.setTextColor(WHITE);

  M5.Lcd.setTextSize(2);                        
  M5.Lcd.drawCentreString("Milador.ca",80,20,2);

  M5.Lcd.setTextSize(1);
  M5.Lcd.drawCentreString("M5Stick Switch Input",80,50,1);

  delay(3000);
}

//*** SHOW MODE PAGE***//
void showMode(){

  g_pageNumber = 1;

  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);                      //Black background
  M5.Lcd.drawRect(1, 1, 159, 20, modeProperty[g_switchMode-1].modeColor);
  M5.Lcd.setTextColor(modeProperty[g_switchMode-1].modeColor);
  M5.Lcd.setTextSize(1);
  M5.Lcd.drawCentreString(modeProperty[g_switchMode-1].modeName,80,2,2);
  
  showModeInfo();
  showMessage();
}
//*** SHOW CUSTOM MESSAGE***//
void showMessage() {

  M5.Lcd.setRotation(3);
  M5.Lcd.setTextSize(1);                    // Select the font
  //Display connection status based on code
  M5.Lcd.drawRect(1, 65, 159, 15, BLUE);
  M5.Lcd.setTextColor(BLUE); 
  g_switchMessage = "Switch Input";
  Serial.println(g_switchMessage);
  M5.Lcd.drawCentreString(g_switchMessage,80,69,1);// Display connection state
    
}

//*** SHOW MODE INFO***//
void showModeInfo() {

  String switchAText = "Swich A: ";
  String switchBText = "Swich B: ";
  M5.Lcd.setRotation(3);
  M5.Lcd.drawRect(1, 23, 159, 41, WHITE);
  M5.Lcd.setTextColor(WHITE); 
  M5.Lcd.setTextSize(1);
  if(g_switchMode==1) {
    switchAText.concat((char)switchProperty[0].switchChar);
    switchBText.concat((char)switchProperty[1].switchChar);
  }
  else if(g_switchMode==2) {
    switchAText.concat(switchProperty[0].switchMacText);
    switchBText.concat(switchProperty[1].switchMacText);
  }
  else if(g_switchMode==3) {
    switchAText.concat(switchProperty[0].switchMorseText);
    switchBText.concat(switchProperty[1].switchMorseText);
  }
  else if(g_switchMode==4) {
    switchAText.concat(switchProperty[0].switchSettingsText);
    switchBText.concat(switchProperty[1].switchSettingsText);
  }
  M5.Lcd.drawCentreString(switchAText,80,28,2);
  M5.Lcd.drawCentreString(switchBText,80,43,2);
}

/***INTRO PAGE LOOP***/
void introLoop() {

}


//***MODE PAGE LOOP***//
void modeLoop() {
  //Update 
  M5.update();
  
  //Change mode on M5 Button press release 
  if (M5.BtnA.wasReleased()) {
    changeSwitchMode();
    showMode();  
  } 

  static int ctr;                          //Control variable to set previous status of switches 
  unsigned long timePressed;               //Time that switch one or two are pressed
  unsigned long timeNotPressed;            //Time that switch one or two are not pressed
  static int previousSwitchAState, previousSwitchBState;  //Previous status of switch A and switch B
  
  //Update status of switch inputs
  g_switchAState = digitalRead(SWITCH_A_PIN);
  g_switchBState = digitalRead(SWITCH_B_PIN);

  timePressed = timeNotPressed  = 0;       //reset time counters
  if (!ctr) {                              //Set previous status of switch A 
    previousSwitchAState = HIGH;  
    ctr++;
  }
  //Check if switch A is pressed to change switch mode
  if (g_switchAState == LOW && previousSwitchAState == HIGH) {
     previousSwitchAState = LOW; 
     g_switchATimeWatcher[0].stop();                                //Reset and start the timer         
     g_switchATimeWatcher[0].reset();                                                                        
     g_switchATimeWatcher[0].start(); 
     switchAPressTime = millis();    
  }
  //Check if switch B is pressed
  if (g_switchBState == LOW && previousSwitchBState == HIGH) {
     previousSwitchBState = LOW; 
     switchBPressTime = millis();
  }
  // Switch A was released
  if (g_switchAState == HIGH && previousSwitchAState == LOW) {
    previousSwitchAState = HIGH;
    timePressed = g_switchATimeWatcher[0].elapsed();                //Calculate the time that switch one was pressed 
    g_switchATimeWatcher[0].stop();                                 //Stop the single action (dot/dash) timer and reset
    g_switchATimeWatcher[0].reset();
    //Perform action if the switch has been hold active for specified time
    if (timePressed >= SWITCH_MODE_CHANGE_TIME){
      changeSwitchMode();
      showMode();                                                                
    } else if(g_switchMode==4) {
      settingsAction(LOW,g_switchAState); 
    }
  }
  //Perform actions based on the mode
  switch (g_switchMode) {
      case 1:
        keyboardAction(false,g_switchAState,g_switchBState);                                  //Switch mode
        break;
      case 2:
        keyboardAction(true,g_switchAState,g_switchBState);                                   //Switch Mac mode
        break;
      case 3:
        morseAction(g_switchAState,g_switchBState);                                           //Keyboard Morse mode
        break;
      case 4:
        settingsAction(HIGH,g_switchBState);                                                  //Settings mode
        break;
  };
  delay(g_switchReactionTime);
}

//***SETUP SWITCH MODE FUNCTION***//
void switchSetup() {

  //Initialize the switch pins as inputs
  pinMode(SWITCH_A_PIN, INPUT_PULLUP);    
  pinMode(SWITCH_B_PIN, INPUT_PULLUP);   

  //Check if it's first time running the code
  g_switchIsConfigured = EEPROM.read(EEPROM_isConfigured);
  delay(5); 
  if (g_switchIsConfigured==0) {
    //Define default settings if it's first time running the code
    g_switchIsConfigured=1;
    g_switchMode=SWITCH_DEFAULT_MODE;
    g_switchReactionLevel=SWITCH_DEFAULT_REACTION_LEVEL;

    //Write default settings to EEPROM 
    EEPROM.write(EEPROM_isConfigured,g_switchIsConfigured);
    EEPROM.write(EEPROM_modeNumber,g_switchMode);
    EEPROM.write(EEPROM_reactionLevel,g_switchReactionLevel);
    EEPROM.commit();
    delay(5);
  } else {
    //Load settings from flash storage if it's not the first time running the code
    g_switchMode = EEPROM.read(EEPROM_modeNumber);
    g_switchReactionLevel = EEPROM.read(EEPROM_reactionLevel);
    delay(5);
  }  

    //Calculate switch delay based on g_switchReactionLevel
    g_switchReactionTime = ((11-g_switchReactionLevel)*SWITCH_REACTION_TIME);
    g_morseReactionTime = (pow(1.5,(11-g_switchReactionLevel))*MORSE_REACTION_TIME);
    
    //Serial print settings 
    Serial.print("Switch Mode: ");
    Serial.println(g_switchMode);

    Serial.print("Switch Reaction Level: ");
    Serial.println(g_switchReactionLevel);
    Serial.print("Reaction Time(ms): ");
    Serial.print(g_switchReactionTime);
    Serial.print("-");
    Serial.println(g_morseReactionTime);   

}


//***SETUP MORSE FUNCTION***//
void morseSetup() {
    morse.clear();
    msMin = g_morseReactionTime;
    msMax = msEnd = msClear = MORSE_TIMEOUT; 

}

//***ADAPTIVE SWITCH KEYBOARD FUNCTION***//
void keyboardAction(bool macMode,int switchA,int switchB) {
    if(!switchA) {
      //Windows,Android: a, Mac: F1
      (macMode) ? bleKeyboard.write(switchProperty[0].switchMacChar) : bleKeyboard.write(switchProperty[0].switchChar);
    } else if(!switchB) {
      //Windows,Android: b, Mac: F2
      (macMode) ? bleKeyboard.write(switchProperty[1].switchMacChar) : bleKeyboard.write(switchProperty[1].switchChar);
    } else
    {
      bleKeyboard.releaseAll();
    }
    delay(5);

}

//***MORSE CODE TO MOUSE CONVERT FUNCTION***//
void morseAction(int switch1,int switch2) {
  int i;
  static int ctr;
  unsigned long timePressed;
  unsigned long timeNotPressed;
  static int previousSwitch1;
  static int previousSwitch2;

  int isShown;                                                                                  //is character shown yet?
  int backspaceDone = 0;                                                                        //is backspace entered? 0 = no, 1=yes

  timePressed = timeNotPressed  = 0; 

  if (!ctr) {
    previousSwitch1 = LOW;  
    previousSwitch2 = LOW;  
    ctr++;
  }
   //Check if dot or dash switch is pressed
   if ((switch1 == LOW && previousSwitch1 == HIGH) || (switch2 == LOW && previousSwitch2 == HIGH)) {
      //Dot
     if (switch1 == LOW) { 
         //switchFeedback(1,mode,morseReactionTime,1);                                            //Blink blue once
         previousSwitch1 = LOW;
     } //Dash
     if (switch2 == LOW) {
         //switchFeedback(2,mode,morseReactionTime,1);                                            //Blink red once
         previousSwitch2 = LOW;
     }
     isShown = 0;                                                                               //A key is pressed but not Shown yet
     for (i=0; i<3; i++) g_timeWatcher[i].stop();                                                          //Stop end of char
     
     g_timeWatcher[0].reset();                                                                             //Reset and start the the timer
     g_timeWatcher[0].start();                                                                             
   }

 
 // Switch 1 or Dot was released
 if (switch1 == HIGH && previousSwitch1 == LOW) {
   previousSwitch1 = HIGH;
   
   backspaceDone = 0;                                                                           //Backspace is not entered 
   timePressed = g_timeWatcher[0].elapsed();                                                               //Calculate the time that key was pressed 
   for (i=0; i<3; i++) g_timeWatcher[i].stop();                                                            //Stop end of character and reset
   for (i=0; i<3; i++) g_timeWatcher[i].reset();                                                           
   g_timeWatcher[1].start();

  //Push dot to morse stack if it's pressed for specified time
    if (timePressed >= msMin && timePressed < msMax) {
      morse.push(1);
      isShown = 0; 
    }
 }

   //Switch 2 or Dash was released
   if (switch2 == HIGH && previousSwitch2 == LOW) {

    previousSwitch2 = HIGH;
    backspaceDone = 0;                                                                          //Backspace is not entered 
    timePressed = g_timeWatcher[0].elapsed();                                                              //Calculate the time that key was pressed 
    for (i=0; i<3; i++) g_timeWatcher[i].stop();                                                           //Stop end of character and reset
    for (i=0; i<3; i++) g_timeWatcher[i].reset();
    g_timeWatcher[1].start();
   
    //Push dash to morse stack if it's pressed for specified time
    if (timePressed >= msMin && timePressed < msMax) {
      morse.push(2);
      isShown = 0;       
    }
   }

 // Processing the backspace key if button 1 or button 2 are hold and pressed for defined time
  if (timePressed >= msMax && timePressed >= msClear && backspaceDone == 0) {
    previousSwitch1 = HIGH;
    previousSwitch2 = HIGH;
    bleKeyboard.write(8);                                                               //Press Backspace key
    backspaceDone = 1;                                                                          //Set Backspace done already
    isShown = 1;
    for (i=1; i<3; i++) { g_timeWatcher[i].stop(); g_timeWatcher[i].reset(); }                      //Stop and reset end of character
 }

   //End the character if nothing pressed for defined time and nothing is shown already 
   timeNotPressed = g_timeWatcher[1].elapsed();
    if (timeNotPressed >= msMax && timeNotPressed >= msEnd && isShown == 0 && backspaceDone == 0) {
       bleKeyboard.write(morse.getCharAscii());                                                  //Enter keyboard key based on ascii code if it's in morse keyboard mode
      
      //Clean up morse code and get ready for next character
      morse.clear();
      isShown = 1;                                                                                //Set variable to is shown                                                                                      
      g_timeWatcher[1].stop();                                                                               //Stop and reset the timer to form a character
      g_timeWatcher[1].reset();
  }
  
}

//***CHANGE SWITCH MODE FUNCTION***//
void changeSwitchMode(){
    //Update switch mode
    g_switchMode++;
    if (g_switchMode == (sizeof (modeProperty) / sizeof (modeProperty[0]))+1) {
      g_switchMode=1;
    } 
    else {
    }

    morseSetup();
    
    //Serial print switch mode
    Serial.print("Switch Mode: ");
    Serial.println(g_switchMode);
    
    //Save switch mode
    EEPROM.write(EEPROM_modeNumber, g_switchMode);
    EEPROM.commit();
    delay(25);
}

//***CONFIGURATION MODE ACTIONS FUNCTION***//
void settingsAction(int switchA,int switchB) {
  if(switchA==LOW) {
    decreaseReactionLevel();
  }
  if(switchB==LOW) {
    increaseReactionLevel();
  }
}

//***INCREASE SWITCH REACTION LEVEL FUNCTION***//
void increaseReactionLevel(void) {
  g_switchReactionLevel++;
  if (g_switchReactionLevel == 11) {
    g_switchReactionLevel = 10;
  } else {
    g_switchReactionTime = ((11-g_switchReactionLevel)*SWITCH_REACTION_TIME);
    g_morseReactionTime = (pow(1.5,(11-g_switchReactionLevel))*MORSE_REACTION_TIME);
    delay(25);
  }
  Serial.print("Reaction level: ");
  Serial.println(g_switchReactionLevel);
  Serial.print("Reaction Time(ms): ");
  Serial.print(g_switchReactionTime);
  Serial.print("-");
  Serial.println(g_morseReactionTime);
  EEPROM.write(EEPROM_reactionLevel, g_switchReactionLevel);
  EEPROM.commit();
  delay(25);
}

//***DECREASE SWITCH REACTION LEVEL FUNCTION***//
void decreaseReactionLevel(void) {
  g_switchReactionLevel--;
  if (g_switchReactionLevel == 0) {
    g_switchReactionLevel = 1; 
  } else {
    g_switchReactionTime = ((11-g_switchReactionLevel)*SWITCH_REACTION_TIME);
    g_morseReactionTime = (pow(1.5,(11-g_switchReactionLevel))*MORSE_REACTION_TIME);
    delay(25);
  } 
  Serial.print("Reaction level: ");
  Serial.println(g_switchReactionLevel);
  Serial.print("Reaction Time(ms): ");
  Serial.print(g_switchReactionTime);
  Serial.print("-");
  Serial.println(g_morseReactionTime);  
  
  EEPROM.write(EEPROM_reactionLevel, g_switchReactionLevel);
  EEPROM.commit();
  delay(25);
}
