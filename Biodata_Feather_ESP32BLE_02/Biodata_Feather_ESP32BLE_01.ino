
// **-----------------------------------------------------------------------------
//ESP32 test for UDP/RTP AppleMIDI over wifi
//Biodata Sonification pulse input data via interrupt
//Knob and button interface
//Neopixel LEDs (5)
//Stretch to include BLE MIDI
//Stretch to include USB HID MIDI on seperate chip *promicro daughter?
//Stretch to include Serial MIDI I/O
// **-----------------------------------------------------------------------------

#include <HardwareSerial.h>

HardwareSerial SerialMIDI(1);
#define SerialTX 17
#define SerialRX 16

#include <PinButton.h>

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

//Debug and MIDI output Settings ********
byte debugSerial = 1; //debugging serial messages
byte serialMIDI = 1; //write serial data to MIDI hardware output
byte wifiMIDI = 0; //do all the fancy wifi stuff and RTP MIDI over AppleMIDI
byte bleMIDI = 1; //bluetooth midi
byte usbMIDI = 0; //usb MIDI connection
byte midiMode = 0;  //change mode for serial, ble, wifi, usb
byte wifiActive = 0; //turn the wifi on and off, needed for wifiMIDI apparently
byte midiControl = 1; //use channel 16 to recieve parameter changes? fancy!
// **************************************

// I/O Pin declarations
int buttonPin = 13;
int potPin = A2; //A0 is not usable, need something on the other ADC
int neoPin = 27; //Neopixels attached to this pin
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
unsigned long prevNoteMillis = 0;

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
byte maxBrightness = 15;

//MIDI Note and Controls
const byte polyphony = 5; // number of notes to track at a given time
byte channel = 1;  //setting channel to 11 or 12 often helps simply computer midi routing setups
int noteMin = 36; //C2  - keyboard note minimum
int noteMax = 96; //C7  - keyboard note maximum
byte controlNumber = 80; //set to mappable control, low values may interfere with other soft synth controls!!
int maxNoteDur = 3500;
int minNoteDur = 225;


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


void bleSetup() {

 
  uint64_t chipid = ESP.getEfuseMac();
  byte chip = (uint32_t)chipid;
  String chipStr = "BIODATA ";
  chipStr.concat(chip);
  char chipString[15];
  chipStr.toCharArray(chipString,15);
  BLEDevice::init(chipString);
  if(debugSerial)  { Serial.print("Biodata BLE "); Serial.println(chip); }
    
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

//setups for each MIDI type, provide led display output
void setupSerialMIDI() {
   if (debugSerial) Serial.println("MIDI set on Serial1 31250");
   //Serial1.begin(31250);
  SerialMIDI.begin(31250, SERIAL_8N1, SerialRX, SerialTX);
   
}



void checkKnob() {
  //float knobValue
  int potReading = analogRead(potPin);
  //set threshold to knobValue mapping
  threshold = mapfloat(potReading, 0, 4095, threshMin, threshMax);
}

//provide float map function
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
