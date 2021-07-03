//####################################################################################################################
// Author: Aask & RaigaHomsar
// Creation Date: 05/04/2019
// Last Updated: 6/30/2021
//
// Preface: This is the main library for the DEF-CELL: WiFi Internet of Batteries for DEF CON 27 & 28;
//
// This file contains everything needed to run the basic functions on the DEF CELL, as well as some other fun tools!
//####################################################################################################################

#include "app.h"
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include "boi/boi_wifi.h"
#include "boi/boi.h"
#include "messages.h"
#include "leds.h"

// Operating frequency of ESP32 set to 80MHz from default 240Hz in platformio.ini

// Initialize ESP Global variables
float current_sensed; // This should be read out of Flash each boot, but not yet :)

typedef enum ModeEnum {
  BatteryMode = 0,
  NodeCountMode,
  PartyMode,
  Mode_Count
} ModeEnum;

uint8_t CurLED = 0; // Keep track of what LED is lit
bool SafeBoot = 0;
uint8_t CurrentMode = 0;
uint8_t ModeSelectedLED[Mode_Count] = {LED_TWINKLE, LED_DEFAULT_ALL, LED_BLINKLE};
uint8_t PrevModeSelectedLED;
uint8_t TouchCount = 0;
uint8_t SelectingLED = 0;
uint8_t LastNodeCount = 0;
uint8_t LEDCap = 5;
uint8_t DoublePress_ACT = 0;

boi *Battery; // Create global boi class
boi_wifi *BatteryWifi;
Messages *MessageHandler;
LEDs *LEDHandler;
Preferences *Prefs;

int64_t LastTouchCheckTime = 0;
int64_t LastTouchSetTime = 0;
int64_t LastSensorPrintTime = 0;
int64_t LastButtonPressTime = 0;
int64_t LastButtonPwrHeldTime = 0;
int64_t LastButtonActHeldTime = 0;
int64_t LastButtonBonusHeldTime = 0;
int64_t LastScanTime = 0;
uint8_t CurrentLEDCap = 0;

void SwitchMode();
float safeboot_voltage();

void LEDCallback(const uint16_t ID, bool Finished) {
  if(Finished) { //if a script finished then see if it matches the ID of the mode we are in, if so, restart the script
    if(ModeSelectedLED[CurrentMode] == ID) //if the selected script indicates it completed then make it restart
      LEDHandler->StartScript(ID, 0);
  }
}

