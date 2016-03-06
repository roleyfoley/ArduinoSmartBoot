// SmartBoot
// Allows for the control of a Door (via servo) based on a proximity based switch or ambient temperature 
// Written By Michael Foley - e:roleyfoley@gmail.com

// ** Hardware Configuration ** 
//Servo
#include <Servo.h>
Servo doorServo;
const unsigned int servoPin = 10;

// Digital Sensor Setup  - 
// Proximity Sensor (https://www.adafruit.com/products/466)
// Temp Sensor (https://www.adafruit.com/products/2635)  
// Multiplexer (https://www.adafruit.com/product/2717)
#include <Wire.h>
#include <Adafruit_VCNL4010.h>
#include <Adafruit_HDC1000.h>

Adafruit_VCNL4010 doorProxSensor; 
Adafruit_VCNL4010 ampProxSensor;
Adafruit_HDC1000 doorHeatSensor = Adafruit_HDC1000();

// ic2 Multiplexer - Allows for multiple temp sensors
#define proxMulti  0x70
const unsigned int multiProxDoorID = 1;
const unsigned int multiProxAmpID = 2;


// Button - Standard Button working with the Prox
const unsigned int buttPin = 2;

// LED - Single Colour LED 
const unsigned int ledPin = 13;

// ** Behaviour Constants ** 
// -- Proximity Sensor --
// How close you need to be (65535 is on top of it, 0 is away from it)  
const unsigned int proxTrigger = 2500;
// Time(ms) you need to leave your hand over the prox sensor
const unsigned int proxTriggerDelay = 200; 

// -- Button -- 
const unsigned int buttTriggerDelay = 50;

// -- Servo -- 
// Servo Position for Open Door
const unsigned int posOpen = 35;
// Servo Position for Closed Door
const unsigned int posClosed = 110;
// How long to wait (milliseconds) on each incrament of the movement (per degree) - Total time = (POSOpen - POSClosed) * DoorMoveDelay 
const unsigned long posDelay = 15;

// -- Temp --  
// - All Temp readings are in degress celcius 
// Temp required for door to open
const unsigned int tempTrigger = 50;
// Temp requried for door to close (make it cooler than the trigger to prevent flapping) 
const unsigned int tempTriggerClose = 35;
// A delay (ms) to reduce flapping again 
const unsigned long tempDelay = 5000; 


// ** Running Variables **
String proxSwitch = "OFF"; 

int buttSwitch; 
int buttSwitchLast;
long buttPrevTime = 0;

String doorTempResult = "CLOSED";
String doorProxResult = "CLOSED"; 
String doorResult = "CLOSED";

String doorState = "CLOSED";

// Time Control
unsigned long tempPrevTime = 0;

// Control the multiplexer 
void tcaselect(uint8_t i) {
  if (i > 7) return;
 
  Wire.beginTransmission(proxMulti);
  Wire.write(1 << i);
  Wire.endTransmission();  
}

// ** Script Start ** 
void setup() {

// Serial Debug
Serial.begin(9600);
Serial.println("SmartBoot");

// Startup Servo
doorServo.attach(servoPin);

// Prox Setup
tcaselect(multiProxDoorID);
doorProxSensor.begin(); 

// Temp Setup
doorHeatSensor.begin();

// Button Setup 
pinMode(buttPin, INPUT);

// LED Setup
pinMode(ledPin, OUTPUT);
}


void loop() {
// Time Control - Set Current Time
unsigned long currentMillis = millis();

// **** Sensor Readings ***** 

// ---- Temperature Based ----
// If the temperature within the area is to high then the door opens. 
// Once it is a bit below (this stops it from flapping) the trigger temp it will close again.

// Check the Temperature every tempDelay - prevents the sensor being too sensitive
if (currentMillis - tempPrevTime >= tempDelay) {
        tempPrevTime = currentMillis;
        int temp = doorHeatSensor.readTemperature();
        Serial.print("Temperature: "); Serial.println(temp);
        if (temp > tempTrigger) {
          doorTempResult = "OPEN";
        }
        else if ( temp < tempTriggerClose) {
          doorTempResult = "CLOSED";
        }

}

// ---- Proximity Switch Based ----
// Using a Proximity sensor as a trigger Switch. Holding your hand over it triggers a state change. 
// The object in front must be removed then held over again to trigger the action again.

tcaselect(multiProxDoorID);
unsigned int proxCurrent = doorProxSensor.readProximity();

if ( proxCurrent > proxTrigger) {
    
    delay(proxTriggerDelay);
    tcaselect(multiProxDoorID);     
    unsigned int proxDelay = doorProxSensor.readProximity();
    
    if (proxDelay >= proxTriggerDelay) {
         if (proxSwitch == "OFF") {
          proxSwitch = "ON";
          if (doorProxResult == "CLOSED" ) {
            doorProxResult = "OPEN";
          }
          else {
            doorProxResult = "CLOSED"; 
          }
         }
    }
}

// Once the object is removed wait a little bit then reset the switch.
if (proxCurrent < proxTrigger ) {
    delay(proxTriggerDelay);
    tcaselect(multiProxDoorID);
    unsigned int proxDelay =  doorProxSensor.readProximity();

    if (proxDelay <= proxTrigger) {
      if (proxSwitch == "ON") {
        proxSwitch = "OFF"; 
      }
    }
}

// ---- Standard Button Based ---
// Allows for a standard button (or Device which will send a pulse to work along with the Prox sensor 
int buttReading = digitalRead(buttPin);

if (buttReading != buttSwitchLast) {
      buttPrevTime = currentMillis;
      buttSwitchLast = buttReading;
   } 

if ( currentMillis - buttPrevTime >= buttTriggerDelay) {
  if (buttSwitch != buttSwitchLast) {
    buttSwitch = buttSwitchLast;
    if (buttSwitch == HIGH) {
       if (doorProxResult == "CLOSED" ) {
          doorProxResult = "OPEN";
       }
       else {
        doorProxResult = "CLOSED"; 
       }  
    }
  }
}

// ***** Determine Door Action  ***** 
// Sets which states result in the appropriate action on the door. 
// - Button updates the Prox Sensor Result - You can open door with Prox and close with button
// - The temp sensor overrides the Proximity Sensor
// - The temp sensor will remember the Prox sensor state before it was triggered
// -- If the Prox had the door open it will stay open after the temp has gone below the trigger
// -- If the Prox had the door closed it will close after the temp has gone below the trigger

if ( (doorTempResult == "OPEN" && doorProxResult == "CLOSED") || (doorTempResult== "CLOSED" && doorProxResult == "OPEN") ){
    if (doorResult == "CLOSED") {
      doorResult = "OPEN";
    }
}
if ( doorTempResult == "CLOSED" && doorProxResult == "CLOSED") {
    if (doorResult == "OPEN") {
      doorResult = "CLOSED";
    }
}

// **** Door Movement ****
// Based on the result above, if the result is different to the current door state, perform the necessary action to make it the same.
// Nice Sweep open prevents doors breaking... 
if ( doorResult != doorState ) {
    if (doorResult == "OPEN") {
      Serial.println("Opening the door");
        for (int posCurrent = posClosed; posCurrent >= posOpen; posCurrent--){
          doorServo.write(posCurrent);
          delay(posDelay);
        }        
        digitalWrite(ledPin, HIGH);

    }
    if (doorResult == "CLOSED") {
      Serial.println("Closing the door");
        digitalWrite(ledPin, LOW);
        for (int posCurrent = posOpen; posCurrent <= posClosed; posCurrent++){
          doorServo.write(posCurrent);
          delay(posDelay);
        }
    }
    doorState = doorResult;
}

}
