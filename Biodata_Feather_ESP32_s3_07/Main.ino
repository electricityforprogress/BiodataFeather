// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void setup() {

  if (debugSerial || rawSerial) Serial.begin(115200); // Serial baud for debugging and raw
  
      //load from EEPROM memory
  EEPROM.begin(EEPROM_SIZE);

    //welcome message
    if(debugSerial) Serial.println(); Serial.println();
    if(debugSerial) Serial.println(F("Welcome to Biodata Sonification .. now with BLE and Wifi!"));


  if(usbmidi) { //could get hung here if no USB? !!
    usbMIDI.begin(); //turn on USB-MIDI
    // Wait until device is enumerated properly before sending MIDI message
   // while( !TinyUSBDevice.mounted() ) delay(1);
  }
  
  //pinMode(buttonPin, INPUT_PULLUP); //button managed by PinButton
  pinMode(interruptPin, INPUT_PULLUP); //pulse input

  button.begin();
 
//check if button is held at startup, potentially for reset stuff
 

    //LED light show
    //ledcSetup(0,5000,13);
    for(byte i=0;i<5;i++) {
        ledFaders[i].Setup(i);
        ledFaders[i].Set(ledFaders[i].maxBright,500*(i+1)); 
    }
    unsigned long prevMillis = millis();
    while(prevMillis+1000>millis()) {
        for(byte i=0;i<5;i++) ledFaders[i].Update();
    }
    for(byte i=0;i<5;i++) {
        ledFaders[i].Set(0,500*(i+1)); 
    }
    prevMillis = millis();
    while(prevMillis+3000>millis()) {
        for(byte i=0;i<5;i++) ledFaders[i].Update();
    }
   

 if(!digitalRead(buttonPin)) {
     if (debugSerial) Serial.println("Button Held at Bootup - Reset!");
     ledFaders[4].Set(255, 1000);
     ledFaders[4].Update();
     while(ledFaders[4].isRunning) {
        ledFaders[4].Update();
     }
     //reset memory - chromatic scale, channel 1, wifi off, ble on
      EEPROM.write(0, defScale); EEPROM.write(1, channel); EEPROM.write(2,0); EEPROM.write(3,1);
      EEPROM.commit();
         bleMIDI = 1;
         wifiMIDI = 0;
         //channel = 1;  //declared at top
         scaleSelect = scalePenta;

     ledFaders[4].Set(0, 0); //does this set immediately?

  }

//read from memory and load
  byte scaleIndex = EEPROM.read(0);
  byte midiChannel = EEPROM.read(1);
  byte wifiPower = EEPROM.read(2);
  byte blePower = EEPROM.read(3);
  byte keybyte = EEPROM.read(4);
  if(keybyte != 1) { //if not initialized first time - Scale,channel,wifi,bluetooth, key
    //init for millersville
    //EEPROM.write(0, 0); EEPROM.write(1, 1); EEPROM.write(2,1); EEPROM.write(3,0); EEPROM.write(4,1);
     //normal init - ble ON, wifi OFF
      EEPROM.write(0, defScale); EEPROM.write(1, channel); EEPROM.write(2,0); EEPROM.write(3,1); EEPROM.write(4,1);
      EEPROM.commit();
      if (debugSerial) Serial.println("EEPROM Initialized - First time! BLE ON, WiFi OFF");
         scaleIndex = EEPROM.read(0);
         midiChannel = EEPROM.read(1);
         wifiPower = EEPROM.read(2);
         blePower = EEPROM.read(3);
  }
  
  channel = midiChannel; //need two bytes to hold up to 16 channels!!
  
                       if(scaleIndex == 0) scaleSelect = scaleChrom; 
                       if(scaleIndex == 1) scaleSelect = scaleMinor; 
                       if(scaleIndex == 2) scaleSelect = scaleMajor; 
                       if(scaleIndex == 3) scaleSelect = scalePenta; 
                       if(scaleIndex == 4) scaleSelect = scaleIndian; 
  wifiMIDI = wifiPower;
  bleMIDI = blePower;
  

  if (serialMIDI)  setupSerialMIDI(); // MIDI hardware serial output
  if (wifiMIDI)    setupWifi(); 
  else { WiFi.disconnect(true); delay(1);   WiFi.mode(WIFI_OFF); delay(1);} //turn wifi radio off
  if (bleMIDI)     bleSetup();

  //setup pulse input pin
  attachInterrupt(interruptPin, sample, RISING);  //begin sampling from interrupt

  for(byte i=0;i<5;i++) { ledFaders[i].Update(); ledFaders[i].Set(0,2000); } //all fade off

} //end setup(){}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void loop() {
  //manage time
  currentMillis = millis();
  MIDI.read();
  
  //analyze data when the buffer is full
  if (sampleIndex >= samplesize)  {
    analyzeSample();
  }

  // Manage MIDI
  checkNote();  //turn off expired notes
  checkControl();  //update control value

  // Mange LEDs
  for(byte i=0;i<5;i++) ledFaders[i].Update();
  
  //Manage pot and button
   checkKnob();  // updates threshold in main biodata mode
 

//this keeps the Red LED on when wifi is not connected
  if(wifiMIDI && WiFi.status() != WL_CONNECTED) { ledFaders[0].Set(ledFaders[0].maxBright,0); } 

  checkButton();

} //end loop(){}
