

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
      if(serialMIDI) midiSerial(144, channel, value, velocity); //play note
      if(wifiMIDI&&isConnected) MIDI.sendNoteOn(value,velocity,channel);
      if (bleMIDI) {
              if (deviceConnected) {
              // note On
                midiPacket[2] = (144 | ((notechannel-1) & 0x0F)); //note On with channel
               // midiPacket[2] = 0x90; // note down, channel 0
                midiPacket[3] = value; //note
                midiPacket[4] = velocity;  // velocity
                pCharacteristic->setValue(midiPacket, 5); // packet, length in bytes
                pCharacteristic->notify();
              }
      }
        if (debugSerial) { 
          Serial.print("Note On "); 
          Serial.println(noteArray[i].value);
        }

        //setLED
        ledFaders[i].Set(ledFaders[i].maxBright,350);
      
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
  if(debugSerial) { 
    Serial.print("setControl CC"); Serial.print(controlNumber); Serial.print(" value "); Serial.println(velocity); 
  }

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
       if(serialMIDI) midiSerial(176, channel, controlMessage.type, controlMessage.value); 
       if(wifiMIDI&&isConnected) { MIDI.sendControlChange(controlNumber,controlMessage.value,channel); } //AppleMIDI.sendControlChange?
       if(deviceConnected) {   //bluetooth CC
                // note On
          midiPacket[2] = (176 | ((channel-1) & 0x0F)); //CC with base channel ** 
         // midiPacket[2] = 0x90; // note down, channel 0
          midiPacket[3] = controlNumber; //cc 80
          midiPacket[4] = controlMessage.value;
          pCharacteristic->setValue(midiPacket, 5); // packet, length in bytes
          pCharacteristic->notify();
        } 
    }
  }
}

void checkNote()
{
  for (int i = 0;i<polyphony;i++) {
    if(noteArray[i].velocity) {
      if (noteArray[i].duration <= currentMillis) {
        //send noteOff for all notes with expired duration    
        if(serialMIDI) midiSerial(144, channel, noteArray[i].value, 0); 
        if(wifiMIDI && isConnected) MIDI.sendNoteOff(noteArray[i].value, 0, channel);
        noteArray[i].velocity = 0;
        ledFaders[i].Set(0,800);//turn off leds fade out
        if (debugSerial) { 
          Serial.print("Note Off "); 
          Serial.println(noteArray[i].value);
        }
       if (bleMIDI) {
        if (deviceConnected) {
        // note On
          midiPacket[2] = (144 | ((noteArray[i].channel-1) & 0x0F)); //note On with channel
         // midiPacket[2] = 0x90; // note down, channel 0
          midiPacket[3] = noteArray[i].value; //note
          midiPacket[4] = noteArray[i].velocity;  // velocity
          pCharacteristic->setValue(midiPacket, 5); // packet, length in bytes
          pCharacteristic->notify();
        }
       }
      }
    }
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
