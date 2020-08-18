

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
