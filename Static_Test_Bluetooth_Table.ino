#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

#include <HX711_ADC.h>
#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif

BluetoothSerial SerialBT;

// pins
const int HX711_dout = 4; //mcu > HX711 dout pin
const int HX711_sck = 5; //mcu > HX711 sck pin

//HX711 constructor:
HX711_ADC LoadCell(HX711_dout, HX711_sck);

const int calVal_eepromAdress = 0;
unsigned long t = 0;

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32 Static Test");
  Serial.println("------------------------------------------------");
  Serial.println("Bluetooth ativado, conecte o dispositivo.");
  Serial.println("------------------------------------------------");

  delay(10000);

  LoadCell.begin();
  //LoadCell.setReverseOutput(); //uncomment to turn a negative output value to positive
  unsigned long stabilizingtime = 2000; // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    SerialBT.println("Timeout, check MCU>HX711 wiring and pin designations");
    while (1);
  }
  else {
    LoadCell.setCalFactor(1.0); // user set calibration value (float), initial value 1.0 may be used for this sketch
    SerialBT.println("Startup is complete");
  }
  while (!LoadCell.update());
  calibrate(); //start calibration procedure

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
      SerialBT.print("Load_cell output val: ");
      SerialBT.println(i);
      newDataReady = 0;
      t = millis();
    }
  }

  // receive command from serial terminal
  if (Serial.available() > 0) {
    char inByte = SerialBT.read();
    if (inByte == 't') LoadCell.tareNoDelay(); //tare
    else if (inByte == 'r') calibrate(); //calibrate
    else if (inByte == 'c') changeSavedCalFactor(); //edit calibration value manually
  }

  // check if last tare operation is complete
  if (LoadCell.getTareStatus() == true) {
    SerialBT.println("Tare complete");
  }

}

void calibrate() {
  SerialBT.println("***");
  SerialBT.println("Start calibration:");
  SerialBT.println("Place the load cell an a level stable surface.");
  SerialBT.println("Remove any load applied to the load cell.");
  SerialBT.println("Send 't' from serialBT monitor to set the tare offset.");

  boolean _resume = false;
  while (_resume == false) {
    LoadCell.update();
    if (SerialBT.available() > 0) {
      if (SerialBT.available() > 0) {
        char inByte = SerialBT.read();
        if (inByte == 't') LoadCell.tareNoDelay();
      }
    }
    if (LoadCell.getTareStatus() == true) {
      SerialBT.println("Tare complete");
      _resume = true;
    }
  }

  SerialBT.println("Now, place your known mass on the loadcell.");
  SerialBT.println("Then send the weight of this mass (i.e. 100.0) from serial monitor.");

  float known_mass = 0;
  _resume = false;
  while (_resume == false) {
    LoadCell.update();
    if (SerialBT.available() > 0) {
      known_mass = SerialBT.parseFloat();
      if (known_mass != 0) {
        SerialBT.println("Known mass is: ");
        SerialBT.println(known_mass);
        _resume = true;
      }
    }
  }

  LoadCell.refreshDataSet(); //refresh the dataset to be sure that the known mass is measured correct
  float newCalibrationValue = LoadCell.getNewCalibration(known_mass); //get the new calibration value

  SerialBT.println("New calibration value has been set to: ");
  SerialBT.println(newCalibrationValue);
  SerialBT.println(", use this as calibration value (calFactor) in your project sketch.");
  SerialBT.println("Save this value to EEPROM adress ");
  SerialBT.println(calVal_eepromAdress);
  SerialBT.println("? y/n");

  _resume = false;
  while (_resume == false) {
    if (SerialBT.available() > 0) {
      char inByte = SerialBT.read();
      if (inByte == 'y') {
#if defined(ESP8266)|| defined(ESP32)
        EEPROM.begin(512);
#endif
        EEPROM.put(calVal_eepromAdress, newCalibrationValue);
#if defined(ESP8266)|| defined(ESP32)
        EEPROM.commit();
#endif
        EEPROM.get(calVal_eepromAdress, newCalibrationValue);
        SerialBT.println("Value ");
        SerialBT.println(newCalibrationValue);
        SerialBT.println(" saved to EEPROM address: ");
        SerialBT.println(calVal_eepromAdress);
        _resume = true;

      }
      else if (inByte == 'n') {
        SerialBT.println("Value not saved to EEPROM");
        _resume = true;
      }
    }
  }

  SerialBT.println("End calibration");
  SerialBT.println("***");
  SerialBT.println("To re-calibrate, send 'r' from serial monitor.");
  SerialBT.println("For manual edit of the calibration value, send 'c' from serial monitor.");
  SerialBT.println("***");
}

void changeSavedCalFactor() {
  float oldCalibrationValue = LoadCell.getCalFactor();
  boolean _resume = false;
  SerialBT.println("***");
  SerialBT.println("Current value is: ");
  SerialBT.println(oldCalibrationValue);
  SerialBT.println("Now, send the new value from serial monitor, i.e. 696.0");
  float newCalibrationValue;
  while (_resume == false) {
    if (SerialBT.available() > 0) {
      newCalibrationValue = SerialBT.parseFloat();
      if (newCalibrationValue != 0) {
        SerialBT.println("New calibration value is: ");
        SerialBT.println(newCalibrationValue);
        LoadCell.setCalFactor(newCalibrationValue);
        _resume = true;
      }
    }
  }
  _resume = false;
  SerialBT.println("Save this value to EEPROM adress ");
  SerialBT.println(calVal_eepromAdress);
  SerialBT.println("? y/n");
  while (_resume == false) {
    if (SerialBT.available() > 0) {
      char inByte = SerialBT.read();
      if (inByte == 'y') {
#if defined(ESP8266)|| defined(ESP32)
        EEPROM.begin(512);
#endif
        EEPROM.put(calVal_eepromAdress, newCalibrationValue);
#if defined(ESP8266)|| defined(ESP32)
        EEPROM.commit();
#endif
        EEPROM.get(calVal_eepromAdress, newCalibrationValue);
        SerialBT.println("Value ");
        SerialBT.println(newCalibrationValue);
        SerialBT.println(" saved to EEPROM address: ");
        SerialBT.println(calVal_eepromAdress);
        _resume = true;
      }
      else if (inByte == 'n') {
        SerialBT.println("Value not saved to EEPROM");
        _resume = true;
      }
    }
  }
  SerialBT.println("End change calibration value");
  SerialBT.println("***");
}