//*******Analysis of sample data
/*
 * Fill an array with samples
 * calcluate max, min, delta, average, standard deviation
 * compare against a 'change threshold' delta > (stdevi * threshold)
 * when change is detected map note pitch, velocity, duration
 * set the sample array size, the effective 'resolution' of change detection
 * using micros() for sample values, which is irratic, use a better method
 * sampling at the 'rising' edge, which counts a full period
 */
//*******


void sample(){
  if(sampleIndex < samplesize) {
    samples[sampleIndex] = micros() - microseconds;
    microseconds = samples[sampleIndex] + microseconds; //rebuild micros() value w/o recalling
    //micros() is very slow
    //try a higher precision counter
    //samples[index] = ((timer0_overflow_count << 8) + TCNT0) - microseconds;
    sampleIndex += 1;
    //Serial.println(microseconds);
  } 
}

void analyzeSample()
{
  unsigned long averg = 0;
  unsigned long maxim = 0;
  unsigned long minim = 100000;
  float stdevi = 0;
  unsigned long delta = 0;
  byte change = 0;

  if (sampleIndex >= samplesize) { //array is full
    unsigned long sampanalysis[analysize]; //copy to new array - is this needed?
    for (byte i=0; i<analysize; i++){ 
      //skip first element in the array
      sampanalysis[i] = samples[i+1];  //load analysis table (due to volitle)
      //manual calculation
      if(sampanalysis[i] > maxim) { maxim = sampanalysis[i]; }
      if(sampanalysis[i] < minim) { minim = sampanalysis[i]; }
      averg += sampanalysis[i];
      stdevi += sampanalysis[i] * sampanalysis[i];  //prep stdevi
    }

    //calculation
    averg = averg/analysize;
    stdevi = sqrt(stdevi / analysize - averg * averg); //calculate stdevu
    if (stdevi < 1) { stdevi = 1.0; } //min stdevi of 1
    delta = maxim - minim; 
    
    //**********perform change detection 
    if (delta > (stdevi * threshold)){
      change = 1;
    }
    //*********
    
    if(change){       
      
    //Serial.println("change");

      //analyze the values
       int dur = 150+(map(delta%127,1,127,minNoteDur,maxNoteDur)); //length of note
       int ramp = 3 + (dur%100) ; //control slide rate, min 25 (or 3 ;)
       
       //set scaling, root key, note
       int setnote = map(averg%127,1,127,noteMin,noteMax);  //derive note, min and max note
       setnote = scaleNote(setnote, scaleSelect, root);  //scale the note
       // setnote = setnote + root; // (apply root?)
       setNote(setnote, 100, dur, channel); 
  
       //derive control parameters and set    
       setControl(controlNumber, controlMessage.value, delta%127, ramp); //set the ramp rate for the control
      
    } 
       
    //reset array for next sample
    sampleIndex = 0;
  }
}
