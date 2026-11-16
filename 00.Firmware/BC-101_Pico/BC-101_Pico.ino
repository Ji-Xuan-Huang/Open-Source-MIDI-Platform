#include <MIDI.h>
#include <Wire.h>
#include <rgb_lcd.h>
#include "Adafruit_MPR121.h"
#ifndef MIDI_SOFT_THRU
#define MIDI_SOFT_THRU 1
#endif
#ifndef _BV
#define _BV(bit) (1 << (bit)) 
#endif
#define MIDI_BASE_NOTE 36

const uint8_t MIDI_VOLUME_CC = 7;
const uint8_t MIDI_MODULATION_CC = 1;
const uint8_t MIDI_CHANNEL = 1;
const uint8_t MIDI_PAD_CHANNEL = 10;
const uint8_t ButtonPin = 7;
const uint8_t PotentiometerPin = A6;
const uint8_t JoystickXPin = A0;
const uint8_t JoystickYPin = A1;
const int JoystickXMinimum = 240;
const int JoystickXCenter = 510;
const int JoystickXMaximum = 778;
const int JoystickYMinimum = 244;
const int JoystickYCenter = 509;
const int JoystickYMaximum = 774;
const uint8_t EncoderPinA = 8;
const uint8_t EncoderPinB = 9;
const uint8_t LedPins[] = {5, 6, 10, 11};
const int8_t OctaveOffsets[] = {-1, 0, 1, 2};
const uint8_t PadNoteNumbers[12] = {
  49, 42, 38, 36, 127, 127, 127, 127, 127, 127, 127, 127
};
const uint8_t LedCount = sizeof(LedPins) / sizeof(LedPins[0]);
const unsigned long ButtonDebounceMs = 30;
const int PotentiometerHysteresis = 3;
const int JoystickHysteresis = 3;
const uint8_t EncoderMinimum = 0;
const uint8_t EncoderMaximum = 127;
const uint8_t LcdColumns = 16;

Adafruit_MPR121 Key[2] = {Adafruit_MPR121(), Adafruit_MPR121()};
rgb_lcd lcd;

// Keeps track of the last pins touched
// so we know when buttons are 'released'
uint16_t Currtouched[2] = {0, 0};
uint16_t Lasttouched[2] = {0, 0};
uint8_t ActiveMidiNotes[12] = {0};

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

uint8_t activeLed = 1;
int lastPotentiometerValue = -1;
int lastModulationRawValue = -1;
int lastModulationValue = -1;
int lastPitchBendRawValue = -1;
int lastPitchBendValue = 0;
int lastButtonReading = HIGH;
int buttonState = HIGH;
unsigned long lastDebounceTime = 0;
uint8_t lastEncoderState = 0;
int8_t encoderStep = 0;
uint8_t encoderValue = EncoderMinimum;

const int8_t EncoderTransitions[16] = {
  0, 1, -1, 0,
  -1, 0, 0, 1,
  1, 0, 0, -1,
  0, -1, 1, 0
};

void sendProgramChange() {
  Serial.print("Program Change : ");
  Serial.println(encoderValue + 1);
  updateLcdLine(0, "Prog Change: ", encoderValue + 1);
  MIDI.sendProgramChange(encoderValue, MIDI_CHANNEL);
}

void sendVolume(int value) {
  MIDI.sendControlChange(MIDI_VOLUME_CC, value, MIDI_CHANNEL);
  MIDI.sendControlChange(MIDI_VOLUME_CC, value, MIDI_PAD_CHANNEL);
}

void updateLcdLine(uint8_t row, const char *label, uint16_t value) {
  lcd.setCursor(0, row);
  lcd.print(label);
  lcd.print(value);

  uint8_t charactersPrinted = strlen(label);
  uint16_t remainingValue = value;
  do {
    charactersPrinted++;
    remainingValue /= 10;
  } while (remainingValue > 0);

  while (charactersPrinted++ < LcdColumns) {
    lcd.print(' ');
  }
}

void selectLed(uint8_t ledIndex) {
  for (uint8_t i = 0; i < LedCount; i++) {
    digitalWrite(LedPins[i], i == ledIndex ? HIGH : LOW);
  }
}

uint8_t getMidiNote(uint8_t keyIndex) {
  return MIDI_BASE_NOTE + keyIndex + 12 * OctaveOffsets[activeLed];
}

void printOctave() {
  int8_t octave = OctaveOffsets[activeLed];
  Serial.print("Octave ");
  if (octave > 0) {
    Serial.print("+");
  }
  Serial.println(octave);
}

