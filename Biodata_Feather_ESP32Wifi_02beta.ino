

// **-----------------------------------------------------------------------------
//ESP32 test for UDP/RTP AppleMIDI over wifi
//Biodata Sonification pulse input data via interrupt
//Knob and button interface
//Neopixel LEDs (5)
//ESP32 Feather A13 pin reads half of battery voltage (4.2 (3.7) -3.2V)
// **-----------------------------------------------------------------------------

// Wifi Credentials ~~~~~~~~~~~!!!!
char ssid[] = "OxfordEast"; //  your network SSID (name)
char pass[] = "oxfordeast";    // your network password (use for WPA, or use as key for WEP)
// ~~~~~~~~~~~~~!!!!
//  Set the MIDI Channel of this node
// if running multiple nodes either use many RTP Sessions
// or change the midi Channel on each node and use a single Session
byte channel = 1; 



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
int neoPin = 27;  //11;//M0  //Neopixels attached to this pin
const byte interruptPin = 12; //galvanometer input

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
float threshMax = 3.71; //scaling threshold max
float prevThreshold = 0;

//LEDs and Neopixels
#include <FastLED.h>
#define NUM_LEDS 5
#define CHIPSET WS2811
#define COLOR_ORDER GRB
#define LED_PIN 27  //11//M0
CRGB leds[NUM_LEDS];
byte maxBrightness = 60;

//MIDI Note and Controls
const byte polyphony = 5; // number of notes to track at a given time
 //setting channel to 11 or 12 often helps simply computer midi routing setups
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
int scaleMajor[]  = {7, 0, 2, 4, 5, 7, 9, 11};
int scaleDiaMinor[]  = {7, 0, 2, 3, 5, 7, 8, 10};
int scaleIndian[]  = {7, 0, 1, 1, 4, 5, 8, 10};
int scaleMinor[]  = {7, 0, 2, 3, 5, 7, 8, 10};
int scaleChrom[] = {13, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
int *scaleSelect = scaleChrom; //initialize scaling
int root = 0; //initialize for root
//*******************************

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
  threshold = analogRead(potPin); 
  if(prevThreshold != threshold) { Serial.print("Thresh: "); Serial.println(threshold); 
                                   prevThreshold = threshold; }
  
  //set threshold to knobValue mapping
  threshold = mapfloat(threshold, 0, 4095, threshMin, threshMax);
  //Serial.println(threshold);
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
  Serial.print(F("Connecting to Wifi Network: "));
  Serial.print(ssid); Serial.print(F("  pass: ")); Serial.print(pass); Serial.println();

  WiFi.begin(ssid, pass);

//could get 'stuck' here if we can't connect ... hmm..
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  Serial.println(F(""));
  Serial.println(F("WiFi connected"));


  Serial.println();
  Serial.print(F("IP address is "));
  Serial.println(WiFi.localIP());

  // Create a session and wait for a remote host to connect to us
//  AppleMIDI.begin("ESP32_MIDI");
    MIDI.begin();
  //isConnected
  AppleMIDI.setHandleConnected([](const APPLEMIDI_NAMESPACE::ssrc_t & ssrc, const char* name) {
    isConnected = 1;
    DBG(F("Connected to session"), ssrc, name);
  });
  AppleMIDI.setHandleDisconnected([](const APPLEMIDI_NAMESPACE::ssrc_t & ssrc) {
    isConnected = 0;
    DBG(F("Disconnected"), ssrc);
  });

  
}
