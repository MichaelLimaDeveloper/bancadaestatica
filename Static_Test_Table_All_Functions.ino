#include <HX711_ADC.h>
#include <SPI.h>
#include <SD.h>
#include <BluetoothSerial.h>

#if defined(ESP8266) || defined(ESP32)
#include <EEPROM.h>
#endif

// Pins
const int HX711_dout = 4;  // mcu > HX711 dout pin
const int HX711_sck = 5;   // mcu > HX711 sck pin
const int CS = 21;         // SD card chip select pin

File myFile;

// HX711 constructor
HX711_ADC LoadCell(HX711_dout, HX711_sck);

const int calVal_eepromAdress = 0;
unsigned long t = 0;

// Bluetooth Serial
BluetoothSerial SerialBT;

void setup() {
  Serial.begin(57600);      // Initialize Serial Monitor
  SerialBT.begin("ESP32_BT"); // Initialize Bluetooth Serial with the name "ESP32_BT"
  
  delay(10);
  Serial.println();
  Serial.println("Starting...");
  SerialBT.println("Starting...");

  LoadCell.begin();
  unsigned long stabilizingtime = 2000;  // Stabilizing time after power-up
  boolean _tare = true;                  // Set this to false if you don't want tare
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    SerialBT.println("Timeout, check MCU>gtHX711 wiring and pin designations");
    while (1);
  } else {
    LoadCell.setCalFactor(1.0);  // Initial calibration value
    Serial.println("Startup is complete");
    SerialBT.println("Startup is complete");
  }
  while (!LoadCell.update());
  calibrate();  // Start calibration procedure

  // Initialize SD card
  Serial.println("Initializing SD card...");
  SerialBT.println("Initializing SD card...");
  if (!SD.begin(CS)) {
    Serial.println("Initialization failed!");
    SerialBT.println("Initialization failed!");
    return;
  }
  Serial.println("Initialization done.");
  SerialBT.println("Initialization done.");

  // Open file for writing
  myFile = SD.open("/log.txt", FILE_WRITE);
  if (myFile) {
    myFile.println("Log Start");
    myFile.close();
  } else {
    Serial.println("Error opening log.txt");
    SerialBT.println("Error opening log.txt");
  }
}

void loop() {
  static boolean newDataReady = 0;
  const int serialPrintInterval = 0;  // Increase value to slow down serial print activity

  // Check for new data/start next conversion
  if (LoadCell.update()) newDataReady = true;

  // Get smoothed value from the dataset
  if (newDataReady) {
    if (millis() > t + serialPrintInterval) {
      float i = LoadCell.getData();
      String output = "Load_cell output val: " + String(i);
      Serial.println(output);
      SerialBT.println(output);  // Print to Bluetooth
      logToSD(output);  // Log load cell value
      newDataReady = 0;
      t = millis();
    }
  }

  // Receive and log command from serial terminal
  if (Serial.available() > 0 || SerialBT.available() > 0) {
    String serialInput = "";
    
    // Read from Serial
    while (Serial.available() > 0) {
      char inByte = Serial.read();
      serialInput += inByte;
    }
    
    // Read from Bluetooth
    while (SerialBT.available() > 0) {
      char inByte = SerialBT.read();
      serialInput += inByte;
    }

    Serial.print("Received data: ");
    Serial.println(serialInput);
    SerialBT.print("Received data: ");
    SerialBT.println(serialInput);
    logToSD("Received data: " + serialInput);  // Log serial data

    // Process serial commands
    processSerialCommand(serialInput);
  }

  // Check if last tare operation is complete
  if (LoadCell.getTareStatus() == true) {
    String tareMessage = "Tare complete";
    Serial.println(tareMessage);
    SerialBT.println(tareMessage);
    logToSD(tareMessage);
  }
}

