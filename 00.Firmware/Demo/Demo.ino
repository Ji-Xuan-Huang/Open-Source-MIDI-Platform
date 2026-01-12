#include <Wire.h>
#include "Adafruit_MPR121.h"
#ifndef _BV
#define _BV(bit) (1 << (bit)) 
#endif
#define MIDI_BASE_NOTE 48
#define ALPHA 0.8  // 平滑係數 (0.0 ~ 1.0)

// You can have up to 4 on one i2c bus but one is enough for testing!
//Adafruit_MPR121 Key[4] = Adafruit_MPR121();
Adafruit_MPR121 Key[2] = {Adafruit_MPR121(), Adafruit_MPR121()};

// Keeps track of the last pins touched
// so we know when buttons are 'released'
uint16_t Currtouched[2] = {0, 0};
uint16_t Lasttouched[2] = {0, 0};

int AddrPin[] = {11,10}; // D9 -> MSB
int AddrPinValue[4][2] = {
                              {0,0},
                              {0,1},
                              {1,0},
                              {1,1},
                                       };
                                       
int KnobPin[] = {A0, A1};
int KnobValue[2][4] = {
                              {0,0,0,0},
                              {0,0,0,0},
                                            };
int LastKnobValue[2][4] = {
                              {0,0,0,0},
                              {0,0,0,0},
                                            };
int KnobCCNo[2][4] = {
                              {1,7,10,64},
                              {1,7,10,64},
                                            };
int KnobCCCH[2][4] = {
                              {0,0,0,0},
                              {9,9,9,9},
                                            };

int PadPin[] = {4, 5};
int PadValue[2][4] = {
                              {0,0,0,0},
                              {0,0,0,0},
                                            };
int LastPadValue[2][4] = {
                              {0,0,0,0},
                              {0,0,0,0},
                                            };
int PadNoteNo[2][4] = {
                              {35,37,42,51},
                              {50,43,36,38},
                                            };

void setup() {
  Serial.begin(9600);
  Serial1.begin(31250);
  Key[0].begin(0x5B);
  Key[1].begin(0x5C);
  
//  while (!Serial) { // needed to keep leonardo/micro from starting too fast!
//    delay(10);
//  }
//  Serial.println("Adafruit MPR121 Capacitive Touch sensor test"); 
//  
//  if (!Key[0].begin(0x5B)) {
//    Serial.println("MPR121 0x5B not found, check wiring?");
//  } else {
//    Serial.println("MPR121 0x5B found!");
//  }
//  
//  if (!Key[1].begin(0x5C)) {
//    Serial.println("MPR121 0x5C not found, check wiring?");
//  } else {
//    Serial.println("MPR121 0x5C found!");
//  }

  for (int i=0; i<2; i++){
    pinMode(AddrPin[i], OUTPUT);
  }
  
  for (int i=0; i<2; i++){
    pinMode(KnobPin[i], INPUT);
  }
  
  for (int i=0; i<2; i++){
    pinMode(PadPin[i], INPUT_PULLUP);
  }
} 

void loop() {
  for (uint8_t i=0; i<2; i++){ 
    // Get the currently touched pads
    Currtouched[i] = Key[i].touched();
    for (uint8_t j=0; j<12; j++) {
      // it if *is* touched and *wasnt* touched before, alert!
      if ((Currtouched[i] & _BV(j)) && !(Lasttouched[i] & _BV(j)) ) {
        Serial.print("NoteOn : ");
        Serial.println(MIDI_BASE_NOTE+j+12*i); 
        NoteOn(0x90, MIDI_BASE_NOTE+j+12*i, 127);
      }
      // if it *was* touched and now *isnt*, alert!
      if (!(Currtouched[i] & _BV(j)) && (Lasttouched[i] & _BV(j)) ) {
        Serial.print("NoteOff : ");
        Serial.println(MIDI_BASE_NOTE+j+12*i); 
        NoteOn(0x90, MIDI_BASE_NOTE+j+12*i, 0);
      }
    }
    // reset our state
    Lasttouched[i] = Currtouched[i];
  
    for (int j=0; j<4; j++){
      for (int k=0; k<2; k++){
        digitalWrite(AddrPin[k],AddrPinValue[j][k]);
      }
      KnobValue[i][j] = ALPHA * analogRead(KnobPin[i]) + (1 - ALPHA) * KnobValue[i][j];
      //KnobValue[i][j] = analogRead(KnobPin[i]);
      int currentVal = map(KnobValue[i][j], 0, 1023, 0, 127);
      int lastVal = map(LastKnobValue[i][j], 0, 1023, 0, 127);
      if ( currentVal != lastVal){
        Serial.print("CH#");
        Serial.print(KnobCCCH[i][j]+1);
        Serial.print("  CC#");
        Serial.print(KnobCCNo[i][j]);
        Serial.print("  Value=");
        Serial.println(currentVal);                   
        ControlChange(0xB0+KnobCCCH[i][j], KnobCCNo[i][j], currentVal);         
      }
      LastKnobValue[i][j] = KnobValue[i][j];      
    }  

    for (int j=0; j<4; j++){
      for (int k=0; k<2; k++){
        digitalWrite(AddrPin[k],AddrPinValue[j][k]);
      }
      PadValue[i][j] = digitalRead(PadPin[i]);
      if ( PadValue[i][j] != LastPadValue[i][j]){
        if (PadValue[i][j] == LOW) {
          Serial.print("NoteOn (");
          Serial.print(0x99);
          Serial.print(", ");
          Serial.print(PadNoteNo[i][j]);
          Serial.print(", ");    
          Serial.print(127);
          Serial.println(")");
          NoteOn(0x99, PadNoteNo[i][j], 127);
        }else if(PadValue[i][j] == HIGH){
          Serial.print("NoteOff (");
          Serial.print(0x99);
          Serial.print(", ");
          Serial.print(PadNoteNo[i][j]);
          Serial.print(", ");    
          Serial.print(0);
          Serial.println(")");
          NoteOn(0x99, PadNoteNo[i][j], 0);     
        }
      }
      LastPadValue[i][j] = PadValue[i][j];      
    }
  
  }
}

void NoteOn(int cmd, int pitch, int velocity) {
  Serial1.write(cmd);
  Serial1.write(pitch);
  Serial1.write(velocity);
}

void ControlChange(int nn, int cc, int vv) {
  Serial1.write(nn);
  Serial1.write(cc);
  Serial1.write(vv);
}
