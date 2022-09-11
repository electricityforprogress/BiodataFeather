 //   ESP32 S3 
//Electricity for Progress
// electricityforprogress.com
// store - Biodata - Modules - Custom
// **-----------------------------------------------------------------------------
//ESP32-S3 UDP/RTP AppleMIDI over wifi
//Biodata Sonification pulse input data via interrupt
//Knob and button interface
//LEDs using PWM fading
//ESP32 Feather A13 pin re ads half of battery voltage (4.2 (3.7) -3.2V)
// ~~~NEW~~~
//  * Captive Portal for wifi configuration
//  * Velocity - five levels of control: red-100,yellow-accent(90/120),
//     green-(75,95,120),blue-musical map(value,x,y,50,120),
//     white-fluent map(value,x,y,0,127)
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
//int scaleDiaMinor[]  = {7, 0, 2, 3, 5, 7, 8, 10};
int scalePenta[]  = {5, 0, 3, 5, 7, 9};
int scaleMajor[]  = {7, 0, 2, 4, 5, 7, 9, 11};
int scaleIndian[]  = {7, 0, 1, 1, 4, 5, 8, 10};
int scaleMinor[]  = {7, 0, 2, 3, 5, 7, 8, 10};
int scaleChrom[] = {13, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
     //enter the scale name here to use
int *scaleSelect = scaleChrom; //initialize scaling
byte defScale = 3; 

int root = 0; //initialize for root
//*******************************

//Debug and MIDI output Settings ********
byte debugSerial = 1; //debugging serial messages
byte rawSerial = 0; // raw biodata stream via serial data
byte serialMIDI = 1; //write serial data to MIDI hardware output
byte wifiMIDI = 0; //do all the fancy wifi stuff and RTP MIDI over AppleMIDI
byte bleMIDI = 1; //bluetooth midi
byte usbmidi = 1; //usb MIDI connection <Adafruit_TinyUSB.h> for ESP32S3
byte midiMode = 0;  //change mode for serial, ble, wifi, usb
byte wifiActive = 0; //turn the wifi on and off, needed for wifiMIDI apparently
byte bleActive = 0; //toggle for connected disconnected?  prob not needed, use library
byte midiControl = channel; // 1; //use channel 16 to recieve parameter changes? fancy! or recv on base channel...hmm
//setting channel to 11 or 12 often helps simply computer midi routing setups
int noteMin = 36; //C2  - keyboard note minimum
int noteMax = 96; //C7  - keyboard note maximum
byte controlNumber = 80; //set to mappable control, low values may interfere with other soft synth controls!!
unsigned long rawSerialTime = 0;
int rawSerialDelay = 0;
// **************************************

#include <EEPROM.h>
#define EEPROM_SIZE 5 // scaleindex, midi channel, wifi, bluetooth, key


// I/O Pin declarations
int buttonPin = 13;
int potPin = A2; //A2 ESP32 and A0 ESP8266
const byte interruptPin = 12; //galvanometer input

//leds
byte leds[5] = {18,17,8,36,35}; //ESP32-S3 pins  // ESP32Huzzah -- { 26,25,4,5,18 };
byte ledBrightness[5] = {50,110,120,120,60};  //{185,255,255,255,195};  
byte maxBrightness = 60;
bool blinkToggle = 0;
unsigned long blinkTime = 0; 

//USB MIDI
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, usbMIDI);


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
    ledcSetup(chan,5000,8);
    ledcAttachPin(pinNumber,chan);
  }
  
};

samFader ledFaders[] = {  samFader(leds[0],0,ledBrightness[0]),
                          samFader(leds[1],1,ledBrightness[1]),
                          samFader(leds[2],2,ledBrightness[2]),
                          samFader(leds[3],3,ledBrightness[3]),                          
                          samFader(leds[4],4,ledBrightness[4])
};


class samButton {
  public:

    unsigned int doubleClickTime = 300; //milliseconds within which a double click event could occur
    unsigned int debounce = 35; //debounce time
    bool changed = false; //short lived indicator that a change happened, will see after an update call

    samButton(byte buttonPin, bool pUp); //constructor

    void begin(); // initialize button pin with pullup
    bool update(); // update button and led
    bool read(); // read current button state 'raw' value
    unsigned long changedTime(); // returns last changed time
    bool pressed(); // is the button currently pressed?
    bool wasPressed(); // was the button just depressed?
    bool wasReleased(); // was the button just released?
    bool longPress(); // if(read() && currentMillis - pressStart > _longTime)
    bool doubleClick(); // if(_clickCount == 2)