void setup(void) {
  uint8_t WifiState;
  float BatteryPower;
  bool DoSafeBoot;
  bool DoFactoryReset;
  Serial.begin(115200); // init system debug output
  Serial.println("Beginning BoI Setup!"); // DEBUG detail

#if BOI_VERSION == 2
  SPIHandler = new SPIParser();
  delay(500); // give time to make sure the other board inits
  SPIHandler->Communicate(&SPIData);
#endif

  //if the action pin is held then go into safe mode
#if BOI_VERSION == 1
  BatteryPower = safeboot_voltage();
  pinMode(BTN_ACT_PIN, INPUT_PULLUP);
  pinMode(BTN_PWR_PIN, INPUT_PULLUP);
  DoSafeBoot = (digitalRead(BTN_ACT_PIN) == 0);
#elif BOI_VERSION == 2
  BatteryPower = SPIData.BatteryVoltage;
  DoSafeBoot = SPIData.BtnPressed[0];
#endif
  if(DoSafeBoot || (BatteryPower < 3.7)) {
    SafeBoot = 1;
    LEDHandler = NewLEDs(LEDCallback, true);
    Serial.println("Safe boot, reboot to disable"); // DEBUG detail
    if(BatteryPower < 3.7) {
      Serial.println("Battery power too low"); // DEBUG detail
    }

    //action button is held, check and see if power button is held
    //if so then we want to do a factory reset
#if BOI_VERSION == 1
  DoFactoryReset = (digitalRead(BTN_PWR_PIN) == 0);
#elif BOI_VERSION == 2
  DoFactoryReset = SPIData.SliderPressed;
  Serial.printf("DoFactoryReset = SPIData.SliderPressed;");
#endif
    if(DoFactoryReset) { 
        // this for loop is a 5s DEBUG notification to allow user to shut down battery if reset wasn't intentional
      for(int i = 5; i > 0; i--) { // cycle LED_100 every 1s for 5s, notification behavior only
        Serial.printf("Doing factory reset in %d second(s), reset to avoid\n", i); // DEBUG detail
        LEDHandler->SetLEDValue(LED_LEV100, 100.0, LED_OVERRIDE_INFINITE);
        delay(500);
        LEDHandler->SetLEDValue(LED_LEV100, 0.0, LED_OVERRIDE_INFINITE);
        delay(500);
      }

      Serial.println("Factory reset activated"); // DEBUG detail

        // No moar warninz; DO THE ACTUAL RESET!
      LEDHandler->SetLEDValue(LED_LEV100, 100.0, LED_OVERRIDE_INFINITE); //turn on LED100 to indicate doing reset (UX behavior decision)
      MessageHandler = NewMessagesHandler();
      MessageHandler->DoFactoryReset();
      LEDHandler->SetLEDValue(LED_LEV100, 50.0, LED_OVERRIDE_INFINITE); //reset done at this point, set half power the led to indicate complete (UX behavior decision)
    }
    return;
  }

  LEDHandler = NewLEDs(LEDCallback, false);
  LEDCap = LED_Count_Battery;
  LEDHandler->SetLEDCap(LEDCap);

  #include "leds-setup.h" //this will call LEDHandler->AddScript() a bunch of times with stuffs

  Prefs = new Preferences();
  Prefs->begin("options");
  Prefs->getBytes("lightshows", ModeSelectedLED, sizeof(ModeSelectedLED));
  WifiState = Prefs->getUChar("wifi");
  Prefs->end();

  Battery = new boi(); // another birth of a boi !!

  // Current Sensing Operation - Check to see if we have anything on the pins 
  Serial.printf("Current Sensed: %d\n", Battery->read_current());
  LastTouchCheckTime = esp_timer_get_time();
  LastSensorPrintTime = LastTouchCheckTime;
  MessageHandler = NewMessagesHandler();
  BatteryWifi = 0;
 
  SwitchMode(); //go to our mode that is set

  BatteryWifi = new boi_wifi(Battery, MessageHandler, boi_wifi::NormalMode);
  if(BatteryWifi->shouldWeEnterSafeModeWithNetworking()) { // Check to see if we're configured for Safe Mode with Networking
      // Set us up for the battery internet! WOO
    delete BatteryWifi;
    BatteryWifi = 0;
    BatteryWifi = new boi_wifi(Battery, MessageHandler, boi_wifi::SafeModeWithNetworking);
  } 
  else if(!MessageHandler->GetOptions()->Configured || WifiState) {
    delete BatteryWifi;
    BatteryWifi = 0;
    Serial.printf("WifiState: %d\n", WifiState);
    BatteryWifi = new boi_wifi(Battery, MessageHandler, (boi_wifi::WifiModeEnum)WifiState); // Set previous wifi state from last boot
  } 
}

#if BOI_VERSION == 1
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_0, ADC_WIDTH_BIT_12, DEFAULT_VREF, &adc_chars);

  uint32_t adc_reading = 0; //Sample ADC Channel
  
  for (int i = 0; i < NO_OF_SAMPLES; i++) { //Multisampling
    adc_reading += adc1_get_raw((adc1_channel_t)ADC_CHANNEL_0);
  }
  adc_reading /= NO_OF_SAMPLES;
  
  uint32_t v_out = esp_adc_cal_raw_to_voltage(adc_reading, &adc_chars); //divide by two per
  float R1 = 1000000.0;
  float R2 = 249000.0;
  float v_source = ((v_out * R1/R2) + v_out)/1000; // Read voltage and then use voltage divider equations to figure out actual voltage read

  return v_source;
}
#elif BOI_VERSION == 2
  float safeboot_voltage() {
    SPIHandler->Communicate(&SPIData);
    return SPIData.BatteryVoltage;
  }
#endif

