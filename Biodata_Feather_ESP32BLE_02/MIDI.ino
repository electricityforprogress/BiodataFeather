

//Control different MIDI output methods - message type, channel, note/number, velcoity/value
void midiSerial(int type, int channel, int data1, int data2) {

//Hardware Serial 31250 MIDI data
    //  Note type = 144
    //  Control type = 176  
    // remove MSBs on data
    data1 &= 0x7F;  //number
    data2 &= 0x7F;  //velocity
    byte statusbyte = (type | ((channel-1) & 0x0F));
    SerialMIDI.write(statusbyte);
    SerialMIDI.write(data1);
    SerialMIDI.write(data2);

}

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
      prevNoteMillis = currentMillis;
//*************
      if(serialMIDI) midiSerial(144, channel, value, velocity); //play note
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
       midiSerial(176, channel, controlMessage.type, controlMessage.value); 
       if (bleMIDI) {
        if (deviceConnected) {
        // send CC
          midiPacket[2] = (176 | ((controlMessage.channel) & 0x0F)); //note On with channel
         // midiPacket[2] = 0x90; // note down, channel 0
          midiPacket[3] = controlMessage.type; //note
          midiPacket[4] = controlMessage.value;  // velocity
          pCharacteristic->setValue(midiPacket, 5); // packet, length in bytes
          pCharacteristic->notify();
        }
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
      leds[i] = CHSV(noteColor, 255, 255);
      FastLED.show();
   
      if (noteArray[i].duration <= currentMillis) {
        //send noteOff for all notes with expired duration    
        midiSerial(144, channel, noteArray[i].value, 0); 
        noteArray[i].velocity = 0;

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
