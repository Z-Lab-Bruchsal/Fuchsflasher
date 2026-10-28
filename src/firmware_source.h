#pragma once

#include <Arduino.h>

#include "hexparser.h"

// Liefert das zu flashende Image aus der eingebetteten HEX oder von der SD-Karte.
namespace firmware {

// Bindet die SD-Karte ein, wenn sie in den Einstellungen aktiviert ist.
void begin();
bool sdMounted();

// Lädt das Image gemäß den aktuellen Einstellungen neu.
bool reload();
bool valid();
const Image& image();
const String& name();
const String& message();  // Fehlertext, wenn !valid()

// Prüft eine HEX-Datei auf der SD-Karte, ohne das aktive Image zu verändern.
bool validateFile(const String& path, String& err);

// Dateiname aus einer Benutzereingabe, z.B. "../a b.HEX" → "/ab.HEX". Leer bei ungültig.
String sanitizeName(const String& raw);

void appendLog(const String& line);

}  // namespace firmware
