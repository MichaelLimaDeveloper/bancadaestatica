#include <HX711_ADC.h>
#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif
#include <SPI.h>
#include <SD.h>

//pins:
const int HX711_dout = 4; //mcu > HX711 dout pin
const int HX711_sck = 5; //mcu > HX711 sck pin

//SD card pins
const int CS = 21;

File myFile;

//HX711 constructor:
HX711_ADC LoadCell(HX711_dout, HX711_sck);

const int calVal_eepromAdress = 0;
unsigned long t = 0;

void setup() {
  Serial.begin(57600); delay(10);
  Serial.println();
  logToSD("\nStarting...");

  LoadCell.begin();
  //LoadCell.setReverseOutput(); //uncomment to turn a negative output value to positive
  unsigned long stabilizingtime = 2000; // precision right after power-up can be improved by adding a few seconds of stabilizing time
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    logToSD("Timeout, check MCU>HX711 wiring and pin designations");
    while (1);
  }
  else {
    LoadCell.setCalFactor(1.0); // user set calibration value (float), initial value 1.0 may be used for this sketch
    Serial.println("Startup is complete");
    logToSD("Startup is complete");
  }
  while (!LoadCell.update());
  calibrate(); //start calibration procedure

  // Initialize SD card
  Serial.println("Initializing SD card...");
  logToSD("Initializing SD card...");
  if (!SD.begin(CS)) {
    Serial.println("Initialization failed!");
    logToSD("Initialization failed!");
    return;
  }
  Serial.println("Initialization done.");
  logToSD("Initialization done.");
  myFile = SD.open("/log.txt", FILE_WRITE);
  if (myFile) {
    myFile.println("Log Start");
    myFile.close();
  }
}

void loop() {
  static boolean newDataReady = 0;
  const int serialPrintInterval = 0; //increase value to slow down serial print activity

  // check for new data/start next conversion:
  if (LoadCell.update()) newDataReady = true;

  // get smoothed value from the dataset:
  if (newDataReady) {
    if (millis() > t + serialPrintInterval) {
      float i = LoadCell.getData();
      String message = "Load_cell output val: " + String(i);
      Serial.println(message);
      logToSD(message);
      newDataReady = 0;
      t = millis();
    }
  }

  // receive command from serial terminal
  if (Serial.available() > 0) {
    char inByte = Serial.read();
    if (inByte == 't') LoadCell.tareNoDelay(); //tare
    else if (inByte == 'r') calibrate(); //calibrate
    else if (inByte == 'c') changeSavedCalFactor(); //edit calibration value manually
  }

  // check if last tare operation is complete
  if (LoadCell.getTareStatus() == true) {
    Serial.println("Tare complete");
    logToSD("Tare complete");
  }
}

void calibrate() {
  logToSD("***\nStart calibration:\nPlace the load cell on a level stable surface.\nRemove any load applied to the load cell.\nSend 't' from serial monitor to set the tare offset.");

  boolean _resume = false;
  while (_resume == false) {
    LoadCell.update();
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 't') LoadCell.tareNoDelay();
    }
    if (LoadCell.getTareStatus() == true) {
      Serial.println("Tare complete");
      logToSD("Tare complete");
      _resume = true;
    }
  }

  logToSD("Now, place your known mass on the load cell.\nThen send the weight of this mass (i.e. 100.0) from serial monitor.");

  float known_mass = 0;
  _resume = false;
  while (_resume == false) {
    LoadCell.update();
    if (Serial.available() > 0) {
      known_mass = Serial.parseFloat();
      if (known_mass != 0) {
        String message = "Known mass is: " + String(known_mass);
        Serial.println(message);
        logToSD(message);
        _resume = true;
      }
    }
  }

  LoadCell.refreshDataSet(); //refresh the dataset to be sure that the known mass is measured correctly
  float newCalibrationValue = LoadCell.getNewCalibration(known_mass); //get the new calibration value

  String message = "New calibration value has been set to: " + String(newCalibrationValue) + ", use this as calibration value (calFactor) in your project sketch.\nSave this value to EEPROM address " + String(calVal_eepromAdress) + "? y/n";
  Serial.println(message);
  logToSD(message);

  _resume = false;
  while (_resume == false) {
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 'y') {
#if defined(ESP8266)|| defined(ESP32)
        EEPROM.begin(512);
#endif
        EEPROM.put(calVal_eepromAdress, newCalibrationValue);
#if defined(ESP8266)|| defined(ESP32)
        EEPROM.commit();
#endif
        EEPROM.get(calVal_eepromAdress, newCalibrationValue);
        message = "Value " + String(newCalibrationValue) + " saved to EEPROM address: " + String(calVal_eepromAdress);
        Serial.println(message);
        logToSD(message);
        _resume = true;
      }
      else if (inByte == 'n') {
        Serial.println("Value not saved to EEPROM");
        logToSD("Value not saved to EEPROM");
        _resume = true;
      }
    }
  }

  logToSD("End calibration\n***\nTo re-calibrate, send 'r' from serial monitor.\nFor manual edit of the calibration value, send 'c' from serial monitor.\n***");
}

void changeSavedCalFactor() {
  float oldCalibrationValue = LoadCell.getCalFactor();
  boolean _resume = false;
  String message = "***\nCurrent value is: " + String(oldCalibrationValue) + "\nNow, send the new value from serial monitor, i.e. 696.0";
  Serial.println(message);
  logToSD(message);
  float newCalibrationValue;
  while (_resume == false) {
    if (Serial.available() > 0) {
      newCalibrationValue = Serial.parseFloat();
      if (newCalibrationValue != 0) {
        message = "New calibration value is: " + String(newCalibrationValue);
        Serial.println(message);
        logToSD(message);
        LoadCell.setCalFactor(newCalibrationValue);
        _resume = true;
      }
    }
  }
  _resume = false;
  message = "Save this value to EEPROM address " + String(calVal_eepromAdress) + "? y/n";
  Serial.println(message);
  logToSD(message);
  while (_resume == false) {
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 'y') {
#if defined(ESP8266)|| defined(ESP32)
        EEPROM.begin(512);
#endif
        EEPROM.put(calVal_eepromAdress, newCalibrationValue);
#if defined(ESP8266)|| defined(ESP32)
        EEPROM.commit();
#endif
        EEPROM.get(calVal_eepromAdress, newCalibrationValue);
        message = "Value " + String(newCalibrationValue) + " saved to EEPROM address: " + String(calVal_eepromAdress);
        Serial.println(message);
        logToSD(message);
        _resume = true;
      }
      else if (inByte == 'n') {
        Serial.println("Value not saved to EEPROM");
        logToSD("Value not saved to EEPROM");
        _resume = true;
      }
    }
  }
  logToSD("End change calibration value\n***");
}

void logToSD(String message) {
  myFile = SD.open("/log.txt", FILE_WRITE);
  if (myFile) {
    myFile.println(message);
    myFile.close();
  } else {
    Serial.println("Error opening log.txt");
  }
}

void logToSD(float value) {
  myFile = SD.open("/log.txt", FILE_WRITE);
  if (myFile) {
    myFile.print("Load_cell output val: ");
    myFile.println(value);
    myFile.close();
  } else {
    Serial.println("Error opening log.txt");
  }
}