void retuneHeldNotes() {
  for (uint8_t j = 0; j < 12; j++) {
    if (Lasttouched[0] & _BV(j)) {
      Serial.print("NoteOff : ");
      Serial.println(ActiveMidiNotes[j]);
      updateLcdLine(1, "Note Off: ", ActiveMidiNotes[j]);
      MIDI.sendNoteOff(ActiveMidiNotes[j], 0, MIDI_CHANNEL);
      ActiveMidiNotes[j] = getMidiNote(j);
      Serial.print("NoteOn : ");
      Serial.println(ActiveMidiNotes[j]);
      updateLcdLine(1, "Note On: ", ActiveMidiNotes[j]);
      MIDI.sendNoteOn(ActiveMidiNotes[j], 127, MIDI_CHANNEL);
    }
  }
}

void readEncoder() {
  uint8_t encoderState = (digitalRead(EncoderPinA) << 1) |
                         digitalRead(EncoderPinB);
  uint8_t transition = (lastEncoderState << 2) | encoderState;
  encoderStep += EncoderTransitions[transition];
  lastEncoderState = encoderState;

  if (encoderStep >= 4) {
    if (encoderValue == EncoderMaximum) {
      encoderValue = EncoderMinimum;
    } else {
      encoderValue++;
    }
    sendProgramChange();
    encoderStep = 0;
  } else if (encoderStep <= -4) {
    if (encoderValue == EncoderMinimum) {
      encoderValue = EncoderMaximum;
    } else {
      encoderValue--;
    }
    sendProgramChange();
    encoderStep = 0;
  }
}

bool potentiometerValueChanged(int rawValue, int newValue) {
  if (newValue == lastPotentiometerValue) {
    return false;
  }

  const bool increasing = newValue > lastPotentiometerValue;
  const int boundaryValue = lastPotentiometerValue + (increasing ? 1 : 0);
  const int boundaryRaw = (boundaryValue * 1023L + 126) / 127;

  if (increasing) {
    const int threshold = min(1023, boundaryRaw + PotentiometerHysteresis);
    return rawValue >= threshold;
  }
  const int threshold = max(0, boundaryRaw - PotentiometerHysteresis);
  return rawValue <= threshold;
}

void updatePitchBend(int rawValue) {
  rawValue = constrain(rawValue, JoystickXMinimum, JoystickXMaximum);
  int pitchBendValue;

  if (abs(rawValue - JoystickXCenter) <= JoystickHysteresis) {
    pitchBendValue = 0;
  } else if (rawValue < JoystickXCenter) {
    pitchBendValue = (long)(rawValue - JoystickXCenter) * -MIDI_PITCHBEND_MIN /
                     (JoystickXCenter - JoystickXMinimum);
  } else {
    pitchBendValue = (long)(rawValue - JoystickXCenter) * MIDI_PITCHBEND_MAX /
                     (JoystickXMaximum - JoystickXCenter);
  }

  if (lastPitchBendRawValue < 0 ||
      abs(rawValue - lastPitchBendRawValue) >= JoystickHysteresis) {
    if (pitchBendValue != lastPitchBendValue) {
      Serial.print("Pitch Bend : ");
      Serial.println(pitchBendValue);
      lcd.setCursor(0, 1);
      lcd.print("Pitch Bend ON   ");
      MIDI.sendPitchBend(pitchBendValue, MIDI_CHANNEL);
      lastPitchBendValue = pitchBendValue;
    }
    lastPitchBendRawValue = rawValue;
  }
}

void updateModulation(int rawValue) {
  rawValue = constrain(rawValue, JoystickYMinimum, JoystickYMaximum);
  int modulationValue;

  if (abs(rawValue - JoystickYCenter) <= JoystickHysteresis) {
    modulationValue = 0;
  } else if (rawValue < JoystickYCenter) {
    modulationValue = (long)(JoystickYCenter - rawValue) * 127 /
                      (JoystickYCenter - JoystickYMinimum);
  } else {
    modulationValue = (long)(rawValue - JoystickYCenter) * 127 /
                      (JoystickYMaximum - JoystickYCenter);
  }

  if (lastModulationRawValue < 0 ||
      abs(rawValue - lastModulationRawValue) >= JoystickHysteresis) {
    if (modulationValue != lastModulationValue) {
      Serial.print("Modulation : ");
      Serial.println(modulationValue);
      lcd.setCursor(0, 1);
      lcd.print("Modulation ON   ");
      MIDI.sendControlChange(MIDI_MODULATION_CC, modulationValue, MIDI_CHANNEL);
      lastModulationValue = modulationValue;
    }
    lastModulationRawValue = rawValue;
  }
}

