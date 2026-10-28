// fuchsflasher – ATtiny85-Batch-Programmer.
// Chip einlegen, Taster drücken → Erase, Write, Verify, Fuses. LED zeigt das Ergebnis.

#include <Arduino.h>

#include "config.h"
#include "firmware_source.h"
#include "flasher.h"
#include "pins.h"
#include "webui.h"

namespace {

constexpr uint32_t DEBOUNCE_MS = 50;

bool wifiStarted = false;

bool buttonPressed() {
  static int lastState = HIGH;
  static uint32_t lastChange = 0;
  int state = digitalRead(PIN_BUTTON);
  if (state != lastState && millis() - lastChange > DEBOUNCE_MS) {
    lastChange = millis();
    lastState = state;
    return state == LOW;
  }
  return false;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  flasher::begin();
  config::load();
  Serial.println("\nfuchsflasher startet");

  // Notfall-Zugang: Taster beim Einschalten gedrückt → WLAN immer an
  delay(10);
  bool forceWifi = digitalRead(PIN_BUTTON) == LOW;

  firmware::begin();
  firmware::reload();

  if (config::get().wifiOn || forceWifi) {
    if (forceWifi) Serial.println("Taster gedrückt: WLAN wird erzwungen");
    webui::begin();
    wifiStarted = true;
  }

  // Den Notfall-Tastendruck nicht als Flash-Auftrag werten
  while (digitalRead(PIN_BUTTON) == LOW) delay(10);
  Serial.println("bereit – Chip einlegen und Taster drücken");
}

void loop() {
  if (wifiStarted) webui::handle();
  if (buttonPressed()) flasher::run();
  flasher::tick();
}
