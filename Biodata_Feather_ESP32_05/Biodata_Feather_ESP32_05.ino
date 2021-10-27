 

// **-----------------------------------------------------------------------------
//ESP32 test for UDP/RTP AppleMIDI over wifi
//Biodata Sonification pulse input data via interrupt
//Knob and button interface
//LEDs using PWM fading
//ESP32 Feather A13 pin re ads half of battery voltage (4.2 (3.7) -3.2V)
//
// **-----------------------------------------------------------------------------

/***
1. Static IP Addressing
2. -removed-
3. LED display of wifi status, RTP status, battery status?
  Green Wifi Connected, White RTP connected, Yellow RTP disc conn, Blue Ble conn
  menus
    Red - MIDI channel, Yellow - MIDI Scale, Green - WiFi on/off, Blue - Ble on/off, White - Battery level
4. Move key variables to top of .ino (channel, wifi cred, IP, blue/wifi mode)
5. Velocity using CC80, mapping velocity range
6. Enable CC, variable for cc number and mapping range
7. press button, display status LED and usb serial information print

***/
// Wifi Credentials ~~~~~~~~~~~!!!!

char ssid[] = "OxfordEast"; //  your network SSID (name)
char pass[] = "oxfordeast";    // your network password (use for WPA, or use as key for WEP)
// ~~~~~~~~~~~~~!!!!
//  Set the MIDI Channel of this node
  byte channel = 1; // 
  IPAddress local_IP(192,168,68,207); //use this IP
  bool staticIP = false;  // true;  //toggle for dynamic IP
//static IP  

//MIDI Note and Controls
const byte polyphony = 5; // 1; //mono  // number of notes to track at a given time

//******************************
//set scaled values, sorted array, first element scale length
//the whole scaling algorithm needs to be refactored ;)
int scaleMajor[]  = {7, 0, 2, 4, 5, 7, 9, 11};
int scaleDiaMinor[]  = {7, 0, 2, 3, 5, 7, 8, 10};
int scaleIndian[]  = {7, 0, 1, 1, 4, 5, 8, 10};
int scaleMinor[]  = {7, 0, 2, 3, 5, 7, 8, 10};
int scaleChrom[] = {13, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
     //enter the scale name here to use
int *scaleSelect = scaleChrom; //initialize scaling
int root = 0; //initialize for root
//*******************************

#include <PinButton.h>

//Debug and MIDI output Settings ********
byte debugSerial = 1; //debugging serial messages
byte serialMIDI = 1; //write serial data to MIDI hardware output
byte wifiMIDI = 0; //do all the fancy wifi stuff and RTP MIDI over AppleMIDI
byte bleMIDI = 1; //bluetooth midi
//byte usbMIDI = 0; //usb MIDI connection
byte midiMode = 0;  //change mode for serial, ble, wifi, usb
byte wifiActive = 0; //turn the wifi on and off, needed for wifiMIDI apparently
byte bleActive = 0; //toggle for connected disconnected?  prob not needed, use library
byte midiControl = channel; // 1; //use channel 16 to recieve parameter changes? fancy! or recv on base channel...hmm
//setting channel to 11 or 12 often helps simply computer midi routing setups
int noteMin = 36; //C2  - keyboard note minimum
int noteMax = 96; //C7  - keyboard note maximum
byte controlNumber = 80; //set to mappable control, low values may interfere with other soft synth controls!!
 
// **************************************

#include <EEPROM.h>
#define EEPROM_SIZE 5 // scaleindex, midi channel, wifi, bluetooth, key


// I/O Pin declarations
int buttonPin = 13;
int potPin = A2; //A2 ESP32 and A0 ESP8266
const byte interruptPin = 12; //galvanometer input

//leds
byte leds[5] = { 26,25,4,5,18 };
byte ledBrightness[5] = {50,110,120,120,60};
byte maxBrightness = 60;
bool blinkToggle = 0;
unsigned long blinkTime = 0; 

//Bluetooth Configuration
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#define SERVICE_UUID        "03b80e5a-ede8-4b33-a751-6ce34ec4c700"
#define CHARACTERISTIC_UUID "7772e5db-3868-4112-a1a9-f2669d106bf3"

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

uint8_t midiPacket[] = {
   0x80,  // header
   0x80,  // timestamp, not implemented 
   0x00,  // status
   0x3c,  // 0x3c == 60 == middle c
   0x00   // velocity
};

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};


