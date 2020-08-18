
// **-----------------------------------------------------------------------------
//ESP32 test for UDP/RTP AppleMIDI over wifi
//Biodata Sonification pulse input data via interrupt
//Knob and button interface
//Neopixel LEDs (5)
//Stretch to include BLE MIDI
//Stretch to include USB HID MIDI on seperate chip *promicro daughter?
//Stretch to include Serial MIDI I/O
// **-----------------------------------------------------------------------------

#include <PinButton.h>

//Debug and MIDI output Settings ********
byte debugSerial = 1; //debugging serial messages
byte serialMIDI = 1; //write serial data to MIDI hardware output
byte wifiMIDI = 1; //do all the fancy wifi stuff and RTP MIDI over AppleMIDI
byte bleMIDI = 0; //bluetooth midi
byte usbMIDI = 0; //usb MIDI connection
byte midiMode = 0;  //change mode for serial, ble, wifi, usb
byte wifiActive = 0; //turn the wifi on and off, needed for wifiMIDI apparently
byte midiControl = 1; //use channel 16 to recieve parameter changes? fancy!
// **************************************

// I/O Pin declarations 
int buttonPin = 13;
int potPin = A2; //A2 ESP32 and A0 ESP8266
int neoPin = 27; //Neopixels attached to this pin
const byte interruptPin = 12; //galvanometer input

PinButton button(buttonPin);

//Timing and tracking
unsigned long currentMillis = 0;
unsigned long prevMillis = 0;

//Wifi and MIDI stuff
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <WiFiAP.h>
#include <WebServer.h>
#include "AppleMidi.h"

char ssid[] = "OxfordEast"; //  your network SSID (name)
char password[] = "oxfordeast";    // your network password (use for WPA, or use as key for WEP)
char* assid = "ESP32_MIDI";  // name of access point
char* apassword = "password"; // password for access point

WebServer server(80);
bool isConnected = false;

APPLEMIDI_CREATE_INSTANCE(WiFiUDP, AppleMIDI); // see definition in AppleMidi_Defs.h


//****** sample size sets the 'grain' of the detector
// a larger size will smooth over small variations
// a smaller size will excentuate small changes
const byte samplesize = 10; //set sample array size
const byte analysize = samplesize - 1;  //trim for analysis array,

volatile unsigned long microseconds; //sampling timer
volatile byte sampleIndex = 0;
volatile unsigned long samples[samplesize];

float threshold = 2.3; //threshold multiplier
float threshMin = 1.61; //scaling threshold min
float threshMax = 3.71; //scaling threshold max

//LEDs and Neopixels
#include <FastLED.h>
#define NUM_LEDS 5
#define CHIPSET WS2811
#define COLOR_ORDER GRB
#define LED_PIN 27
CRGB leds[NUM_LEDS];
byte maxBrightness = 10;


//MIDI Note and Controls
const byte polyphony = 5; // number of notes to track at a given time
byte channel = 1;  //setting channel to 11 or 12 often helps simply computer midi routing setups
int noteMin = 36; //C2  - keyboard note minimum
int noteMax = 96; //C7  - keyboard note maximum
byte controlNumber = 80; //set to mappable control, low values may interfere with other soft synth controls!!

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


//******************************
//set scaled values, sorted array, first element scale length
int scaleMajor[]  = {7, 1, 3, 5, 6, 8, 10, 12};
int scaleDiaMinor[]  = {7, 1, 3, 4, 6, 8, 9, 11};
int scaleIndian[]  = {7, 1, 2, 2, 5, 6, 9, 11};
int scaleMinor[]  = {7, 1, 3, 4, 6, 8, 9, 11};
int scaleChrom[] = {12, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
int *scaleSelect = scaleChrom; //initialize scaling
int root = 0; //initialize for root
//*******************************

//Setup wifi MIDI
void setupWifiMIDI() {
  WiFi.mode(WIFI_AP_STA);
  if (debugSerial) Serial.print("Access Point: "); Serial.println(assid);
  WiFi.softAP(assid, apassword);
  if (debugSerial) Serial.print("Access Point IP Address: "); Serial.println(WiFi.softAPIP());
  
  if (debugSerial) Serial.print("Connecting to: "); Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (debugSerial) Serial.print(F("."));
  }
  if (debugSerial) {
    Serial.println(F(""));
    Serial.println(F("WiFi connected"));
    Serial.print(F("IP address is "));
    Serial.println(WiFi.localIP());
  
    Serial.println(F("RTP MIDI Connection"));
    Serial.print(F("Add device with Host/Port "));
    Serial.print(WiFi.localIP());
    Serial.println(F(":5004"));
    Serial.println(F("Then press the Connect button"));
    Serial.println(F("Then open a MIDI listener (eg MIDI-OX) and monitor incoming notes"));
  }

  
    // Create a session and wait for a remote host to connect to us
    if (debugSerial) AppleMIDI.begin("ESP32_MIDI");
  AppleMIDI.OnConnected(OnAppleMidiConnected);
  AppleMIDI.OnDisconnected(OnAppleMidiDisconnected);

  AppleMIDI.OnReceiveNoteOn(OnAppleMidiNoteOn);
  AppleMIDI.OnReceiveNoteOff(OnAppleMidiNoteOff);

   //server declarations
    server.on("/", handleRoot);
    server.on("/off", handle_off);
    server.on("/on", handle_on);
    server.on("/white", handle_white);
    server.onNotFound(handle_notFound);
    server.begin();
}

