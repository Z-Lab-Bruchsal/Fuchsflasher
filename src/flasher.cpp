#include "flasher.h"

#include "config.h"
#include "firmware_source.h"
#include "pins.h"

namespace flasher {

namespace {

constexpr uint32_t BLINK_ON_MS = 200;
constexpr uint32_t BLINK_OFF_MS = 250;
constexpr uint32_t BLINK_PAUSE_MS = 1200;

// Automatik: Abfrageintervall und nötige Treffer in Folge (entprellt Einstecken/Clip)
constexpr uint32_t POLL_MS = 300;
constexpr uint8_t PRESENT_POLLS = 3;
constexpr uint8_t ABSENT_POLLS = 3;
constexpr uint32_t HEARTBEAT_PERIOD_MS = 2000;
constexpr uint32_t HEARTBEAT_ON_MS = 60;

State currentState = State::Idle;
Report report;
bool reportValid = false;
uint32_t blinkStart = 0;

AutoState autoSt = AutoState::Off;
uint8_t pollCount = 0;
uint32_t lastPoll = 0;

void setLeds(bool ok, bool error, bool busy) {
  digitalWrite(PIN_LED_OK, ok);
  digitalWrite(PIN_LED_ERROR, error);
  digitalWrite(PIN_LED_BUSY, busy);
}

Error fromIsp(isp::Result r) {
  switch (r) {
    case isp::Result::Ok:           return Error::None;
    case isp::Result::NoResponse:   return Error::NoResponse;
    case isp::Result::BadSignature: return Error::BadSignature;
    case isp::Result::Timeout:      return Error::Timeout;
    case isp::Result::VerifyFail:   return Error::Verify;
    case isp::Result::FuseFail:     return Error::Fuses;
  }
  return Error::Timeout;
}

Error program(Report& rep) {
  const config::Settings& cfg = config::get();
  if (!firmware::valid()) return Error::NoImage;
  if (cfg.writeFuses && config::fuseError(cfg.fuses)) return Error::Fuses;

  isp::Result r = isp::enterProgMode();
  if (r != isp::Result::Ok) return fromIsp(r);

  rep.sig = isp::readSignature();
  if (!rep.sig.isAttiny85()) return Error::BadSignature;

  const Image& img = firmware::image();
  if ((r = isp::chipErase()) != isp::Result::Ok) return fromIsp(r);
  if ((r = isp::writeFlash(img.data, img.size)) != isp::Result::Ok) return fromIsp(r);
  if (cfg.verify && (r = isp::verifyFlash(img.data, img.size)) != isp::Result::Ok) {
    return fromIsp(r);
  }

  if (cfg.writeFuses && (r = isp::writeFuses(cfg.fuses)) != isp::Result::Ok) {
    return fromIsp(r);
  }
  rep.fuses = isp::readFuses();
  rep.fusesValid = true;

  // Lock-Bits zuletzt, danach ist kein Rücklesen des Flashs mehr möglich
  if (cfg.writeLock && (r = isp::writeLock(cfg.lock)) != isp::Result::Ok) {
    return fromIsp(r);
  }
  return Error::None;
}

String hexSig(const isp::Signature& s) {
  char buf[9];
  snprintf(buf, sizeof(buf), "%02X%02X%02X", s.b[0], s.b[1], s.b[2]);
  return buf;
}

// Steckt ein Chip im Sockel? Ohne Chip kann das Echo 0x53 nicht entstehen.
bool chipPresent() {
  isp::powerOn();
  bool present = isp::enterProgMode(1) == isp::Result::Ok;
  isp::leaveProgMode();
  isp::powerOff();
  return present;
}

void pollSocket() {
  if (millis() - lastPoll < POLL_MS) return;
  lastPoll = millis();
  bool present = chipPresent();

  if (autoSt == AutoState::WaitChip) {
    pollCount = present ? pollCount + 1 : 0;
    if (pollCount >= PRESENT_POLLS) {
      Serial.println("Automatik: neuer Chip erkannt");
      run();  // setzt WaitRemoval
    }
  } else {
    pollCount = present ? 0 : pollCount + 1;
    if (pollCount >= ABSENT_POLLS) {
      Serial.println("Automatik: Chip entnommen, warte auf nächsten");
      autoSt = AutoState::WaitChip;
      pollCount = 0;
      currentState = State::Idle;
      setLeds(false, false, false);
    }
  }
}

}  // namespace

void begin() {
  pinMode(PIN_LED_OK, OUTPUT);
  pinMode(PIN_LED_ERROR, OUTPUT);
  pinMode(PIN_LED_BUSY, OUTPUT);
  setLeds(false, false, false);
  isp::begin();
}

const Report& run() {
  currentState = State::Busy;
  setLeds(false, false, true);
  uint32_t start = millis();

  report = Report();
  isp::powerOn();
  report.error = program(report);
  isp::leaveProgMode();
  isp::powerOff();
  report.durationMs = millis() - start;

  config::Settings& cfg = config::get();
  bool ok = report.error == Error::None;
  if (ok) ++cfg.counterOk;
  else ++cfg.counterFail;
  config::saveCounters();

  firmware::appendLog(String(cfg.counterOk + cfg.counterFail) + "," + millis() / 1000 + "," +
                      errorName(report.error) + "," + hexSig(report.sig) + "," +
                      firmware::name());
  Serial.printf("Flashen: %s (%u ms), Signatur %s\n", errorName(report.error),
                unsigned(report.durationMs), hexSig(report.sig).c_str());

  currentState = ok ? State::Ok : State::Failed;
  reportValid = true;
  setLeds(ok, false, false);
  blinkStart = millis();

  // Auch nach manuellem Start denselben Chip nicht erneut automatisch flashen
  if (cfg.autoFlash) {
    autoSt = AutoState::WaitRemoval;
    pollCount = 0;
    lastPoll = millis();
  }
  return report;
}

void tick() {
  if (!config::get().autoFlash) {
    autoSt = AutoState::Off;
  } else {
    if (autoSt == AutoState::Off) {
      autoSt = AutoState::WaitChip;
      pollCount = 0;
    }
    pollSocket();
  }

  // Gelbes Lebenszeichen: bereit und wartet auf einen Chip
  bool heartbeat = autoSt == AutoState::WaitChip && currentState == State::Idle &&
                   millis() % HEARTBEAT_PERIOD_MS < HEARTBEAT_ON_MS;
  digitalWrite(PIN_LED_BUSY, heartbeat);

  if (currentState != State::Failed) return;
  // Fehlercode n: n Impulse, dann Pause, endlos bis zum nächsten Vorgang
  uint32_t pulses = uint32_t(report.error);
  uint32_t period = pulses * (BLINK_ON_MS + BLINK_OFF_MS) + BLINK_PAUSE_MS;
  uint32_t t = (millis() - blinkStart) % period;
  bool on = t < pulses * (BLINK_ON_MS + BLINK_OFF_MS) &&
            t % (BLINK_ON_MS + BLINK_OFF_MS) < BLINK_ON_MS;
  digitalWrite(PIN_LED_ERROR, on);
}

State state() {
  return currentState;
}

AutoState autoState() {
  return autoSt;
}

bool hasReport() {
  return reportValid;
}

const Report& last() {
  return report;
}

const char* errorName(Error e) {
  switch (e) {
    case Error::None:         return "OK";
    case Error::NoResponse:   return "keine Antwort vom Chip";
    case Error::BadSignature: return "falsche Signatur";
    case Error::Verify:       return "Verify fehlgeschlagen";
    case Error::NoImage:      return "kein gültiges Image";
    case Error::Fuses:        return "Fuses fehlgeschlagen";
    case Error::Timeout:      return "Timeout";
  }
  return "?";
}

}  // namespace flasher
