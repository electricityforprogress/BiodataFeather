

void setNote(int value, int velocity, long duration, int notechannel)
{
  //find available note in array (velocity = 0);
  for(int i=0;i<polyphony;i++){
    if(!noteArray[i].velocity){
      //if velocity is 0, replace note in array
      noteArray[i].type = 0;
      noteArray[i].value = value;
      noteArray[i].velocity = velocity;
      noteArray[i].duration = currentMillis + duration;
      noteArray[i].channel = notechannel;
      
//*************
      midiSerial(144, channel, value, velocity); //play note
      if (wifiMIDI && isConnected) AppleMIDI.sendNoteOn(value, velocity, notechannel);
      
      break; // exit for loop and continue once new note is placed
    }
  }
}

void setControl(int type, int value, int velocity, long duration)
{
  controlMessage.type = type;
  controlMessage.value = value;
  controlMessage.velocity = velocity;
  controlMessage.period = duration;
  controlMessage.duration = currentMillis + duration; //schedule for update cycle
}


void checkControl()
{
  //need to make this a smooth slide transition, using high precision 
  //distance is current minus goal
  signed int distance =  controlMessage.velocity - controlMessage.value; 
  //if still sliding
  if(distance != 0) {
    //check timing
    if(currentMillis>controlMessage.duration) { //and duration expired
        controlMessage.duration = currentMillis + controlMessage.period; //extend duration
        //update value
       if(distance > 0) { controlMessage.value += 1; } else { controlMessage.value -=1; }
       
       //send MIDI control message after ramp duration expires, on each increment
//*************
      //determine serial, wifi, or ble
       midiSerial(176, controlMessage.channel, controlMessage.type, controlMessage.value); 
         if (wifiMIDI && isConnected) {
          AppleMIDI.sendControlChange(controlMessage.velocity, controlMessage.value, controlMessage.channel);
         }

    }
  }
}

void checkNote()
{
  for (int i = 0;i<polyphony;i++) {
    if(noteArray[i].velocity) {
      //set the led and color
      byte noteColor = map(noteArray[i].value,noteMin,noteMax,0,255);
      leds[i] = CHSV(noteColor, 255, maxBrightness);
      FastLED.show();
   
      if (noteArray[i].duration <= currentMillis) {
        //send noteOff for all notes with expired duration    
        midiSerial(144, channel, noteArray[i].value, 0); 
        noteArray[i].velocity = 0;
        if (wifiMIDI && isConnected) AppleMIDI.sendNoteOff(noteArray[i].type, noteArray[i].value, noteArray[i].channel);

      }
    }
  }

}

// ====================================================================================
// Event handlers for incoming MIDI messages
// ====================================================================================

// -----------------------------------------------------------------------------
// rtpMIDI session. Device connected
// -----------------------------------------------------------------------------
void OnAppleMidiConnected(uint32_t ssrc, char* name) {
  isConnected  = true;
  if (debugSerial) Serial.print(F("Connected to rtpMIDI session "));
  if (debugSerial) Serial.println(name);
}

// -----------------------------------------------------------------------------
// rtpMIDI session. Device disconnected
// -----------------------------------------------------------------------------
void OnAppleMidiDisconnected(uint32_t ssrc) {
  isConnected  = false;
  if (debugSerial) Serial.println(F("Disconnected from RTP MIDI"));
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void OnAppleMidiNoteOn(byte channel, byte note, byte velocity) {
   if (debugSerial) {
    Serial.print(F("Incoming rtpMIDI NoteOn from channel:"));
    Serial.print(channel);
    Serial.print(F(" note:"));
    Serial.print(note);
    Serial.print(F(" velocity:"));
    Serial.print(velocity);
    Serial.println();
   }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void OnAppleMidiNoteOff(byte channel, byte note, byte velocity) {
   if (debugSerial) {
    Serial.print(F("Incoming rtpMIDI NoteOff from channel:"));
    Serial.print(channel);
    Serial.print(F(" note:"));
    Serial.print(note);
    Serial.print(F(" velocity:"));
    Serial.print(velocity);
    Serial.println();
   }
}


//Control different MIDI output methods - message type, channel, note/number, velcoity/value
void midiSerial(int type, int channel, int data1, int data2) {

//Hardware Serial 31250 MIDI data
  if(serialMIDI) {
    //  Note type = 144
    //  Control type = 176  
    // remove MSBs on data
    data1 &= 0x7F;  //number
    data2 &= 0x7F;  //velocity
    byte statusbyte = (type | ((channel-1) & 0x0F));
    Serial1.write(statusbyte);
    Serial1.write(data1);
    Serial1.write(data2);
  }
}