//setups for each MIDI type, provide led display output
void setupSerialMIDI() {
   if (debugSerial) Serial.println("MIDI set on Serial1 31250");
  Serial1.begin(31250);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void setup() {

  if (debugSerial) Serial.begin(115200); // Serial for debugging

 // pinMode(buttonPin, INPUT_PULLUP); //button managed by PinButton
  pinMode(interruptPin, INPUT_PULLUP); //pulse input

 //Neopixel LED strip
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(maxBrightness);
    leds[0] = CRGB::Red;
          fadeToBlackBy(leds,NUM_LEDS,35); 
    FastLED.show(); delay(250);
    leds[1] = CRGB::Yellow;
          fadeToBlackBy(leds,NUM_LEDS,35); 
    FastLED.show(); delay(250);
    leds[2] = CRGB::Green;
          fadeToBlackBy(leds,NUM_LEDS,35); 
    FastLED.show(); delay(250);
    leds[3] = CRGB::Blue;
          fadeToBlackBy(leds,NUM_LEDS,35); 
    FastLED.show(); delay(250);
    leds[4] = CRGB::White;
          fadeToBlackBy(leds,NUM_LEDS,35); 
    FastLED.show(); delay(350);

    //welcome message
    if (debugSerial)  Serial.println();
    if (debugSerial) Serial.println("Welcome to Biodata Sonification");
    if (debugSerial)  Serial.println("potentially printout variables/options, IP, etc");

//clear the LEDs in a fadeout
    currentMillis = millis();
    prevMillis = currentMillis;
    while(prevMillis + 3500 > currentMillis) { 
      currentMillis = millis();
      fadeToBlackBy(leds,NUM_LEDS,35); FastLED.show(); 
      delay(35);
    }
    FastLED.clear(); FastLED.show(); //cleanup!
    
//check if button is held, potentially for reset stuff
  if(!digitalRead(buttonPin)) {
     if (debugSerial) Serial.println("Button Held at Bootup!");
    leds[0] = CRGB::White;
    FastLED.show(); delay(500);
    FastLED.clear(); FastLED.show();
  }

  if (serialMIDI)  setupSerialMIDI(); // MIDI hardware serial output
  if (wifiMIDI) setupWifiMIDI(); //MIDI RTP and AppleMIDI setup
  
  //setup pulse input pin
  attachInterrupt(interruptPin, sample, RISING);  //begin sampling from interrupt

} //end setup(){}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void loop() {
  //manage time
  currentMillis = millis();

  //analyze data when the buffer is full
  if (sampleIndex >= samplesize)  {
    analyzeSample();
  }

//   if (debugSerial) Serial.println(digitalRead(interruptPin));

  //Manage wifi page
  //server.handleClient();
  
  // Listen to incoming notes
  AppleMIDI.run(); //can probably remove this unless MIDI parameter change input!! woot!

  // Manage MIDI
  checkNote();  //turn off expired notes
  checkControl();  //update control value

  //Manage pot and button
  checkKnob();  // updates threshold and wheel rate

  
  button.update();

  if(button.isClick()) { 
     if (debugSerial) Serial.println("Clicked, but what kind hmm?");
    leds[4] = CRGB::White;
    FastLED.show(); 
  }
  if(button.isSingleClick()) {
       if (debugSerial) Serial.println("oh yeah single click, right");
  }
  //if double, if long click, etc
  
  //create menu structure and manage mode

  //Manage LEDs
  fadeToBlackBy(leds,NUM_LEDS,10); //always fade out, maintain color in subroutines
  FastLED.show();

} //end loop(){}

void checkKnob() {
  //float knobValue
  threshold = analogRead(potPin); 
  
  //set threshold to knobValue mapping
  threshold = mapfloat(threshold, 0, 4095, threshMin, threshMax);
}

//provide float map function
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