// do the Loopty Loop ... and your shoes are looking cool ~ !!!
  // refactor tasks
    //most code both bat run
      //some VER_1 runs
      //some VER_2 runs
    //these VER_X runs are dispersed at random within this loop, how do we pull VER_1 and VER_2 code to be each on its own in its own place & called from here
void loop() {
  SensorDataStruct SensorData;
  int8_t NewLEDCap;

#if BOI_VERSION == 1
  uint32_t HeldTime;
#endif

  if(SafeBoot) {
    Serial.printf("In safeboot - battery voltage: %fV\n", safeboot_voltage());
    delay(100);
    esp_sleep_enable_timer_wakeup(2000000);
    esp_light_sleep_start();
    return;
  }

  Battery->get_sensor_data(&SensorData); //get sensor data
    
  if(SensorData.bat_voltage_detected > 0.0) { //if our voltage is not 0 then see if the led's need to be updated
    float VoltagePerStep = (3.9 - 3.6) / 5; //adjust our light limit based on the sensor data
    NewLEDCap = LED_Count_Battery - ((3.9 - SensorData.bat_voltage_detected) / VoltagePerStep); //calculate it
    if(NewLEDCap > LED_Count_Battery) {
      NewLEDCap = LED_Count_Battery;
    }
    else if(NewLEDCap < 0) {
      NewLEDCap = 0;
    }
    
    if((NewLEDCap != CurrentLEDCap) && (CurrentMode == BatteryMode)) { //set the led limit based on battery voltage
      Serial.printf("Voltage: %0.3f, cap: %d\n", SensorData.bat_voltage_detected, NewLEDCap);
      LEDHandler->SetLEDCap(NewLEDCap);
      CurrentLEDCap = NewLEDCap;
    }
  }
    
 
  int64_t CurTime = esp_timer_get_time();
  //if(((CurTime - LastSensorPrintTime) / 1000000ULL) >= 3) { // set print sensor data cadence to every 3 seconds
    //LastSensorPrintTime = CurTime;
    //Battery->print_sensor_data();
  //}

    // Check to see if a button was pressed or other event triggered
  if(Battery->button_pressed(boi::BTN_ACT)) {
    Serial.println("Saw button BTN_ACT"); // DEBUG detail
      //determine if this was a double click
    if((CurTime - LastButtonPressTime) <= 400000ULL) { // check if last btn press is within 250ms, if so, count it
      DoublePress_ACT += 1;
    }
    LastButtonPressTime = CurTime;
  }

    //if ACTION button double pressed then swap the captive portal otherwise swap led options
  if(DoublePress_ACT && (CurTime - LastButtonPressTime) > 500000ULL) {
    printf("DoublePress: %d\n", DoublePress_ACT);
    if(BatteryWifi) {
        if(BatteryWifi->  SafeModeWithNetworking) {
          LEDHandler->StopScript(LED_SMWN_ACTIVE);
          LEDHandler->StartScript(LED_SMWN_DEACTIVATE,1);
          LEDHandler->StartScript(LED_TWINKLE,1);
        }
        delete BatteryWifi;
        BatteryWifi = 0;

        Prefs->begin("options");
        Prefs->putBool("wifi", false); //indicate wifi is off 
        Prefs->end();
      }
    else if(DoublePress_ACT == 2) {
      BatteryWifi = new boi_wifi(Battery, MessageHandler, boi_wifi::BusinessCardMode);
    }
    else if(DoublePress_ACT == 3) {
      BatteryWifi = new boi_wifi(Battery, MessageHandler, boi_wifi::SafeModeWithNetworking);
    }
    else {
      BatteryWifi = new boi_wifi(Battery, MessageHandler, boi_wifi::NormalMode);
    }
    LastButtonPressTime = 0;
    DoublePress_ACT = 0;
  }
  else if(LastButtonPressTime && ((CurTime - LastButtonPressTime) > 500000ULL)) { //button was pressed in the past and was more than a half a second ago, do a light entry
    LastButtonPressTime = 0;
    DoublePress_ACT = 0;
    
    CurrentMode = (CurrentMode + 1) % 3; // Only trigger on the first time around
    Serial.printf("BTN_ACT pressed, mode %d\n", CurrentMode);
    TouchCount = 0;
    SelectingLED = 0;

    SwitchMode(); //flash briefly that we are selecting a mode
  }
  else if(Battery->button_pressed(boi::BTN_PWR)) {
    Serial.println("Toggling Backpower if possible!");
    Battery->toggle_backpower(); // Attempt to toggle backpower
  }
#if BOI_VERSION == 1
  else if(Battery->button_pressed(boi::BTN_BONUS)) {
    // Only trigger on the first time around
    Serial.println("BONUS button pressed!"); // DEBUG detail
    LastButtonPressTime = 0;
  }

  //check for LED brightness increase
  if(MessageHandler->GetOptions()->BrightnessButtons) {
    float BrightnessPercent;
    HeldTime = Battery->button_held(boi::BTN_ACT);
    if(HeldTime) {
      if(!LastButtonActHeldTime) {
        LastButtonActHeldTime = HeldTime;
      }
      else {
        //action button is held, increment brightness as it is held
        //increment every 100ms
        if((HeldTime - LastButtonActHeldTime) >= 100) {
          LastButtonActHeldTime = HeldTime;
          BrightnessPercent = LEDHandler->GetLEDBrightness();
          BrightnessPercent += 0.01;
          LEDHandler->SetLEDBrightness(BrightnessPercent);
        }
      }
    }
    else {
      //if we had a time and it is greater than 100ms we need to save the new option value
      if(LastButtonActHeldTime && ((HeldTime - LastButtonActHeldTime) >= 100)) {
        MessageHandler->BrightnessModified();
      } 
      LastButtonActHeldTime = 0;
    }

    //check for LED brightness decrease
    HeldTime = Battery->button_held(boi::BTN_PWR);
    if(HeldTime) {
      if(!LastButtonPwrHeldTime)
        LastButtonPwrHeldTime = HeldTime;
      else {
        //action button is held, increment brightness as it is held
        //increment every 100ms
        if((HeldTime - LastButtonPwrHeldTime) >= 100)
        {
          LastButtonPwrHeldTime = HeldTime;
          BrightnessPercent = LEDHandler->GetLEDBrightness();
          BrightnessPercent -= 0.01;
          LEDHandler->SetLEDBrightness(BrightnessPercent);
        }
      }
    }
    else {
      //if we had a time and it is greater than 100ms we need to save the new option value
      if(LastButtonPwrHeldTime && ((HeldTime - LastButtonPwrHeldTime) >= 100)) {
        MessageHandler->BrightnessModified();
      }
      LastButtonPwrHeldTime = 0;
    }
  }

  HeldTime = Battery->button_held(boi::BTN_BONUS); //check for bonus button held
  if(HeldTime) {
    if(!LastButtonBonusHeldTime) {
      LastButtonBonusHeldTime = HeldTime;
    LEDHandler->StartScript(LED_CLASSIC_BONUS, 1); //trigger action for bonus held
    }
  }
  else {
    if(LastButtonBonusHeldTime) {
      LEDHandler->StopScript(LED_CLASSIC_BONUS);
      LastButtonBonusHeldTime = 0;
    }
  }

  if((CurTime - LastTouchCheckTime) > 500000ULL) { //check for the touch sensor every half a second
    LastTouchCheckTime = CurTime;
    byte touch = touchRead(BTN_DEFLOCK_PIN);
    if(touch < 61) {
      Serial.print(touch);
      if(TouchCount >= 6) { //if held for 3 seconds then indicate we are selecting a led mode
        if(!SelectingLED) { //flash briefly that we are setting the default for this mode
          SelectingLED = 1;
          LEDHandler->SetLEDCap(LED_Count_Battery);
          PrevModeSelectedLED = ModeSelectedLED[CurrentMode];
        }
      }
      else {
        TouchCount++;
      }

      if(SelectingLED) { //even if the value is reset, if we are still in select mode then set it
        Serial.printf("Selecting new LED for mode %d\n", CurrentMode); // DEBUG detail
        LEDHandler->StopScript(ModeSelectedLED[CurrentMode]); //stop the old script and start the new one
        ModeSelectedLED[CurrentMode] = (ModeSelectedLED[CurrentMode] + 1) % LED_SCRIPT_COUNT;
        LEDHandler->StartScript(ModeSelectedLED[CurrentMode], 0);
        LastTouchSetTime = CurTime;

        if(SelectingLED == 1) //if the first time then do our flash animation on LED 100
        {
          LEDHandler->StartScript(LED_LED_100_FLASH, 1);
          SelectingLED = 2;
        }
      }
    }else {
      TouchCount = 0;
      if(SelectingLED && (CurTime - LastTouchSetTime) >= 10000000) { //if it's been 10 seconds flash and save the setting
        SelectingLED = 0;
        LastTouchSetTime = 0;

        LEDHandler->StartScript(LED_LED_100_FLASH, 1); //flash briefly that we are setting the default for this mode

        Prefs->begin("options");
        Prefs->putBytes("lightshows", ModeSelectedLED, sizeof(ModeSelectedLED)); //save the value
        Prefs->end();
        
        LEDHandler->SetLEDCap(LEDCap); //re-establish our cap as it might have changed depending on the mode
      }
    }
  }
#endif

  if((CurrentMode == NodeCountMode) || BatteryWifi) {
    if((CurTime - LastScanTime) >= 5000000ULL) { //if we are in nodecount or wifi is active then do a scan every 5 seconds
      LastScanTime = CurTime;
      MessageHandler->DoScan();
    }
  }
  
  if(CurrentMode == NodeCountMode) { //if our mode is NodeCount then see if we have a new count to set
    uint8_t CurNodeCount = MessageHandler->GetPingCount();
    if(LastNodeCount != CurNodeCount) {
      LastNodeCount = CurNodeCount;
      if(CurNodeCount > LED_Count_Battery) {
        CurNodeCount = LED_Count_Battery;
      }
      LEDCap = CurNodeCount;
      if(!SelectingLED) //set cap then update if not selecting a lightshow
        LEDHandler->SetLEDCap(CurNodeCount);
    }
  }
  yield();
  delay(50);
} // end loop()

