// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void setup() {

  if (debugSerial) Serial.begin(115200); // Serial for debugging
  
      //load from EEPROM memory
  EEPROM.begin(EEPROM_SIZE);
  
  //pinMode(buttonPin, INPUT_PULLUP); //button managed by PinButton
  pinMode(interruptPin, INPUT_PULLUP); //pulse input

     //welcome message
    if(debugSerial) Serial.println(); Serial.println();
    if (debugSerial) Serial.println(F("Welcome to Biodata Sonification .. now with BLE and Wifi!"));

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
     if (debugSerial) Serial.println("Button Held at Bootup!");
     ledFaders[4].Set(255, 1000);
     ledFaders[4].Update();
     while(ledFaders[4].isRunning) {
        ledFaders[4].Update();
     }
     //reset memory - chromatic scale, channel 1, wifi off, ble on
      EEPROM.write(0, 0); EEPROM.write(1, channel); EEPROM.write(2,0); EEPROM.write(3,1);
      EEPROM.commit();
         bleMIDI = 1;
         wifiMIDI = 0;
         //channel = 1;  //declared at top
         scaleSelect = scaleChrom;

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
      EEPROM.write(0, 0); EEPROM.write(1, channel); EEPROM.write(2,0); EEPROM.write(3,1); EEPROM.write(4,1);
      EEPROM.commit();
      Serial.println("EEPROM Initialized - First time! BLE ON, WiFi OFF");
         scaleIndex = EEPROM.read(0);
         midiChannel = EEPROM.read(1);
         wifiPower = EEPROM.read(2);
         blePower = EEPROM.read(3);
  }
  
  channel = midiChannel; //need two bytes to hold up to 16 channels!!
  
                       if(scaleIndex == 0) scaleSelect = scaleChrom; 
                       if(scaleIndex == 1) scaleSelect = scaleMinor; 
                       if(scaleIndex == 2) scaleSelect = scaleDiaMinor; 
                       if(scaleIndex == 3) scaleSelect = scaleMajor; 
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
  
  button.update();

  if(button.isSingleClick()) {
       if (debugSerial) {
        Serial.println("---***---***---***---***---***---***");
          byte mac[6];
          WiFi.macAddress(mac);
          String macAddr =  String(mac[0],HEX) +String(mac[1],HEX) +String(mac[2],HEX) +String(mac[3],HEX) + String(mac[4],HEX) + String(mac[5],HEX);          
          int uniq =  mac[0] + mac[1] + mac[2] + mac[3] + mac[4] + mac[5];
          Serial.print("Biodata "); Serial.println(uniq);
          Serial.print("MIDI Channel "); Serial.println(channel);
          Serial.print("ScaleIndex "); Serial.println(EEPROM.read(0));
          Serial.print("Threshold "); Serial.println(threshold);
          Serial.print("MAC Address: "); Serial.println(macAddr);
          
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
        float batteryLevel = float(analogRead(35))/float(4095)*2.0*3.3*1.1; //A13
        Serial.print("Battery Voltage: ");
        Serial.println(batteryLevel); //of course while plugged into usb the voltage will be 4.x


        int knobValue = analogRead(potPin);
        int prevKnob = knobValue;
        unsigned long menuTimer = millis();
        int menu = 0; //main biodata play mode
        while(menuTimer + 10000 > millis()) {
           //knob turn to extend time and select menu
           knobValue = analogRead(potPin);
          // if(abs(knobValue - prevKnob) > 45){
            //select range for each menu and check for click
             menu = map(knobValue, 0, 4095, 0, 4);

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
             int modeValue = 0;
             if(button.isSingleClick()) {
                Serial.print("Menu "); Serial.println(menu);
                menuTimer = millis();
                while(menuTimer + 10000 > millis()){
                    
                    button.update();
                    //knob turn to extend time and select menu
                    knobValue = analogRead(potPin);
                   // if(abs(knobValue - prevKnob) > 45){  //if knob change
                      //menuTimer = millis();
                     //turn on Value LED, perhaps pulse?
                      for(byte i=0;i<5;i++) { ledFaders[i].Set(0,0); } //all off
 
                     //select range for each menu and check for click
                     
                      if(menu == 0) { // MIDI Scaling
                       modeValue = map(knobValue, 0, 4095, 0, 4);
                       //blink led during selection
                       if((blinkTime + 150) < millis()) { blinkToggle = !blinkToggle; blinkTime = millis(); }   
                       if(blinkToggle) ledFaders[modeValue].Set(ledFaders[modeValue].maxBright,0);
                       if(menu!=modeValue) ledFaders[menu].Set(ledFaders[menu].maxBright,0); //red0
                       
                       if(modeValue == 0) scaleSelect = scaleChrom; 
                       if(modeValue == 1) scaleSelect = scaleMinor; 
                       if(modeValue == 2) scaleSelect = scaleDiaMinor; 
                       if(modeValue == 3) scaleSelect = scaleMajor; 
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
                      if(menu==4) {
                          int batt = batteryLevel*10; 
                          modeValue = map(batt, 32,44,0,4);
                        if((blinkTime + 150) < millis()) { blinkToggle = !blinkToggle; blinkTime = millis(); }   
                        if(blinkToggle) ledFaders[modeValue].Set(ledFaders[modeValue].maxBright,0);
                        if(modeValue != menu) ledFaders[menu].Set(ledFaders[menu].maxBright,0); //white
                        
                      } 
                            
                      if(button.isSingleClick()) { ///select value sub menu
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
                          Serial.print("MIDI Scale "); Serial.println(modeValue);
                        }
                        if(menu == 1){
                          channel = modeValue;  //set channel value
                          EEPROM.write(1,channel);
                        //  EEPROM.commit();
                          Serial.print("Channel "); Serial.println(channel);
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
                                wifiMIDI = 1; 
                                EEPROM.write(2,1);
                                setupWifi();  //turn on wifi and initialize (turn on, connect)
                             }   
                        }
                        if(menu == 3) {
                              if(modeValue == 0)  { bleMIDI = 0; btStop(); EEPROM.write(3,0); Serial.println("BLE Off");}//turn off bluetooth
                              if(modeValue == 1)  { bleMIDI = 1;  btStart(); bleSetup(); EEPROM.write(3,1); Serial.println("BLE On");}//turn on bluetooth
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

                        return; //break; //save here              
                      }
                    //}  //if knob change
                      //unsure if button clicks get here...
                   if(button.isSingleClick()) {
                     for(byte i=0;i<5;i++) { ledFaders[i].Set(0,0); } //all off
                     Serial.println("shouldnt get here in menus?");
                     return; //save here
                   }
                   prevKnob = knobValue;
              
                }
                
             } 
          
        }
        
        for(byte i=0;i<5;i++) { ledFaders[i].Set(0,0); } //all off


        
          
  }
  //if double, if long click, etc
  
  



} //end loop(){}