  private:
    unsigned int _buttonPin;
    unsigned int _buttonIndex = 0;
    bool _pullup = 1; // resistor pullup = 1
    unsigned int _clickTime = 100; //time to detect true click
    unsigned int _longTime = 1000; //time to detect long press
    unsigned int _clickCount; // number of times button was clicked within a double click time
    bool _state = 0;
    bool _prevState = 0;
    unsigned long _changeTime = 0; //denotes change after toggle or update to button state
    unsigned long _prevDebounce = 0; // previous debounce toggle in real time
};

  //begin SamButton
  //****************
  // Simple Button debouncing
  //****************
  //#include "samButton.h"
  
  samButton::samButton(byte buttonPin, bool pUp) {
    _buttonPin = buttonPin;
    _pullup = pUp;
  }
 
  void samButton::begin() {
    if (_pullup == 1) pinMode(_buttonPin, INPUT_PULLUP); //setup button input
    else pinMode(_buttonPin, INPUT); // no pullup
    _state = !digitalRead(_buttonPin);
    //set button prev state
    _prevState = _state;
    //set button time
    _prevDebounce = millis();
    //change time is current time
    _changeTime = _prevDebounce;
    //change is false
    changed = 0;
  }
  
  bool samButton::update() {
    unsigned long milli = millis(); //get current time
  
    //bool reading = MPR121.getTouchData();  //
    bool reading = !digitalRead(_buttonPin);  //get current reading
    if (reading != _prevState) { // if current reading is change from last secure state
      _prevState = reading; // reset bouncing reading
      _prevDebounce = milli; // update bouncing time
    }
  
    if (milli - _prevDebounce > debounce) { //evaulate debounce time
      _prevDebounce = milli; //reset time
      if (reading != _state) { // debounced and changed
        changed = 1;
        _state = reading;
        _prevState = _state;
        _changeTime = milli;
      }
    }
    else changed = 0; //has not changed or still under debounce time
    return _state;
  }
  
  bool samButton::read() {
    return !digitalRead(_buttonPin); ;
  }
  
  
  bool samButton::pressed() { //returns true if the button is pressed currently
    return _state;
  }
  
  unsigned long samButton::changedTime() {  // used to determine how long button has been in current state
    return _changeTime;
  }
  
  bool samButton::wasPressed() {
    return changed && _state; // if both just changed and button is pressed
  }
  
  bool samButton::wasReleased() {
    return changed && !_state;
  }
  //end SamButton

//declare button
samButton button(buttonPin,true);

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


//I did this before using the other MIDI libraries ....
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
  Serial1.begin(31250); //MIDI data out TX pins
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
//https://github.com/lathoub/Arduino-AppleMIDI-Library
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
     if (debugSerial) Serial.print(F("Static IP config Failure"));
   }
  }

  if (debugSerial) { 
    Serial.print(F("Connecting to Wifi Network: "));
    Serial.print(ssid); Serial.print(F("  pass: ")); Serial.print(pass); Serial.println();
  }

  WiFi.begin(ssid, pass);