//fading leds
/*
 * pinNumber, espPWMchannel, maxBright,
 * currentLevel, destinationLevel, startTime, 
 * duration, stepSize, stepTime, isRunning
 * set stepSize as rate of fade based on maxBrightness and duration
 *      stepSize = duration/destinationLevel
 * if isRunning
 *   if currTime-startTime<duration //fade running
 *     if currTime=stepTime>stepSize
 *       stepTime=currTime
 *       if curr<dest, curr++
 *       if curr>dest, cur--
 *   else isRunning = FALSE //fade ended
 */
class samFader {
  
  public:
  byte pinNumber;
  byte espPWMchannel;
  int maxBright;
  int currentLevel = 0;
  int destinationLevel = 0;
  unsigned long startTime; //time at which a fade begins
  int duration = 0;
  unsigned long stepSize;  //computed duration across brightness 
  unsigned long stepTime; //last time a step was called
  bool isRunning = 0;

  samFader(byte pin, byte pwmChannel, byte maxB) {
    pinNumber=pin;
    espPWMchannel=pwmChannel;
    maxBright=maxB;
  }

  void Set(int dest, int dur) {
    startTime = millis();
    destinationLevel = dest;
    duration = dur;
    stepTime = 0;
    int difference = abs(destinationLevel - currentLevel);
    stepSize = duration/(difference+1);
    isRunning = 1;
    if(dur == 0) { // do it right away!
      ledcWrite(espPWMchannel, dest);
      isRunning = 0;
    }
  }
  
  void Update() {
    if(isRunning) {
      if(stepTime+stepSize<millis()) {
        //update to the next level
        if(currentLevel>destinationLevel) currentLevel--;
        if(currentLevel<destinationLevel) currentLevel++;
        //update variables for tracking
        stepTime = millis();
        //write brightness to LED
        ledcWrite(espPWMchannel, currentLevel);
      }
      if(startTime+duration<millis()) {
        ledcWrite(espPWMchannel, destinationLevel);
        isRunning = 0;
      }
    }

  }

  void Setup(byte chan) {
    ledcSetup(chan,5000,13);
    ledcAttachPin(pinNumber,chan);
  }
  
};

samFader ledFaders[] = {  samFader(leds[0],0,ledBrightness[0]),
                          samFader(leds[1],1,ledBrightness[1]),
                          samFader(leds[2],2,ledBrightness[2]),
                          samFader(leds[3],3,ledBrightness[3]),                          
                          samFader(leds[4],4,ledBrightness[4])
};


//change this to samButton
PinButton button(buttonPin);

//Timing and tracking
unsigned long currentMillis = 0;
unsigned long prevMillis = 0;


//****** sample size sets the 'grain' of the detector
// a larger size will smooth over small variations
// a smaller size will excentuate small changes
const byte samplesize = 10; //set sample array size
const byte analysize = samplesize - 1;  //trim for analysis array,

volatile unsigned long microseconds; //sampling timer
volatile byte sampleIndex = 0;
volatile unsigned long samples[samplesize];

float threshold = 1.71; //threshold multiplier
float threshMin = 1.61; //scaling threshold min
float threshMax = 4.01  ; //scaling threshold max
float prevThreshold = 0;



typedef struct _MIDImessage { //build structure for Note and Control MIDImessages
  unsigned int type;
  int value;
  int velocity;
  long duration;
  long period;
  int channel;
}
MIDImessage;
MIDImessage noteArray[polyphony]; //manage MIDImessage data as an array with size polyphony
int noteIndex = 0;
MIDImessage controlMessage; //manage MIDImessage data for Control Message (CV out)



#if defined(ARDUINO_SAMD_ZERO) && defined(SERIAL_PORT_USBVIRTUAL)
  // Required for Serial on Zero based boards
  #define Serial SERIAL_PORT_USBVIRTUAL
#endif

//setups for each MIDI type, provide led display output
void setupSerialMIDI() {
   if (debugSerial) Serial.println("MIDI set on Serial1 31250");
  Serial1.begin(31250);
}



void checkKnob() {
  //float knobValue
  bool bigChange = 0;
  threshold = analogRead(potPin); 
  
  if(abs(prevThreshold - threshold) > 200) {
    bigChange = 1;
  }

  
  prevThreshold = threshold; //remember last value
  //set threshold to knobValue mapping
  threshold = mapfloat(threshold, 0, 4095, threshMin, threshMax);

}

//provide float map function
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>

#define SerialMon Serial
#define APPLEMIDI_DEBUG SerialMon
//#include "AppleMidi.h"
#include <AppleMIDI.h>

bool isConnected = false;

//APPLEMIDI_CREATE_INSTANCE(WiFiUDP, AppleMIDI); // see definition in AppleMidi_Defs.h
APPLEMIDI_CREATE_DEFAULTSESSION_INSTANCE();

