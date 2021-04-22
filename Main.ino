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
    if (debugSerial) Serial.println(F("Welcome to Biodata Sonification .. now with Wifi!"));
    

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
  if (wifiMIDI) setupWifi();

  //setup pulse input pin
  attachInterrupt(interruptPin, sample, RISING);  //begin sampling from interrupt

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

//   if (debugSerial) Serial.println(digitalRead(interruptPin));

  // Manage MIDI
  checkNote();  //turn off expired notes
  checkControl();  //update control value

  //Manage pot and button
 // checkKnob();  // updates threshold and wheel rate

  
  button.update();

  if(button.isSingleClick()) {
       if (debugSerial) {
        if(wifiMIDI){
          if(isConnected) Serial.println(F("RTP MIDI Connected!"));
          else Serial.println("RTP MIDI Not Connected");
            Serial.println();
            Serial.print(F("IP address is "));
            Serial.println(WiFi.localIP());
          } 
        }  
  }
  //if double, if long click, etc
  
  //create menu structure and manage mode

  //Manage LEDs
  fadeToBlackBy(leds,NUM_LEDS,10); //always fade out, maintain color in subroutines
  FastLED.show();

} //end loop(){}
