#include "webui.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config.h"
#include "firmware_source.h"
#include "flasher.h"
#include "webui_page.h"

namespace webui {

namespace {

constexpr const char* UPLOAD_TMP = "/upload.tmp";
constexpr size_t MAX_UPLOAD = 64 * 1024;  // 8 KB Flash ≈ 23 KB HEX-Text
constexpr uint32_t REBOOT_DELAY_MS = 500;

WebServer server(80);
bool rebootPending = false;      // Einstellung geändert, die erst nach Neustart greift
uint32_t rebootAt = 0;           // geplanter Neustart, 0 = keiner

File uploadFile;
String uploadError;
String uploadName;
size_t uploadSize = 0;

bool sdActive() {
  return config::get().sdEnabled && firmware::sdMounted();
}

void sendJson(int code, const JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  server.send(code, "application/json", out);
}

void sendError(int code, const String& msg) {
  JsonDocument doc;
  doc["error"] = msg;
  sendJson(code, doc);
}

void sendOk() {
  JsonDocument doc;
  doc["ok"] = true;
  sendJson(200, doc);
}

// Datei-Routen gibt es nur mit aktivem SD-Kartenleser.
bool requireSd() {
  if (sdActive()) return true;
  sendError(404, "SD-Kartenleser nicht aktiv");
  return false;
}

String hexByte(uint8_t v) {
  char buf[3];
  snprintf(buf, sizeof(buf), "%02X", v);
  return buf;
}

const char* stateName(flasher::State s) {
  switch (s) {
    case flasher::State::Idle:   return "idle";
    case flasher::State::Busy:   return "busy";
    case flasher::State::Ok:     return "ok";
    case flasher::State::Failed: return "error";
  }
  return "?";
}

const char* autoStateName(flasher::AutoState s) {
  switch (s) {
    case flasher::AutoState::Off:         return "off";
    case flasher::AutoState::WaitChip:    return "waitChip";
    case flasher::AutoState::WaitRemoval: return "waitRemoval";
  }
  return "?";
}

void fillReport(JsonObject obj, const flasher::Report& rep) {
  obj["error"] = uint8_t(rep.error);
  obj["message"] = flasher::errorName(rep.error);
  obj["signature"] = hexByte(rep.sig.b[0]) + " " + hexByte(rep.sig.b[1]) + " " +
                     hexByte(rep.sig.b[2]);
  if (rep.fusesValid) {
    obj["lfuse"] = rep.fuses.low;
    obj["hfuse"] = rep.fuses.high;
    obj["efuse"] = rep.fuses.ext;
  }
  obj["durationMs"] = rep.durationMs;
}

void handleStatus() {
  const config::Settings& cfg = config::get();
  JsonDocument doc;
  doc["state"] = stateName(flasher::state());
  doc["auto"] = autoStateName(flasher::autoState());
  if (flasher::hasReport()) fillReport(doc["last"].to<JsonObject>(), flasher::last());
  doc["counterOk"] = cfg.counterOk;
  doc["counterFail"] = cfg.counterFail;
  JsonObject img = doc["image"].to<JsonObject>();
  img["name"] = firmware::name();
  img["valid"] = firmware::valid();
  img["size"] = firmware::valid() ? firmware::image().size : 0;
  img["message"] = firmware::message();
  doc["sdEnabled"] = cfg.sdEnabled;
  doc["sdMounted"] = firmware::sdMounted();
  doc["rebootPending"] = rebootPending;
  sendJson(200, doc);
}

void handleGetConfig() {
  const config::Settings& cfg = config::get();
  JsonDocument doc;
  doc["sdEnabled"] = cfg.sdEnabled;
  doc["source"] = config::sourceName(cfg.source);
  doc["hexFile"] = cfg.hexFile;
  doc["wifiOn"] = cfg.wifiOn;
  doc["apSsid"] = cfg.apSsid;
  doc["apPass"] = cfg.apPass;
  doc["autoFlash"] = cfg.autoFlash;
  doc["verify"] = cfg.verify;
  doc["writeFuses"] = cfg.writeFuses;
  doc["lfuse"] = cfg.fuses.low;
  doc["hfuse"] = cfg.fuses.high;
  doc["efuse"] = cfg.fuses.ext;
  doc["writeLock"] = cfg.writeLock;
  doc["lock"] = cfg.lock;
  sendJson(200, doc);
}

bool readByte(JsonVariantConst v, uint8_t& out) {
  if (!v.is<int>()) return false;
  int n = v.as<int>();
  if (n < 0 || n > 255) return false;
  out = n;
  return true;
}

void handlePostConfig() {
  JsonDocument in;
  if (deserializeJson(in, server.arg("plain"))) return sendError(400, "ungültiges JSON");

  // Auf einer Kopie prüfen, erst bei Erfolg übernehmen
  config::Settings next = config::get();

  if (in["sdEnabled"].is<bool>()) next.sdEnabled = in["sdEnabled"];
  if (in["wifiOn"].is<bool>()) next.wifiOn = in["wifiOn"];
  if (in["autoFlash"].is<bool>()) next.autoFlash = in["autoFlash"];
  if (in["verify"].is<bool>()) next.verify = in["verify"];
  if (in["writeFuses"].is<bool>()) next.writeFuses = in["writeFuses"];
  if (in["writeLock"].is<bool>()) next.writeLock = in["writeLock"];

  if (in["source"].is<const char*>()) {
    String s = in["source"].as<const char*>();
    if (s == "embedded") next.source = config::Source::Embedded;
    else if (s == "sd") next.source = config::Source::Sd;
    else return sendError(400, "Quelle muss 'embedded' oder 'sd' sein");
  }
  if (in["hexFile"].is<const char*>()) {
    String name = firmware::sanitizeName(in["hexFile"].as<const char*>());
    if (name.isEmpty()) return sendError(400, "ungültiger Dateiname (*.hex)");
    next.hexFile = name;
  }
  if (in["apSsid"].is<const char*>()) {
    next.apSsid = in["apSsid"].as<const char*>();
    if (next.apSsid.isEmpty() || next.apSsid.length() > 32) {
      return sendError(400, "SSID muss 1–32 Zeichen lang sein");
    }
  }
  if (in["apPass"].is<const char*>()) {
    next.apPass = in["apPass"].as<const char*>();
    size_t len = next.apPass.length();
    if (len != 0 && (len < 8 || len > 63)) {
      return sendError(400, "WLAN-Passwort: leer (offen) oder 8–63 Zeichen");
    }
  }

  const char* byteKeys[] = {"lfuse", "hfuse", "efuse", "lock"};
  uint8_t* byteDest[] = {&next.fuses.low, &next.fuses.high, &next.fuses.ext, &next.lock};
  for (int i = 0; i < 4; ++i) {
    if (in[byteKeys[i]].isNull()) continue;
    if (!readByte(in[byteKeys[i]], *byteDest[i])) {
      return sendError(400, String(byteKeys[i]) + " muss 0–255 sein");
    }
  }
  if (const char* err = config::fuseError(next.fuses)) return sendError(400, err);

  config::Settings& cfg = config::get();
  bool needsReboot = next.sdEnabled != cfg.sdEnabled || next.wifiOn != cfg.wifiOn ||
                     next.apSsid != cfg.apSsid || next.apPass != cfg.apPass;
  bool imageChanged = next.source != cfg.source || next.hexFile != cfg.hexFile;
  cfg = next;
  config::save();
  if (needsReboot) rebootPending = true;
  if (imageChanged) firmware::reload();

  JsonDocument out;
  out["ok"] = true;
  out["rebootPending"] = rebootPending;
  JsonArray warnings = out["warnings"].to<JsonArray>();
  if (cfg.writeFuses) {
    if (const char* w = config::fuseWarning(cfg.fuses)) warnings.add(w);
  }
  if (!firmware::valid()) warnings.add("Image ungültig: " + firmware::message());
  sendJson(200, out);
}

void handleFlash() {
  const flasher::Report& rep = flasher::run();
  JsonDocument doc;
  fillReport(doc.to<JsonObject>(), rep);
  sendJson(rep.error == flasher::Error::None ? 200 : 500, doc);
}

void handleReload() {
  firmware::reload();
  handleStatus();
}

void handleReboot() {
  sendOk();
  rebootAt = millis() + REBOOT_DELAY_MS;
}

void handleCounterReset() {
  config::get().counterOk = 0;
  config::get().counterFail = 0;
  config::saveCounters();
  sendOk();
}

void handleListFiles() {
  if (!requireSd()) return;
  JsonDocument doc;
  JsonArray files = doc["files"].to<JsonArray>();
  File root = SD.open("/");
  for (File f = root.openNextFile(); f; f = root.openNextFile()) {
    String name = f.name();
    String lower = name;
    lower.toLowerCase();
    if (!f.isDirectory() && lower.endsWith(".hex")) {
      JsonObject e = files.add<JsonObject>();
      e["name"] = name;
      e["size"] = f.size();
    }
    f.close();
  }
  root.close();
  doc["selected"] = config::get().hexFile;
  sendJson(200, doc);
}

void handleSelectFile() {
  if (!requireSd()) return;
  String path = firmware::sanitizeName(server.arg("name"));
  if (path.isEmpty() || !SD.exists(path)) return sendError(404, "Datei nicht gefunden");
  config::Settings& cfg = config::get();
  cfg.hexFile = path;
  cfg.source = config::Source::Sd;
  config::save();
  firmware::reload();
  handleStatus();
}

void handleDeleteFile() {
  if (!requireSd()) return;
  String path = firmware::sanitizeName(server.arg("name"));
  if (path.isEmpty() || !SD.exists(path)) return sendError(404, "Datei nicht gefunden");
  if (!SD.remove(path)) return sendError(500, "Löschen fehlgeschlagen");
  if (path == config::get().hexFile) firmware::reload();
  sendOk();
}

void handleUploadChunk() {
  HTTPUpload& up = server.upload();
  switch (up.status) {
    case UPLOAD_FILE_START:
      uploadError = "";
      uploadSize = 0;
      uploadName = firmware::sanitizeName(up.filename);
      if (!sdActive()) uploadError = "SD-Kartenleser nicht aktiv";
      else if (uploadName.isEmpty()) uploadError = "ungültiger Dateiname (*.hex)";
      else if (!(uploadFile = SD.open(UPLOAD_TMP, FILE_WRITE))) uploadError = "SD nicht beschreibbar";
      break;
    case UPLOAD_FILE_WRITE:
      if (!uploadError.isEmpty()) break;
      uploadSize += up.currentSize;
      if (uploadSize > MAX_UPLOAD) {
        uploadError = "Datei zu groß";
        uploadFile.close();
        SD.remove(UPLOAD_TMP);
      } else if (uploadFile.write(up.buf, up.currentSize) != up.currentSize) {
        uploadError = "Schreibfehler auf SD";
      }
      break;
    case UPLOAD_FILE_END:
      if (uploadFile) uploadFile.close();
      break;
    case UPLOAD_FILE_ABORTED:
      if (uploadFile) uploadFile.close();
      SD.remove(UPLOAD_TMP);
      uploadError = "Upload abgebrochen";
      break;
  }
}

void handleUploadDone() {
  if (!uploadError.isEmpty()) {
    if (sdActive()) SD.remove(UPLOAD_TMP);
    return sendError(400, uploadError);
  }
  String err;
  if (!firmware::validateFile(UPLOAD_TMP, err)) {
    SD.remove(UPLOAD_TMP);
    return sendError(400, "Ungültige HEX-Datei: " + err);
  }
  if (SD.exists(uploadName)) SD.remove(uploadName);
  if (!SD.rename(UPLOAD_TMP, uploadName)) return sendError(500, "Umbenennen fehlgeschlagen");
  if (uploadName == config::get().hexFile) firmware::reload();

  JsonDocument doc;
  doc["ok"] = true;
  doc["name"] = uploadName;
  sendJson(200, doc);
}

void handleLog() {
  if (!requireSd()) return;
  File f = SD.open("/log.csv", FILE_READ);
  if (!f) return sendError(404, "noch kein Log vorhanden");
  server.streamFile(f, "text/csv");
  f.close();
}

}  // namespace

void begin() {
  const config::Settings& cfg = config::get();
  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(cfg.apSsid.c_str(), cfg.apPass.isEmpty() ? nullptr : cfg.apPass.c_str());
  Serial.printf("WLAN-AP \"%s\" %s, http://%s\n", cfg.apSsid.c_str(),
                ok ? "gestartet" : "FEHLER", WiFi.softAPIP().toString().c_str());

  server.on("/", HTTP_GET, [] { server.send(200, "text/html; charset=utf-8", INDEX_HTML); });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  server.on("/api/flash", HTTP_POST, handleFlash);
  server.on("/api/reload", HTTP_POST, handleReload);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.on("/api/counter/reset", HTTP_POST, handleCounterReset);
  server.on("/api/files", HTTP_GET, handleListFiles);
  server.on("/api/files", HTTP_DELETE, handleDeleteFile);
  server.on("/api/select", HTTP_POST, handleSelectFile);
  server.on("/api/upload", HTTP_POST, handleUploadDone, handleUploadChunk);
  server.on("/api/log", HTTP_GET, handleLog);
  server.onNotFound([] { sendError(404, "nicht gefunden"); });
  server.begin();
}

void handle() {
  server.handleClient();
  if (rebootAt && int32_t(millis() - rebootAt) >= 0) ESP.restart();
}

}  // namespace webui