void setupWifi(){

  //connect with static IP

  IPAddress gateway(192,168,68,1);
  IPAddress dns(192,168,68,1);
  IPAddress subnet(255,255,255,0);
  if(staticIP) {
    if(!WiFi.config(local_IP,dns, gateway,subnet)) {
     Serial.print(F("Static IP config Failure"));
   }
  }
  Serial.print(F("Connecting to Wifi Network: "));
  Serial.print(ssid); Serial.print(F("  pass: ")); Serial.print(pass); Serial.println();

  WiFi.begin(ssid, pass);

//could get 'stuck' here if we can't connect ... hmm..
//timeout after some duration  to allow Serial MIDI or 
//combo BLE (who would do that?)  
//but it doesn't seem to get stuck .. at home duh!
  bool ledToggle = 1;
  unsigned long wifiMillis = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
    //flash LED on and off toggle
    if(ledToggle) {
      ledFaders[0].Set(ledFaders[0].maxBright,0);
    } else ledFaders[0].Set(0,0);
    ledToggle = !ledToggle;
    ledFaders[0].Update();
    if(wifiMillis+15000<millis()) break;
  }

  //was wifi connectionsuccessfuL?
    if(WiFi.status() == WL_CONNECTED) {
      Serial.println(F("WiFi connected"));
      //solid LED
      ledFaders[0].Set(0,0); //turn off Red
      ledFaders[2].Set(ledFaders[2].maxBright,0); //turn on Green
        delay(1000);
      ledFaders[2].Set(0, 4000); //fade out green LED
    }
    else {
      Serial.println(F("WiFi NOT connected"));
        ledFaders[0].Set(ledFaders[0].maxBright,0); //turn on Red
    }
          //try to use MAC address or other ID
          //for unique name on each ble and wifi RTP device
//          uint64_t chipid = ESP.getEfuseMac();
//          byte chip = (uint32_t)chipid;
//          String chipStr = "BIODATA ";
//          chipStr.concat(chip);
//          Serial.println(chipStr);
    byte mac[6];
    WiFi.macAddress(mac);
    String macAddr =  String(mac[0],HEX) +String(mac[1],HEX) +String(mac[2],HEX) +String(mac[3],HEX) + String(mac[4],HEX) + String(mac[5],HEX);          
    int uniq =  mac[0] + mac[1] + mac[2] + mac[3] + mac[4] + mac[5];
    Serial.print("MAC Address: "); Serial.println(macAddr);
    Serial.print("Biodata "); Serial.println(uniq);
  Serial.print(F("IP address is "));
  Serial.println(WiFi.localIP());

  // Create a session and wait for a remote host to connect to us
//  AppleMIDI.begin("ESP32_MIDI");
    MIDI.begin();
  //isConnected
  AppleMIDI.setHandleConnected([](const APPLEMIDI_NAMESPACE::ssrc_t & ssrc, const char* name) {
    isConnected = 1;
    DBG(F("Connected to session"), ssrc, name);
    ledFaders[4].Set(ledFaders[4].maxBright,0); // turn on white LED;
    delay(1000);
    ledFaders[4].Set(0,0);
  });
  AppleMIDI.setHandleDisconnected([](const APPLEMIDI_NAMESPACE::ssrc_t & ssrc) {
    isConnected = 0;
    DBG(F("Disconnected"), ssrc);
    ledFaders[1].Set(ledFaders[1].maxBright,0); // turn on white LED;
    delay(1000);
    ledFaders[1].Set(0,0);
  }); 
}

void bleSetup() {

  byte mac[6];
    WiFi.macAddress(mac);
    String macAddr =  String(mac[0],HEX) +String(mac[1],HEX) +String(mac[2],HEX) +String(mac[3],HEX) + String(mac[4],HEX) + String(mac[5],HEX);          
    int uniq =  mac[0] + mac[1] + mac[2] + mac[3] + mac[4] + mac[5];
  String chipStr = "BIODATA ";
  chipStr.concat(uniq);
  char chipString[15];
  chipStr.toCharArray(chipString,15);
  BLEDevice::init(chipString);


  if(debugSerial)  { Serial.print("BiodataBLE "); Serial.println(chipString); }  
  //bluetooth notification
  ledFaders[3].Set(ledFaders[3].maxBright,0);
  delay(1000);
    
  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(BLEUUID(SERVICE_UUID));

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
    BLEUUID(CHARACTERISTIC_UUID),
    BLECharacteristic::PROPERTY_READ   |
    BLECharacteristic::PROPERTY_WRITE  |
    BLECharacteristic::PROPERTY_NOTIFY |
    BLECharacteristic::PROPERTY_WRITE_NR
  );

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->addServiceUUID(pService->getUUID());
  pAdvertising->start();

}
