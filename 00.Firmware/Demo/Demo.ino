#include <MIDI.h>
#include <Wire.h>
#include "Adafruit_MPR121.h"

#define MIDI_BASE_NOTE 36

const uint8_t MIDI_CHANNEL = 1;
const uint8_t ButtonPin = 7;
const uint8_t MPR121Count = 4;
const uint8_t KeysPerMPR121 = 12;
const uint8_t MPR121Addresses[MPR121Count] = {0x5A, 0x5B, 0x5C, 0x5D};
const int8_t OctaveOffsets[] = {-1, 0, 1, 2};
const unsigned long ButtonDebounceMs = 30;

Adafruit_MPR121 Keys[MPR121Count];
uint16_t currentTouched[MPR121Count] = {0};
uint16_t lastTouched[MPR121Count] = {0};
uint8_t activeMidiNotes[MPR121Count][KeysPerMPR121] = {{0}};

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

uint8_t activeOctave = 1;
int lastButtonReading = HIGH;
int buttonState = HIGH;
unsigned long lastDebounceTime = 0;

uint8_t getMidiNote(uint8_t moduleIndex, uint8_t keyIndex) {
  return MIDI_BASE_NOTE + moduleIndex * KeysPerMPR121 + keyIndex +
         12 * OctaveOffsets[activeOctave];
}

void retuneHeldNotes() {
  for (uint8_t moduleIndex = 0; moduleIndex < MPR121Count; moduleIndex++) {
    for (uint8_t keyIndex = 0; keyIndex < KeysPerMPR121; keyIndex++) {
      if (lastTouched[moduleIndex] & (1U << keyIndex)) {
        MIDI.sendNoteOff(activeMidiNotes[moduleIndex][keyIndex], 0, MIDI_CHANNEL);
        activeMidiNotes[moduleIndex][keyIndex] =
            getMidiNote(moduleIndex, keyIndex);
        MIDI.sendNoteOn(activeMidiNotes[moduleIndex][keyIndex], 127,
                        MIDI_CHANNEL);
      }
    }
  }
}

void readOctaveButton() {
  int buttonReading = digitalRead(ButtonPin);
  if (buttonReading != lastButtonReading) {
    lastDebounceTime = millis();
    lastButtonReading = buttonReading;
  }

  if (millis() - lastDebounceTime >= ButtonDebounceMs &&
      buttonReading != buttonState) {
    buttonState = buttonReading;
    if (buttonState == LOW) {
      activeOctave = (activeOctave + 1) %
                     (sizeof(OctaveOffsets) / sizeof(OctaveOffsets[0]));
      retuneHeldNotes();
    }
  }
}

void setup() {
  MIDI.begin(MIDI_CHANNEL_OMNI);
  pinMode(ButtonPin, INPUT);

  for (uint8_t moduleIndex = 0; moduleIndex < MPR121Count; moduleIndex++) {
    Keys[moduleIndex].begin(MPR121Addresses[moduleIndex]);
  }
}

void loop() {
  readOctaveButton();

  for (uint8_t moduleIndex = 0; moduleIndex < MPR121Count; moduleIndex++) {
    currentTouched[moduleIndex] = Keys[moduleIndex].touched();
    for (uint8_t keyIndex = 0; keyIndex < KeysPerMPR121; keyIndex++) {
      const uint16_t keyMask = 1U << keyIndex;
      if ((currentTouched[moduleIndex] & keyMask) &&
          !(lastTouched[moduleIndex] & keyMask)) {
        activeMidiNotes[moduleIndex][keyIndex] =
            getMidiNote(moduleIndex, keyIndex);
        MIDI.sendNoteOn(activeMidiNotes[moduleIndex][keyIndex], 127,
                        MIDI_CHANNEL);
      } else if (!(currentTouched[moduleIndex] & keyMask) &&
                 (lastTouched[moduleIndex] & keyMask)) {
        MIDI.sendNoteOff(activeMidiNotes[moduleIndex][keyIndex], 0,
                         MIDI_CHANNEL);
      }
    }
    lastTouched[moduleIndex] = currentTouched[moduleIndex];
  }
}
