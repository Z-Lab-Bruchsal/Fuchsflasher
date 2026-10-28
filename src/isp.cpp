#include "isp.h"

#include "pins.h"

namespace isp {

namespace {

// Halbe SCK-Periode in µs → ~100 kHz. Muss > 2 Takte des Targets sein
// (Werkstakt 1 MHz → mind. 2 µs), daher reichlich Reserve.
constexpr uint32_t SCK_HALF_US = 5;
constexpr uint32_t BUSY_TIMEOUT_MS = 50;

uint8_t transferByte(uint8_t out) {
  uint8_t in = 0;
  for (int bit = 7; bit >= 0; --bit) {
    digitalWrite(PIN_ISP_MOSI, (out >> bit) & 1);
    delayMicroseconds(SCK_HALF_US);
    digitalWrite(PIN_ISP_SCK, HIGH);
    in = (in << 1) | digitalRead(PIN_ISP_MISO);
    delayMicroseconds(SCK_HALF_US);
    digitalWrite(PIN_ISP_SCK, LOW);
  }
  return in;
}

// Sendet einen 4-Byte-Befehl und liefert das letzte empfangene Byte.
uint8_t transfer4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  transferByte(a);
  transferByte(b);
  transferByte(c);
  return transferByte(d);
}

Result waitReady() {
  uint32_t start = millis();
  while (transfer4(0xF0, 0x00, 0x00, 0x00) & 0x01) {
    if (millis() - start > BUSY_TIMEOUT_MS) return Result::Timeout;
  }
  return Result::Ok;
}

void releasePins() {
  pinMode(PIN_ISP_SCK, INPUT);
  pinMode(PIN_ISP_MOSI, INPUT);
  pinMode(PIN_ISP_RESET, INPUT);  // externer Pull-up hält RESET high
}

}  // namespace

void begin() {
  pinMode(PIN_ISP_MISO, INPUT);
  releasePins();
  if (PIN_TARGET_PWR >= 0) {
    pinMode(PIN_TARGET_PWR, OUTPUT);
    digitalWrite(PIN_TARGET_PWR, LOW);
  }
}

void powerOn() {
  if (PIN_TARGET_PWR < 0) return;
  // RESET und SCK low halten, bevor VCC kommt (Datenblatt 20.6.1)
  pinMode(PIN_ISP_RESET, OUTPUT);
  digitalWrite(PIN_ISP_RESET, LOW);
  pinMode(PIN_ISP_SCK, OUTPUT);
  digitalWrite(PIN_ISP_SCK, LOW);
  digitalWrite(PIN_TARGET_PWR, HIGH);
  delay(50);
}

void powerOff() {
  releasePins();
  if (PIN_TARGET_PWR >= 0) digitalWrite(PIN_TARGET_PWR, LOW);
}

Result enterProgMode(int attempts) {
  pinMode(PIN_ISP_SCK, OUTPUT);
  digitalWrite(PIN_ISP_SCK, LOW);
  pinMode(PIN_ISP_MOSI, OUTPUT);
  pinMode(PIN_ISP_RESET, OUTPUT);

  for (int attempt = 0; attempt < attempts; ++attempt) {
    // Positiver RESET-Puls, falls SCK beim Einschalten nicht low war
    digitalWrite(PIN_ISP_RESET, HIGH);
    delayMicroseconds(100);
    digitalWrite(PIN_ISP_RESET, LOW);
    delay(25);

    transferByte(0xAC);
    transferByte(0x53);
    uint8_t echo = transferByte(0x00);
    transferByte(0x00);
    if (echo == 0x53) return Result::Ok;
  }
  return Result::NoResponse;
}

void leaveProgMode() {
  releasePins();
}

Signature readSignature() {
  Signature sig;
  for (uint8_t i = 0; i < 3; ++i) sig.b[i] = transfer4(0x30, 0x00, i, 0x00);
  return sig;
}

Result chipErase() {
  transfer4(0xAC, 0x80, 0x00, 0x00);
  return waitReady();
}

Result writeFlash(const uint8_t* image, size_t len) {
  if (len > FLASH_SIZE) len = FLASH_SIZE;
  for (size_t page = 0; page < len; page += PAGE_SIZE) {
    size_t pageEnd = min(page + PAGE_SIZE, len);

    bool empty = true;
    for (size_t i = page; i < pageEnd; ++i) {
      if (image[i] != 0xFF) { empty = false; break; }
    }
    if (empty) continue;  // nach Chip Erase ohnehin 0xFF

    for (size_t i = page; i < pageEnd; i += 2) {
      uint8_t word = (i / 2) & 0xFF;
      transfer4(0x40, 0x00, word, image[i]);
      transfer4(0x48, 0x00, word, i + 1 < pageEnd ? image[i + 1] : 0xFF);
    }
    uint16_t wordAddr = page / 2;
    transfer4(0x4C, wordAddr >> 8, wordAddr & 0xFF, 0x00);
    Result r = waitReady();
    if (r != Result::Ok) return r;
  }
  return Result::Ok;
}

Result verifyFlash(const uint8_t* image, size_t len) {
  if (len > FLASH_SIZE) len = FLASH_SIZE;
  for (size_t i = 0; i < len; ++i) {
    uint16_t wordAddr = i / 2;
    uint8_t cmd = (i & 1) ? 0x28 : 0x20;
    if (transfer4(cmd, wordAddr >> 8, wordAddr & 0xFF, 0x00) != image[i]) {
      return Result::VerifyFail;
    }
  }
  return Result::Ok;
}

Fuses readFuses() {
  Fuses f;
  f.low  = transfer4(0x50, 0x00, 0x00, 0x00);
  f.high = transfer4(0x58, 0x08, 0x00, 0x00);
  f.ext  = transfer4(0x50, 0x08, 0x00, 0x00);
  return f;
}

Result writeFuses(const Fuses& fuses) {
  const uint8_t cmds[3] = {0xA0, 0xA8, 0xA4};
  const uint8_t vals[3] = {fuses.low, fuses.high, fuses.ext};
  for (int i = 0; i < 3; ++i) {
    transfer4(0xAC, cmds[i], 0x00, vals[i]);
    Result r = waitReady();
    if (r != Result::Ok) return r;
  }
  Fuses check = readFuses();
  // Beim Extended Fuse ist nur Bit 0 (SELFPRGEN) belegt
  if (check.low != fuses.low || check.high != fuses.high ||
      (check.ext & 0x01) != (fuses.ext & 0x01)) {
    return Result::FuseFail;
  }
  return Result::Ok;
}

uint8_t readLock() {
  return transfer4(0x58, 0x00, 0x00, 0x00);
}

Result writeLock(uint8_t lock) {
  transfer4(0xAC, 0xE0, 0x00, lock);
  return waitReady();
}

const char* resultName(Result r) {
  switch (r) {
    case Result::Ok:           return "OK";
    case Result::NoResponse:   return "keine Antwort";
    case Result::BadSignature: return "falsche Signatur";
    case Result::Timeout:      return "Timeout";
    case Result::VerifyFail:   return "Verify fehlgeschlagen";
    case Result::FuseFail:     return "Fuses fehlgeschlagen";
  }
  return "?";
}

}  // namespace isp
