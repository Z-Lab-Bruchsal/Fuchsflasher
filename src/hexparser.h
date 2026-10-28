#pragma once

#include <Arduino.h>

#include "isp.h"

// Flash-Abbild des Targets, ungenutzte Bytes sind 0xFF.
struct Image {
  uint8_t data[isp::FLASH_SIZE];
  size_t size;  // höchste belegte Adresse + 1, auf Wortgrenze aufgerundet
};

// Intel-HEX → Image. Bei Fehler steht in `err` eine lesbare Meldung.
bool parseHexStream(Stream& in, Image& img, String& err);
bool parseHexString(const char* text, Image& img, String& err);
