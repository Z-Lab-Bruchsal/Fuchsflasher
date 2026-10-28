#include "hexparser.h"

namespace {

constexpr size_t MAX_LINE = 600;  // längster Record: 1 + 2*(5 + 255) Zeichen

int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

class HexParser {
 public:
  explicit HexParser(Image& img) : img_(img) {
    memset(img_.data, 0xFF, sizeof(img_.data));
    img_.size = 0;
  }

  // Verarbeitet eine Zeile (ohne Zeilenende). false bei Fehler.
  bool feedLine(const char* line) {
    ++lineNo_;
    if (*line == '\0') return true;
    if (done_) return true;  // alles nach dem EOF-Record ignorieren
    if (*line != ':') return fail("Zeile beginnt nicht mit ':'");

    uint8_t rec[260];
    size_t len = 0;
    for (const char* p = line + 1; *p; p += 2) {
      int hi = hexNibble(p[0]);
      int lo = p[1] ? hexNibble(p[1]) : -1;
      if (hi < 0 || lo < 0) return fail("ungültiges Hex-Zeichen");
      if (len >= sizeof(rec)) return fail("Record zu lang");
      rec[len++] = (hi << 4) | lo;
    }
    if (len < 5 || len != size_t(rec[0]) + 5) return fail("falsche Record-Länge");

    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i) sum += rec[i];
    if (sum != 0) return fail("Prüfsumme falsch");

    uint8_t count = rec[0];
    uint32_t addr = (uint32_t(rec[1]) << 8) | rec[2];
    const uint8_t* data = rec + 4;

    switch (rec[3]) {
      case 0x00:  // Daten
        addr += base_;
        if (addr + count > isp::FLASH_SIZE) return fail("Daten über 8 KB Flash hinaus");
        memcpy(img_.data + addr, data, count);
        img_.size = max(img_.size, size_t(addr + count));
        return true;
      case 0x01:  // EOF
        done_ = true;
        return true;
      case 0x02:  // Extended Segment Address
        if (count != 2) return fail("ungültiger Typ-02-Record");
        base_ = ((uint32_t(data[0]) << 8) | data[1]) << 4;
        return true;
      case 0x04:  // Extended Linear Address
        if (count != 2) return fail("ungültiger Typ-04-Record");
        base_ = ((uint32_t(data[0]) << 8) | data[1]) << 16;
        return true;
      case 0x03:
      case 0x05:  // Startadresse – für AVR ohne Bedeutung
        return true;
      default:
        return fail("unbekannter Record-Typ");
    }
  }

  bool finish(String& err) {
    if (!error_.isEmpty()) {
      err = error_;
      return false;
    }
    if (!done_) {
      err = "EOF-Record fehlt";
      return false;
    }
    if (img_.size == 0) {
      err = "keine Daten";
      return false;
    }
    img_.size = (img_.size + 1) & ~size_t(1);
    return true;
  }

 private:
  bool fail(const char* msg) {
    error_ = String("Zeile ") + lineNo_ + ": " + msg;
    return false;
  }

  Image& img_;
  uint32_t base_ = 0;
  unsigned lineNo_ = 0;
  bool done_ = false;
  String error_;
};

}  // namespace

bool parseHexStream(Stream& in, Image& img, String& err) {
  HexParser parser(img);
  char line[MAX_LINE + 1];
  size_t len = 0;
  bool overflow = false;
  bool ok = true;

  auto flush = [&]() {
    line[len] = '\0';
    if (overflow) {
      err = "Zeile zu lang";
      ok = false;
    } else if (!parser.feedLine(line)) {
      ok = false;
    }
    len = 0;
    overflow = false;
  };

  while (ok && in.available()) {
    int c = in.read();
    if (c < 0) break;
    if (c == '\n') {
      flush();
    } else if (c != '\r') {
      if (len < MAX_LINE) line[len++] = c;
      else overflow = true;
    }
  }
  if (ok && (len > 0 || overflow)) flush();
  if (!err.isEmpty()) return false;
  return parser.finish(err);
}

bool parseHexString(const char* text, Image& img, String& err) {
  HexParser parser(img);
  char line[MAX_LINE + 1];
  while (*text) {
    const char* end = strchr(text, '\n');
    size_t len = end ? end - text : strlen(text);
    if (len > 0 && text[len - 1] == '\r') --len;
    if (len > MAX_LINE) {
      err = "Zeile zu lang";
      return false;
    }
    memcpy(line, text, len);
    line[len] = '\0';
    if (!parser.feedLine(line)) break;
    text = end ? end + 1 : text + strlen(text);
  }
  return parser.finish(err);
}