void SwitchMode() {
  if(SelectingLED) { //if they were in the middle of selecting an led then reset it
    SelectingLED = 0;
    ModeSelectedLED[CurrentMode] = PrevModeSelectedLED;
  }
  switch(CurrentMode) {
    case BatteryMode:
      LEDHandler->StopScript(LED_PARTY_MODE); //stop party mode
        //stop the previously selected script and start ours
      LEDHandler->StopScript(ModeSelectedLED[Mode_Count - 1]);
      LEDHandler->StartScript(ModeSelectedLED[BatteryMode], 0);

        //set a high time so it will take a long time to expire
      LEDHandler->SetLEDValue(LED_S_BATT, 100.0, 0xffffffff);
      LEDHandler->SetLEDValue(LED_S_NODE, 0.0, 0xffffffff);
      break;

    case NodeCountMode:
        //stop the previously selected script and start ours
      LEDHandler->StopScript(ModeSelectedLED[NodeCountMode - 1]);
      LEDHandler->StartScript(ModeSelectedLED[NodeCountMode], 0);

        //set a high time so it will take a long time to expire
      LEDHandler->SetLEDValue(LED_S_BATT, 0.0, 0xffffffff);
      LEDHandler->SetLEDValue(LED_S_NODE, 100.0, 0xffffffff);

      LEDCap = 0;
      LEDHandler->SetLEDCap(LEDCap);
      MessageHandler->DoScan(); //kick off a scan
      break;

    case PartyMode:
        //quickly set the values to 0 with no time so it expires
      LEDHandler->SetLEDValue(LED_S_BATT, 0.0, 0);
      LEDHandler->SetLEDValue(LED_S_NODE, 0.0, 0);

        //stop the previously selected script and start ours
      LEDHandler->StopScript(ModeSelectedLED[PartyMode - 1]);
      LEDHandler->StartScript(ModeSelectedLED[PartyMode], 0);

      LEDHandler->StartScript(LED_PARTY_MODE, 1); //start the bounce led script for it
      
      LEDCap = LED_Count_Battery;
      LEDHandler->SetLEDCap(LEDCap); //set the cap to 5 for party mode
      break;
  }
}
