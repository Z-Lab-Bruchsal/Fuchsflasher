#include "firmware_source.h"

#include <SD.h>
#include <SPI.h>

#include <memory>

#include "config.h"
#include "firmware_embedded.h"
#include "pins.h"

namespace firmware {

namespace {

constexpr uint32_t SD_FREQ = 4000000;
constexpr const char* LOG_FILE = "/log.csv";

Image active;
bool activeValid = false;
bool mounted = false;
String activeName;
String activeMessage;

}  // namespace

void begin() {
  if (!config::get().sdEnabled) return;
  SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  mounted = SD.begin(PIN_SD_CS, SPI, SD_FREQ);
  Serial.printf("SD-Karte: %s\n", mounted ? "eingebunden" : "nicht gefunden");
}

bool sdMounted() {
  return mounted;
}

bool reload() {
  const config::Settings& cfg = config::get();
  String err;

  if (cfg.source == config::Source::Embedded) {
    activeName = String("eingebettet: ") + EMBEDDED_HEX_NAME;
    activeValid = parseHexString(EMBEDDED_HEX, active, err);
  } else {
    activeName = cfg.hexFile;
    if (!cfg.sdEnabled) {
      err = "SD-Kartenleser ist deaktiviert";
      activeValid = false;
    } else if (!mounted) {
      err = "SD-Karte nicht eingebunden";
      activeValid = false;
    } else {
      File f = SD.open(cfg.hexFile, FILE_READ);
      if (!f) {
        err = "Datei nicht gefunden";
        activeValid = false;
      } else {
        activeValid = parseHexStream(f, active, err);
        f.close();
      }
    }
  }

  activeMessage = activeValid ? "" : err;
  Serial.printf("Image %s: %s\n", activeName.c_str(),
                activeValid ? (String(active.size) + " Bytes").c_str() : err.c_str());
  return activeValid;
}

bool valid() {
  return activeValid;
}

const Image& image() {
  return active;
}

const String& name() {
  return activeName;
}

const String& message() {
  return activeMessage;
}

bool validateFile(const String& path, String& err) {
  File f = SD.open(path, FILE_READ);
  if (!f) {
    err = "Datei nicht lesbar";
    return false;
  }
  std::unique_ptr<Image> tmp(new Image);
  bool ok = parseHexStream(f, *tmp, err);
  f.close();
  return ok;
}

String sanitizeName(const String& raw) {
  int slash = max(raw.lastIndexOf('/'), raw.lastIndexOf('\\'));
  String base = raw.substring(slash + 1);
  String out;
  for (char c : base) {
    if (isalnum(c) || c == '.' || c == '_' || c == '-') out += c;
  }
  String lower = out;
  lower.toLowerCase();
  if (out.length() < 5 || out.length() > 40 || !lower.endsWith(".hex") || out[0] == '.') {
    return "";
  }
  return "/" + out;
}

void appendLog(const String& line) {
  if (!mounted) return;
  bool isNew = !SD.exists(LOG_FILE);
  File f = SD.open(LOG_FILE, FILE_APPEND);
  if (!f) return;
  if (isNew) f.println("nr,uptime_s,ergebnis,signatur,image");
  f.println(line);
  f.close();
}

}  // namespace firmware
