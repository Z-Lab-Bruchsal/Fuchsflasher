#pragma once

#include <Arduino.h>

#include "isp.h"

// Kompletter Flashvorgang inkl. LED-Anzeige.
namespace flasher {

// Wert = Anzahl der Blinkimpulse der roten LED.
enum class Error : uint8_t {
  None = 0,
  NoResponse = 1,
  BadSignature = 2,
  Verify = 3,
  NoImage = 4,
  Fuses = 5,
  Timeout = 6,
};

enum class State : uint8_t { Idle, Busy, Ok, Failed };

// Automatik-Modus: Sockel abfragen, neuen Chip flashen, auf Entnahme warten.
enum class AutoState : uint8_t { Off, WaitChip, WaitRemoval };

struct Report {
  Error error = Error::None;
  isp::Signature sig = {{0, 0, 0}};
  isp::Fuses fuses = {0, 0, 0};
  bool fusesValid = false;
  uint32_t durationMs = 0;
};

void begin();
// Blockiert für die Dauer des Flashens (~1–3 s).
const Report& run();
// Aus loop() aufrufen: Automatik-Modus und LED-Blinken.
void tick();

State state();
AutoState autoState();
bool hasReport();
const Report& last();
const char* errorName(Error e);

}  // namespace flasher