//could get 'stuck' here if we can't connect ... hmm..
//timeout after some duration  to allow Serial MIDI or 
//combo BLE (who would do that?)  
//but it doesn't seem to get stuck .. at home duh!
  bool ledToggle = 1;
  unsigned long wifiMillis = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (debugSerial) Serial.print(F("."));
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
      if (debugSerial) Serial.println(F("WiFi connected"));
      //solid LED
      ledFaders[0].Set(0,0); //turn off Red
      ledFaders[2].Set(ledFaders[2].maxBright,0); //turn on Green
        delay(1000);
      ledFaders[2].Set(0, 4000); //fade out green LED
    }
    else {
      if (debugSerial) Serial.println(F("WiFi NOT connected"));
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
  if (debugSerial){
    Serial.print("MAC Address: "); Serial.println(macAddr);
    Serial.print("Biodata "); Serial.println(uniq);
    Serial.print(F("IP address is "));
    Serial.println(WiFi.localIP());
  }
  // Create a session and wait for a remote host to connect to us
//  AppleMIDI.begin("ESP32_MIDI");
    MIDI.begin();
  //isConnected
  AppleMIDI.setHandleConnected([](const APPLEMIDI_NAMESPACE::ssrc_t & ssrc, const char* name) {
    isConnected = 1;
//    DBG(F("Connected to session"), ssrc, name);
    Serial.print("Connected to session "); Serial.println(name);
    ledFaders[4].Set(ledFaders[4].maxBright,0); // turn on white LED;
    delay(1000);
    ledFaders[4].Set(0,0);
  });
  AppleMIDI.setHandleDisconnected([](const APPLEMIDI_NAMESPACE::ssrc_t & ssrc) {
    isConnected = 0;
//    DBG(F("Disconnected"), ssrc);
    Serial.println("AppleMIDI Disconnected"); 
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


void checkButton() {
//update the button and evaluate menu modes
// **Bug** crash after scale change when notes are running 
    byte modeValue = 0;
    button.update();
  

 // if(button.isSingleClick()) {
    if(button.wasReleased()) {
       if (debugSerial) {
          Serial.println("---***---***---ButtonClick***---***---***");
          byte mac[6];
          WiFi.macAddress(mac);
          String macAddr =  String(mac[0],HEX) +String(mac[1],HEX) +String(mac[2],HEX) +String(mac[3],HEX) + String(mac[4],HEX) + String(mac[5],HEX);          
          int uniq =  mac[0] + mac[1] + mac[2] + mac[3] + mac[4] + mac[5];
          Serial.print("Biodata "); Serial.println(uniq);
          Serial.print("MIDI Channel "); Serial.println(channel);
          Serial.print("ScaleIndex "); Serial.println(EEPROM.read(0));
          Serial.print("Threshold "); Serial.println(threshold);
          Serial.print("MAC Address: "); Serial.println(macAddr);
          Serial.print("Note Scale: "); Serial.println(modeValue);
          //MIDI Output Statuses
          
          if(usbmidi) Serial.println("USB MIDI On");
          else Serial.println("USB MIDI Off");
          //bluetooth status
          if(bleMIDI) {
              Serial.print("Bluetooth On "); 
              if(deviceConnected)  Serial.println("Bluetooth Connected"); else Serial.println("Bluetooth DisConnected");
          } else Serial.println("Bluetooth Off");


        if(wifiMIDI){
              //wifi connection status
          Serial.print(ssid); Serial.print(F("  pass: ")); Serial.print(pass); Serial.println();
          if(WiFi.status() == WL_CONNECTED) {
            Serial.println("Wifi Connected");
          } else Serial.println("WiFi Not Connected");

       //wifi Signal level
          Serial.print("RSS Signal level: "); Serial.println(WiFi.RSSI());
          if(isConnected) Serial.println(F("RTP MIDI Connected!"));
          else Serial.println("RTP MIDI Not Connected");
            Serial.println();
            Serial.print(F("IP address is "));
            Serial.println(WiFi.localIP());
        } else Serial.println("Wifi Off");
      
      }

              //battery monitor
//        float batteryLevel = float(analogRead(35))/float(4095)*2.0*3.3*1.1; //A13
//        if (debugSerial) {
//          Serial.print("Battery Voltage: ");
//          Serial.println(batteryLevel); //of course while plugged into usb the voltage will be 4.x
//        }

        int knobValue = analogRead(potPin);
        int prevKnob = knobValue;
        unsigned long menuTimer = millis();
        int menu = 0; //main biodata play mode
        while(menuTimer + 10000 > millis()) {
           //knob turn to extend time and select menu
           knobValue = analogRead(potPin);
          // if(abs(knobValue - prevKnob) > 45){
            //select range for each menu and check for click
             menu = map(knobValue, 0, 4095, 0, 3);

            for(byte i=0;i<5;i++) { ledFaders[i].Set(0,0); } //turn leds off
            //turn on Menu LED
               //blink led during selection
               if((blinkTime ) < millis()) { blinkToggle = !blinkToggle; blinkTime = millis(); }   
               if(blinkToggle) ledFaders[menu].Set(ledFaders[menu].maxBright,0);
                        
            //reset timer
            //menuTimer = millis();
          // }  if knob change
           prevKnob = knobValue;
           
           //check for click and enter Menu mode
             button.update();
             modeValue = 0;
//             if(button.isSingleClick()) {
              if(button.wasReleased()) {
                if (debugSerial) { 
                  Serial.print("Enter Menu "); Serial.print(menu); 
                  if(menu == 0) Serial.println(" Scale");
                  if(menu == 1) Serial.println(" Channel");
                  if(menu == 2) Serial.println(" Wifi");
                  if(menu == 3) Serial.println(" Bluetooth");                  
                  }
                menuTimer = millis();
                while(menuTimer + 20000 > millis()){
                    //Loop for management of menu mode selection
                    //main biodata routine does not run during menu selection

                    //check the button for clicks
                    button.update();
                    //knob turn to extend time and select menu???
                    knobValue = analogRead(potPin);
                    
                   // if(abs(knobValue - prevKnob) > 45){  //if knob change
                      //menuTimer = millis();
 
                      for(byte i=0;i<5;i++) { ledFaders[i].Set(0,0); } //all off
 
                      if(menu == 0) { // MIDI Scaling
                       modeValue = map(knobValue, 0, 4095, 0, 4);
                       //blink led during selection
                       if((blinkTime + 150) < millis()) { blinkToggle = !blinkToggle; blinkTime = millis(); }   
                       if(blinkToggle) ledFaders[modeValue].Set(ledFaders[modeValue].maxBright,0);
                       if(menu!=modeValue) ledFaders[menu].Set(ledFaders[menu].maxBright,0); //red0
                       
                       if(modeValue == 0) scaleSelect = scaleChrom; 
                       if(modeValue == 1) scaleSelect = scaleMinor; 
                       if(modeValue == 2) scaleSelect = scaleMajor; 
                       if(modeValue == 3) scaleSelect = scalePenta; 
                       if(modeValue == 4) scaleSelect = scaleIndian; 
                       
                      }
                      if(menu==1) { //MIDI Channel Selection
                        modeValue = map(knobValue, 0, 4095, 1, 17); if(modeValue == 17) modeValue = 16;
                        //if((blinkTime + 150) < millis()) { blinkToggle = !blinkToggle; blinkTime = millis(); }   
                        //if(blinkToggle) ledFaders[modeValue].Set(ledFaders[modeValue].maxBright,0); 
                        //display channel in binary
                         int showChannel = modeValue;
                         for (byte i = 0; i < 5; i++) ledFaders[i].Set(0,0); //turn off
                          //convert to binary
                          int cursorPos = 0;
                          int binaryValue[5];
                          for(int i=0;i<5;i++){
                            binaryValue[i] = showChannel % 2;
                            showChannel = showChannel/2;
                          }
                         
                         for (byte i = 0; i < 5; i++) {
                            //if this bit should be on then turn it on...
                             if(binaryValue[i]) ledFaders[i].Set(ledFaders[i].maxBright, 0);
                         }
                       
                      }
                      if(menu==2) { //Wifi Config
                          //display green LED for wifi mode, red if wifi off, yellow if wifi not conn, white if RTP connected
                          //turn knob to select (flashing) - Wifi Off - Red; Wifi On - White
                          
                          //knob map to 0/1
                         modeValue = map(knobValue, 0, 4095, 0, 1);
                          //blink led during selection
                         if((blinkTime + 150) < millis()) { blinkToggle = !blinkToggle; blinkTime = millis(); }   
                         if(blinkToggle) ledFaders[modeValue*4].Set(ledFaders[modeValue].maxBright,0);
                         ledFaders[menu].Set(ledFaders[menu].maxBright,0); //green


                      }                      
                      if(menu==3) { //Bluetooth Config
                        //display blue for ble mode, if discon
                        modeValue = map(knobValue, 0, 4095, 0, 1);
                        if((blinkTime + 150) < millis()) { blinkToggle = !blinkToggle; blinkTime = millis(); }   
                        if(blinkToggle) ledFaders[modeValue*4].Set(ledFaders[modeValue].maxBright,0);
                        ledFaders[menu].Set(ledFaders[menu].maxBright,0); //blue
                        
                        
                      }
//                      if(menu==4) {
//                          int batt = batteryLevel*10; 
//                          modeValue = map(batt, 32,44,0,4);
//                        if((blinkTime + 150) < millis()) { blinkToggle = !blinkToggle; blinkTime = millis(); }   
//                        if(blinkToggle) ledFaders[modeValue].Set(ledFaders[modeValue].maxBright,0);
//                        if(modeValue != menu) ledFaders[menu].Set(ledFaders[menu].maxBright,0); //white
//                        
//                      } 
                      


                     //Menu has been entered, value has been selected
                     //click to save a value from the submenu       
//                      if(button.isSingleClick()) { ///select value sub menu
                      if(button.wasReleased()) {
                        //light show fast flash
                        for(byte i=0;i<5;i++) { ledFaders[i].Set(0,0); } //all off
                        ledFaders[menu].Set(ledFaders[menu].maxBright,0);
                        delay(75);
                        ledFaders[menu].Set(0,0);
                        delay(75);
                        ledFaders[menu].Set(ledFaders[menu].maxBright,0);
                        delay(75);
                        ledFaders[menu].Set(0,0);
                        delay(75);
                        ledFaders[menu].Set(ledFaders[menu].maxBright,0);
                        delay(75);
                        ledFaders[menu].Set(0,0);
                        
                        for(byte i=0;i<5;i++) { ledFaders[i].Set(0,0); } //all off

                        if(menu == 0){
                          EEPROM.write(0,modeValue);
                         // EEPROM.commit();
                          if (debugSerial) { Serial.print("MIDI Scale "); Serial.println(modeValue); }
                        }
                        if(menu == 1){
                          channel = modeValue;  //set channel value
                          EEPROM.write(1,channel);
                        //  EEPROM.commit();
                          if (debugSerial) { Serial.print("Channel "); Serial.println(channel); }
                        }
                        //save and set values as needed (wifi/bluetooth)
                        if(menu == 2){
                             if(modeValue == 0 && wifiMIDI == 1)   { 
                                if(debugSerial) Serial.println("Wifi Shutdown ");
                                WiFi.disconnect(true); delay(1); 
                                WiFi.mode(WIFI_OFF); delay(1);
                               // if(debugSerial) Serial.println(WiFi.status() == WL_CONNECTED)
                                  wifiMIDI = 0;
                                  EEPROM.write(2,0);
                             } //turn off wifi and power down (disconnect, turn off, )
                             if(modeValue == 1)  { 
                              if (debugSerial) Serial.println("Wifi Power On");
                                wifiMIDI = 1; 
                                EEPROM.write(2,1);
                                setupWifi();  //turn on wifi and initialize (turn on, connect)
                             }   
                        }
                        if(menu == 3) {
                              if(modeValue == 0)  { 
                                bleMIDI = 0; btStop(); EEPROM.write(3,0); 
                                if (debugSerial) Serial.println("BLE Off");
                              }//turn off bluetooth
                              if(modeValue == 1)  { 
                                bleMIDI = 1;  btStart(); bleSetup(); EEPROM.write(3,1); 
                                if (debugSerial) Serial.println("BLE On");
                              }//turn on bluetooth
                              ledFaders[menu].Set(0,700); //turn off Blue led
                        }
//                        if(menu == 4) { //display wifi signal level
//                          for(byte i=0;i<5;i++) { ledFaders[i].Set(0,0); } //all off
//                          byte wifiLevel = map(abs(WiFi.RSSI()),90,30,0,4);
//                          for(byte j=0;j<wifiLevel;j++){
//                            ledFaders[j].Set(ledFaders[j].maxBright,0);
//                          }
//                          delay(3000);
//                          for(byte i=0;i<5;i++) { ledFaders[i].Set(0,700); } //all off
////                          if(button.isSingleClick()) { 
//                          Serial.print("RSSI "); Serial.print(WiFi.RSSI()); 
//                          Serial.print(" wifiLevel "); Serial.println(wifiLevel);
////                          return;
////                          }
//                        }
                        
                        EEPROM.commit(); //save all the values!!

                        Serial.println("settings Saved, return to main?");
                        button.update(); //clear that button click lingering
                        return; //break; //save here              
                      }

                      //unsure if button clicks get here...shouldn't tho!
//                   if(button.isSingleClick()) {
                  if(button.wasReleased()) {
                     for(byte i=0;i<5;i++) { ledFaders[i].Set(0,0); } //all off
                     if (debugSerial) Serial.println("shouldnt get here in menus?");
                     return; //save here
                   }
                   prevKnob = knobValue;
                }
             } 
        }
        
        for(byte i=0;i<5;i++) { ledFaders[i].Set(0,0); } //all off
  }
  //if double, if long click, etc
  
}
