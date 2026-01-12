#include <usbh_midi.h>
#include <usbhub.h>

#ifdef USBCON
#define _MIDI_SERIAL_PORT Serial1
#else
#define _MIDI_SERIAL_PORT Serial
#endif

#include <MIDI.h>
#include <Wire.h>
#include "Adafruit_MPR121.h"
#ifndef _BV
#define _BV(bit) (1 << (bit)) 
#endif
#define MIDI_BASE_NOTE 36
#define ALPHA 0.8  // 平滑係數 (0.0 ~ 1.0)
#define ENABLE_MIDI_SERIAL_FLUSH 0

// You can have up to 4 on one i2c bus but one is enough for testing!
//Adafruit_MPR121 Key[4] = Adafruit_MPR121();
Adafruit_MPR121 Key[4] = {Adafruit_MPR121(), Adafruit_MPR121(), Adafruit_MPR121(), Adafruit_MPR121()};

// Keeps track of the last pins touched
// so we know when buttons are 'released'
uint16_t Currtouched[4] = {0, 0, 0, 0};
uint16_t Lasttouched[4] = {0, 0, 0, 0};

MIDI_CREATE_DEFAULT_INSTANCE();

int AddrPin[] = {5,4}; // D5 -> MSB
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
                              {1,1,1,1},
                              {10,10,10,10},
                                            };

int PadPin[] = {6, 7};
int PadValue[2][4] = {
                              {0,0,0,0},
                              {0,0,0,0},
                                            };
int LastPadValue[2][4] = {
                              {0,0,0,0},
                              {0,0,0,0},
                                            };
int PadNoteNo[2][4] = {
                              {37,42,51,49},
                              {36,38,50,45},
                                            };
USB Usb;
USBH_MIDI  Midi(&Usb);

void MIDI_poll();
void setup() {

  _MIDI_SERIAL_PORT.begin(31250);

  if (Usb.Init() == -1) {
    while (1); //halt
  }//if (Usb.Init() == -1...
  delay( 200 );  

  Serial.begin(9600);
  Serial1.begin(31250);
  MIDI.begin(MIDI_CHANNEL_OMNI);           // 開始接收所有通道
  MIDI.turnThruOn();                       // ✅ 啟用 Soft Thru（預設是開的）

  // 可選：設定 Soft Thru 的過濾模式
  // MIDI.setThruFilterMode(midi::Thru::Full); // 預設是 Full
  
  Key[0].begin(0x5A);
  Key[1].begin(0x5B);
  Key[2].begin(0x5C);
  Key[3].begin(0x5D); 
  
  for (int i=0; i<2; i++){
    pinMode(AddrPin[i], OUTPUT);
    pinMode(KnobPin[i], INPUT);
    pinMode(PadPin[i], INPUT_PULLUP);
  }

  pinMode(13, OUTPUT);
} 

void loop() {
  Usb.Task();
  
  // 處理傳統 MIDI In 的 Soft Thru
  MIDI.read();  // 👈 這是必要的，否則 turnThruOn() 沒有作用
  
  if ( Midi ) {
    MIDI_poll();
  }
  //delay(1ms) if you want
  //delayMicroseconds(1000);  
    for (uint8_t i=0; i<=3; i++){ 
    // Get the currently touched pads
    Currtouched[i] = Key[i].touched();
    for (uint8_t j=0; j<12; j++) {
      // it if *is* touched and *wasnt* touched before, alert!
      if ((Currtouched[i] & _BV(j)) && !(Lasttouched[i] & _BV(j)) ) {
        Serial.print("NoteOn : ");
        Serial.println(MIDI_BASE_NOTE+j+12*i); 
        MIDI.sendNoteOn(MIDI_BASE_NOTE+j+12*i, 127, 1);
        digitalWrite(13, HIGH);
      }
      // if it *was* touched and now *isnt*, alert!
      if (!(Currtouched[i] & _BV(j)) && (Lasttouched[i] & _BV(j)) ) {
        Serial.print("NoteOff : ");
        Serial.println(MIDI_BASE_NOTE+j+12*i); 
        MIDI.sendNoteOff(MIDI_BASE_NOTE+j+12*i, 0, 1);
        digitalWrite(13, LOW);
      }
    }
    // reset our state
    Lasttouched[i] = Currtouched[i];
  }
  
  for (uint8_t i=0; i<2; i++){ 
    for (int j=0; j<4; j++){
      for (int k=0; k<2; k++){
        digitalWrite(AddrPin[k],AddrPinValue[j][k]);
      }
      KnobValue[i][j] = ALPHA * analogRead(KnobPin[i]) + (1 - ALPHA) * KnobValue[i][j];
      //KnobValue[i][j] = analogRead(KnobPin[i]);
      int currentVal = map(KnobValue[i][j], 0, 1023, 0, 127);
      int lastVal = map(LastKnobValue[i][j], 0, 1023, 0, 127);
      if ( currentVal != lastVal){
//        Serial.print("CH#");
//        Serial.print(KnobCCCH[i][j]+1);
//        Serial.print("  CC#");
//        Serial.print(KnobCCNo[i][j]);
//        Serial.print("  Value=");
//        Serial.println(currentVal);                   
        MIDI.sendControlChange(KnobCCNo[i][j], currentVal, KnobCCCH[i][j]);         
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
//          Serial.print("NoteOn (");
//          Serial.print(0x99);
//          Serial.print(", ");
//          Serial.print(PadNoteNo[i][j]);
//          Serial.print(", ");    
//          Serial.print(127);
//          Serial.println(")");
          MIDI.sendNoteOn(PadNoteNo[i][j], 127, 10);
        }else if(PadValue[i][j] == HIGH){
//          Serial.print("NoteOff (");
//          Serial.print(0x99);
//          Serial.print(", ");
//          Serial.print(PadNoteNo[i][j]);
//          Serial.print(", ");    
//          Serial.print(0);
//          Serial.println(")");
          MIDI.sendNoteOn(PadNoteNo[i][j], 0, 10);     
        }
      }
      LastPadValue[i][j] = PadValue[i][j];      
    }
  }
}

// Poll USB MIDI Controler and send to serial MIDI
void MIDI_poll()
{
  uint8_t outBuf[ 3 ];
  uint8_t size;

  do {
    if ( (size = Midi.RecvData(outBuf)) > 0 ) {
      //MIDI Output
      _MIDI_SERIAL_PORT.write(outBuf, size);
#if ENABLE_MIDI_SERIAL_FLUSH
      _MIDI_SERIAL_PORT.flush();
#endif
    }
  } while (size > 0);
}
