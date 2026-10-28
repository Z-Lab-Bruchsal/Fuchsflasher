#pragma once

#include <Arduino.h>

// ISP-Programmierung eines ATtiny85 per Bit-Bang-SPI.
namespace isp {

constexpr size_t FLASH_SIZE = 8192;
constexpr size_t PAGE_SIZE  = 64;  // Bytes (32 Wörter)

enum class Result {
  Ok,
  NoResponse,    // Programming Enable wurde nicht bestätigt
  BadSignature,  // kein ATtiny85
  Timeout,       // RDY/BSY-Polling abgelaufen
  VerifyFail,
  FuseFail,
};

struct Signature {
  uint8_t b[3];
  bool isAttiny85() const { return b[0] == 0x1E && b[1] == 0x93 && b[2] == 0x0B; }
};

struct Fuses {
  uint8_t low;
  uint8_t high;
  uint8_t ext;
};

void begin();
void powerOn();
void powerOff();

Result enterProgMode(int attempts = 5);
void leaveProgMode();

Signature readSignature();
Result chipErase();
Result writeFlash(const uint8_t* image, size_t len);
Result verifyFlash(const uint8_t* image, size_t len);

Fuses readFuses();
Result writeFuses(const Fuses& fuses);
uint8_t readLock();
Result writeLock(uint8_t lock);

const char* resultName(Result r);

}  // namespace isp
