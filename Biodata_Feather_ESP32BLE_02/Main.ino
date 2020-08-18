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
         // fadeToBlackBy(leds,NUM_LEDS,255-maxBrightness); 
    FastLED.show(); delay(250);
    leds[1] = CRGB::Yellow;
         // fadeToBlackBy(leds,NUM_LEDS,255-maxBrightness); 
    FastLED.show(); delay(250);
    leds[2] = CRGB::Green;
         // fadeToBlackBy(leds,NUM_LEDS,255-maxBrightness); 
    FastLED.show(); delay(250);
    leds[3] = CRGB::Blue;
         // fadeToBlackBy(leds,NUM_LEDS,255-maxBrightness); 
    FastLED.show(); delay(250);
    leds[4] = CRGB::White;
       //   fadeToBlackBy(leds,NUM_LEDS,255-maxBrightness); 
    FastLED.show(); delay(350);

    //welcome message
    if (debugSerial)  Serial.println();
    if (debugSerial)  Serial.println("Welcome to Biodata Sonification");
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

    
  if (bleMIDI) bleSetup(); //MIDI BLE setup



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

  //show 'awake' notificaiton if no notes
  if(currentMillis > prevNoteMillis + 30000) {
    prevNoteMillis = currentMillis;
    if(deviceConnected) leds[0] = CRGB::Blue;
    else leds[0] = CRGB::Magenta;
    FastLED.delay(700);
    //fadeToBlackBy(leds,NUM_LEDS,255-maxBrightness);//set brightness
  }

  //Manage LEDs
  
  FastLED.show();
  fadeToBlackBy(leds,NUM_LEDS,10); //always fade out, maintain color in subroutines
} //end loop(){}
