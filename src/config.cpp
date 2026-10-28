#include "config.h"

#include <Preferences.h>

namespace config {

namespace {

constexpr const char* NVS_NAMESPACE = "fuchsflasher";

Settings settings;
Preferences prefs;

}  // namespace

Settings& get() {
  return settings;
}

void load() {
  const Settings def;
  prefs.begin(NVS_NAMESPACE, true);
  settings.sdEnabled   = prefs.getBool("sdEnabled", def.sdEnabled);
  settings.source      = Source(prefs.getUChar("source", uint8_t(def.source)));
  settings.hexFile     = prefs.getString("hexFile", def.hexFile);
  settings.wifiOn      = prefs.getBool("wifiOn", def.wifiOn);
  settings.apSsid      = prefs.getString("apSsid", def.apSsid);
  settings.apPass      = prefs.getString("apPass", def.apPass);
  settings.autoFlash   = prefs.getBool("autoFlash", def.autoFlash);
  settings.verify      = prefs.getBool("verify", def.verify);
  settings.writeFuses  = prefs.getBool("writeFuses", def.writeFuses);
  settings.fuses.low   = prefs.getUChar("lfuse", def.fuses.low);
  settings.fuses.high  = prefs.getUChar("hfuse", def.fuses.high);
  settings.fuses.ext   = prefs.getUChar("efuse", def.fuses.ext);
  settings.writeLock   = prefs.getBool("writeLock", def.writeLock);
  settings.lock        = prefs.getUChar("lock", def.lock);
  settings.counterOk   = prefs.getUInt("counterOk", 0);
  settings.counterFail = prefs.getUInt("counterFail", 0);
  prefs.end();

  if (settings.source != Source::Sd) settings.source = Source::Embedded;

  // Gespeicherte Fuses, die den Chip sperren würden, nie verwenden
  if (fuseError(settings.fuses)) {
    settings.fuses = def.fuses;
    settings.writeFuses = false;
  }
}

void save() {
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putBool("sdEnabled", settings.sdEnabled);
  prefs.putUChar("source", uint8_t(settings.source));
  prefs.putString("hexFile", settings.hexFile);
  prefs.putBool("wifiOn", settings.wifiOn);
  prefs.putString("apSsid", settings.apSsid);
  prefs.putString("apPass", settings.apPass);
  prefs.putBool("autoFlash", settings.autoFlash);
  prefs.putBool("verify", settings.verify);
  prefs.putBool("writeFuses", settings.writeFuses);
  prefs.putUChar("lfuse", settings.fuses.low);
  prefs.putUChar("hfuse", settings.fuses.high);
  prefs.putUChar("efuse", settings.fuses.ext);
  prefs.putBool("writeLock", settings.writeLock);
  prefs.putUChar("lock", settings.lock);
  prefs.end();
  saveCounters();
}

void saveCounters() {
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putUInt("counterOk", settings.counterOk);
  prefs.putUInt("counterFail", settings.counterFail);
  prefs.end();
}

const char* fuseError(const isp::Fuses& f) {
  // Fuse-Bits sind „programmiert“, wenn sie 0 sind.
  if (!(f.high & 0x80)) return "RSTDISBL programmiert: RESET-Pin wird I/O, ISP danach unmöglich";
  if (f.high & 0x20) return "SPIEN nicht programmiert: ISP danach unmöglich";
  return nullptr;
}

const char* fuseWarning(const isp::Fuses& f) {
  switch (f.low & 0x0F) {  // CKSEL3..0
    case 0x1:  // PLL 16 MHz
    case 0x2:  // interner RC 8 MHz
    case 0x3:  // ATtiny15-Kompatibilitätsmodus
      return nullptr;
    case 0x0:
      return "Externer Takt: ohne Taktsignal an CLKI ist kein weiteres ISP möglich";
    case 0x4:
      return "128-kHz-Oszillator: zu langsam für den ISP-Takt dieses Programmers";
    default:
      return "Quarz/Resonator: ohne angeschlossenen Quarz ist kein weiteres ISP möglich";
  }
}

const char* sourceName(Source s) {
  return s == Source::Sd ? "sd" : "embedded";
}

}  // namespace config
