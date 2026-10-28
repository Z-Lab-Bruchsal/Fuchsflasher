#pragma once

#include <Arduino.h>

#include "isp.h"

namespace config {

enum class Source : uint8_t { Embedded = 0, Sd = 1 };

struct Settings {
  bool sdEnabled = false;
  Source source = Source::Embedded;
  String hexFile = "/target.hex";

  bool wifiOn = true;
  String apSsid = "fuchsflasher";
  String apPass = "fuchs1234";

  bool autoFlash = false;  // neuen Chip im Sockel erkennen und automatisch flashen
  bool verify = true;
  bool writeFuses = false;
  isp::Fuses fuses = {0x62, 0xDF, 0xFF};  // Werkszustand ATtiny85
  bool writeLock = false;
  uint8_t lock = 0xFF;

  uint32_t counterOk = 0;
  uint32_t counterFail = 0;
};

Settings& get();
void load();
void save();
void saveCounters();

// Fehlertext, falls die Fuses den Chip für ISP unerreichbar machen, sonst nullptr.
const char* fuseError(const isp::Fuses& f);
// Hinweis bei riskanter Taktquelle, sonst nullptr.
const char* fuseWarning(const isp::Fuses& f);

const char* sourceName(Source s);

}  // namespace config