void calibrate() {
  String message = "***\nStart calibration:\nPlace the load cell on a level stable surface.\nRemove any load applied to the load cell.\nSend 't' from serial monitor to set the tare offset.";
  Serial.println(message);
  SerialBT.println(message);
  logToSD(message);

  boolean _resume = false;
  while (_resume == false) {
    LoadCell.update();
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 't') LoadCell.tareNoDelay();
    }
    if (LoadCell.getTareStatus() == true) {
      String tareComplete = "Tare complete";
      Serial.println(tareComplete);
      SerialBT.println(tareComplete);
      logToSD(tareComplete);
      _resume = true;
    }
  }

  message = "Now, place your known mass on the loadcell.\nThen send the weight of this mass (i.e. 100.0) from serial monitor.";
  Serial.println(message);
  SerialBT.println(message);
  logToSD(message);

  float known_mass = 0;
  _resume = false;
  while (_resume == false) {
    LoadCell.update();
    if (Serial.available() > 0) {
      known_mass = Serial.parseFloat();
      if (known_mass != 0) {
        String knownMassMessage = "Known mass is: " + String(known_mass);
        Serial.println(knownMassMessage);
        SerialBT.println(knownMassMessage);
        logToSD(knownMassMessage);
        _resume = true;
      }
    }
  }

  LoadCell.refreshDataSet();
  float newCalibrationValue = LoadCell.getNewCalibration(known_mass);

  String newCalibrationMessage = "New calibration value has been set to: " + String(newCalibrationValue) + ", use this as calibration value (calFactor) in your project sketch.";
  Serial.println(newCalibrationMessage);
  SerialBT.println(newCalibrationMessage);
  logToSD(newCalibrationMessage);
  
  Serial.print("Save this value to EEPROM address ");
  Serial.print(calVal_eepromAdress);
  Serial.println("? y/n");
  SerialBT.print("Save this value to EEPROM address ");
  SerialBT.print(calVal_eepromAdress);
  SerialBT.println("? y/n");
  logToSD("Save new calibration value to EEPROM address " + String(calVal_eepromAdress) + "? y/n");

  _resume = false;
  while (_resume == false) {
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 'y') {
#if defined(ESP8266) || defined(ESP32)
        EEPROM.begin(512);
#endif
        EEPROM.put(calVal_eepromAdress, newCalibrationValue);
#if defined(ESP8266) || defined(ESP32)
        EEPROM.commit();
#endif
        EEPROM.get(calVal_eepromAdress, newCalibrationValue);
        String savedValueMessage = "Value " + String(newCalibrationValue) + " saved to EEPROM address: " + String(calVal_eepromAdress);
        Serial.println(savedValueMessage);
        SerialBT.println(savedValueMessage);
        logToSD(savedValueMessage);
        _resume = true;
      } else if (inByte == 'n') {
        String notSavedMessage = "Value not saved to EEPROM";
        Serial.println(notSavedMessage);
        SerialBT.println(notSavedMessage);
        logToSD(notSavedMessage);
        _resume = true;
      }
    }
  }

  Serial.println("End calibration");
  SerialBT.println("End calibration");
  logToSD("End calibration");
}

void changeSavedCalFactor() {
  float oldCalibrationValue = LoadCell.getCalFactor();
  boolean _resume = false;
  String message = "***\nCurrent value is: " + String(oldCalibrationValue) + "\nNow, send the new value from serial monitor, i.e. 696.0";
  Serial.println(message);
  SerialBT.println(message);
  logToSD("Current calibration value: " + String(oldCalibrationValue));
  
  float newCalibrationValue;
  while (_resume == false) {
    if (Serial.available() > 0) {
      newCalibrationValue = Serial.parseFloat();
      if (newCalibrationValue != 0) {
        String newCalValueMessage = "New calibration value is: " + String(newCalibrationValue);
        Serial.println(newCalValueMessage);
        SerialBT.println(newCalValueMessage);
        logToSD(newCalValueMessage);
        LoadCell.setCalFactor(newCalibrationValue);
        _resume = true;
      }
    }
  }
  
  _resume = false;
  Serial.print("Save this value to EEPROM address ");
  Serial.print(calVal_eepromAdress);
  Serial.println("? y/n");
  SerialBT.print("Save this value to EEPROM address ");
  SerialBT.print(calVal_eepromAdress);
  SerialBT.println("? y/n");
  logToSD("Save new calibration value to EEPROM address " + String(calVal_eepromAdress) + "? y/n");

  while (_resume == false) {
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 'y') {
#if defined(ESP8266) || defined(ESP32)
        EEPROM.begin(512);
#endif
        EEPROM.put(calVal_eepromAdress, newCalibrationValue);
#if defined(ESP8266) || defined(ESP32)
        EEPROM.commit();
#endif
        EEPROM.get(calVal_eepromAdress, newCalibrationValue);
        String savedValueMessage = "Value " + String(newCalibrationValue) + " saved to EEPROM address: " + String(calVal_eepromAdress);
        Serial.println(savedValueMessage);
        SerialBT.println(savedValueMessage);
        logToSD(savedValueMessage);
        _resume = true;
      } else if (inByte == 'n') {
        String notSavedMessage = "Value not saved to EEPROM";
        Serial.println(notSavedMessage);
        SerialBT.println(notSavedMessage);
        logToSD(notSavedMessage);
        _resume = true;
      }
    }
  }
  Serial.println("End change calibration value");
  SerialBT.println("End change calibration value");
  logToSD("End change calibration value");
}

void processSerialCommand(String command) {
  if (command.indexOf('t') >= 0) LoadCell.tareNoDelay();  // tare
  else if (command.indexOf('r') >= 0) calibrate();        // calibrate
  else if (command.indexOf('c') >= 0) changeSavedCalFactor();  // edit calibration value manually
}

void logToSD(String message) {
  myFile = SD.open("/log.txt", FILE_WRITE);
  if (myFile) {
    myFile.println(message);
    myFile.close();
  } else {
    Serial.println("Error opening log.txt");
    SerialBT.println("Error opening log.txt");
  }
}