void setup() {

  Serial.begin(9600);
  MIDI.begin(MIDI_CHANNEL_OMNI);
#if MIDI_SOFT_THRU
  MIDI.turnThruOn();
#else
  MIDI.turnThruOff();
#endif

  lcd.begin(16, 2);
  sendProgramChange();

  pinMode(ButtonPin, INPUT);
  pinMode(EncoderPinA, INPUT_PULLUP);
  pinMode(EncoderPinB, INPUT_PULLUP);
  lastEncoderState = (digitalRead(EncoderPinA) << 1) |
                     digitalRead(EncoderPinB);
  lastPotentiometerValue = map(analogRead(PotentiometerPin), 0, 1023, 0, 127);
  updateModulation(analogRead(JoystickYPin));
  updatePitchBend(analogRead(JoystickXPin));
  updateLcdLine(1, "Volume: ", lastPotentiometerValue);
  sendVolume(lastPotentiometerValue);
  for (uint8_t i = 0; i < LedCount; i++) {
    pinMode(LedPins[i], OUTPUT);
  }
  selectLed(activeLed);
  printOctave();
  
  Key[0].begin(0x5A);
  Key[1].begin(0x5B);

  pinMode(13, OUTPUT);
} 

void loop() {
#if MIDI_SOFT_THRU
  MIDI.read();
#endif

  readEncoder();

  int joystickXValue = analogRead(JoystickXPin);
  updatePitchBend(joystickXValue);

  updateModulation(analogRead(JoystickYPin));

  int rawPotentiometerValue = analogRead(PotentiometerPin);
  int potentiometerValue = map(rawPotentiometerValue, 0, 1023, 0, 127);
  if (potentiometerValueChanged(rawPotentiometerValue, potentiometerValue)) {
    Serial.print("Volume : ");
    Serial.println(potentiometerValue);
    updateLcdLine(1, "Volume: ", potentiometerValue);
    lastPotentiometerValue = potentiometerValue;
    sendVolume(potentiometerValue);
  }

  int buttonReading = digitalRead(ButtonPin);
  if (buttonReading != lastButtonReading) {
    lastDebounceTime = millis();
    lastButtonReading = buttonReading;
  }

  if (millis() - lastDebounceTime >= ButtonDebounceMs &&
      buttonReading != buttonState) {
    buttonState = buttonReading;
    if (buttonState == LOW) {
      activeLed = (activeLed + 1) % LedCount;
      selectLed(activeLed);
      printOctave();
      retuneHeldNotes();
    }
  }

  for (uint8_t i=0; i<1; i++){ 
    // Get the currently touched pads
    Currtouched[i] = Key[i].touched();
    for (uint8_t j=0; j<12; j++) {
      // it if *is* touched and *wasnt* touched before, alert!
      if ((Currtouched[i] & _BV(j)) && !(Lasttouched[i] & _BV(j)) ) {
        ActiveMidiNotes[j] = getMidiNote(j);
        Serial.print("NoteOn : ");
        Serial.println(ActiveMidiNotes[j]); 
        updateLcdLine(1, "Note On: ", ActiveMidiNotes[j]);
        MIDI.sendNoteOn(ActiveMidiNotes[j], 127, MIDI_CHANNEL);
        digitalWrite(13, HIGH);
      }
      // if it *was* touched and now *isnt*, alert!
      if (!(Currtouched[i] & _BV(j)) && (Lasttouched[i] & _BV(j)) ) {
        Serial.print("NoteOff : ");
        Serial.println(ActiveMidiNotes[j]); 
        updateLcdLine(1, "Note Off: ", ActiveMidiNotes[j]);
        MIDI.sendNoteOff(ActiveMidiNotes[j], 0, MIDI_CHANNEL);
        digitalWrite(13, LOW);
      }
    }
    // reset our state
    Lasttouched[i] = Currtouched[i];
  }

  Currtouched[1] = Key[1].touched();
  for (uint8_t j = 0; j < 12; j++) {
    if ((Currtouched[1] & _BV(j)) && !(Lasttouched[1] & _BV(j))) {
      Serial.print("PadOn : ");
      Serial.println(PadNoteNumbers[j]);
      MIDI.sendNoteOn(PadNoteNumbers[j], 127, MIDI_PAD_CHANNEL);
    }
    if (!(Currtouched[1] & _BV(j)) && (Lasttouched[1] & _BV(j))) {
      Serial.print("PadOff : ");
      Serial.println(PadNoteNumbers[j]);
      MIDI.sendNoteOff(PadNoteNumbers[j], 0, MIDI_PAD_CHANNEL);
    }
  }
  Lasttouched[1] = Currtouched[1];
}
