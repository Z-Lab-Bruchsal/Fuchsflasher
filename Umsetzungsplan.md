# Umsetzungsplan „fuchsflasher“ – ATtiny85-Batch-Programmer mit ESP32

## Context
Ziel: ATtiny85 in Serie flashen. Chip in Sockel/Klemme stecken → Knopf drücken → ESP32 flasht (Erase, Write, Verify, Fuses) → LED zeigt OK/Fehler → nächster Chip. Kein PC nötig.
Firmware-Quelle wahlweise **fest einkompiliert** oder **HEX-Datei von SD-Karte**. Alle Einstellungen (z.B. „SD-Kartenleser vorhanden“, Fuses, Verify an/aus, Firmware-Quelle) werden über eine **Weboberfläche im eigenen WLAN (Access Point des ESP32)** gesetzt und im NVS gespeichert. Ist SD aktiviert, bietet die Weboberfläche zusätzlich Upload/Auswahl/Löschen von HEX-Dateien.

Das Verzeichnis `/home/alegab/workspace/fuchsflasher` ist leer → Neuaufbau. **Schritt 0 der Umsetzung:** diesen Plan als `UMSETZUNGSPLAN.md` ins Projektverzeichnis kopieren.

Machbarkeit: Ja. ISP-Protokoll des ATtiny85 ist simpel (4-Byte-SPI-Befehle), 8 KB Flash passen locker ins RAM des ESP32. ATtiny85 lässt sich mit 3,3 V versorgen und programmieren → keine Pegelwandler nötig.

---

## 1. Hardware

| Funktion | ESP32-GPIO | ATtiny85-Pin / Bauteil |
|---|---|---|
| ISP SCK (Bit-Bang) | 14 | Pin 7 (PB2) |
| ISP MOSI | 13 | Pin 5 (PB0) |
| ISP MISO | 27 | Pin 6 (PB1) |
| ISP RESET | 26 | Pin 1 (RESET), 10 kΩ Pull-up nach 3,3 V |
| Target-Power (optional) | 25 | P-MOSFET (z.B. AO3401 + NPN-Treiber) → Pin 8 VCC |
| GND | GND | Pin 4 |
| SD-Karte (VSPI) | SCK 18, MISO 19, MOSI 23, CS 5 | SD-Modul **ohne** 5V-Regler/Levelshifter (oder VCC mit 5 V speisen) |
| Start-Taster | 32 (INPUT_PULLUP) | Taster gegen GND |
| LED grün (OK) | 33 | + 330 Ω |
| LED rot (Fehler) | 22 | + 330 Ω |
| LED gelb (busy) | 21 | + 330 Ω |

Hinweise:
- Strapping-Pins (0, 2, 12, 15) bewusst vermieden; GPIO5 als SD-CS ist Standard.
- Target-Aufnahme: ZIF-DIP8-Sockel oder SOIC-8-Testclip. 100 nF am Target-VCC.
- Optional 220 Ω in Serie auf SCK/MOSI/MISO (Schutz bei Fehlsteckung).
- Mit Target-Power-Schaltung kann man Chips stromlos wechseln (empfohlen für Serie).

## 2. Software-Stack
- **PlatformIO**, Framework Arduino, Board `esp32dev`.
- Libraries: Arduino-Core `WebServer`, `WiFi`, `Preferences`, `SD`, `SPI`; `ArduinoJson` (v7) für die API.
- ISP per **Bit-Bang** (~100 kHz, sicher unter f_cpu/4 auch bei Werks-Takt 1 MHz) → SD bleibt allein auf Hardware-VSPI, keine Bus-Konflikte.

## 3. Projektstruktur
```
fuchsflasher/
├── UMSETZUNGSPLAN.md
├── platformio.ini
├── firmware/                  # HEX-Dateien für die Einbettung
│   └── target.hex
├── tools/
│   └── hex2header.py          # HEX → src/firmware_embedded.h
├── data/                      # (optional) Web-Assets, sonst inline
└── src/
    ├── main.cpp               # Setup, Taster, LED, Zustandsmaschine
    ├── pins.h                 # Pin-Definitionen (Tabelle oben)
    ├── isp.h / isp.cpp        # ATtiny85-ISP-Protokoll
    ├── hexparser.h / .cpp     # Intel-HEX → 8-KB-Puffer
    ├── firmware_source.h/.cpp # liefert Image aus Embedded oder SD
    ├── config.h / config.cpp  # Einstellungen in NVS (Preferences)
    ├── webui.h / webui.cpp    # AP, HTTP-Routen, JSON-API
    ├── webui_page.h           # HTML/JS/CSS als PROGMEM-String
    └── firmware_embedded.h    # generiert von hex2header.py
```

## 4. Module im Detail

### 4.1 `isp.cpp` – ATtiny85-Programmierung
- `begin()`, `powerOn()/powerOff()`, `transfer4(a,b,c,d)` (Bit-Bang SPI Mode 0).
- `enterProgMode()`: RESET low, 20 ms warten, `AC 53 00 00` senden, 3. Byte muss `0x53` sein; sonst RESET-Puls + bis zu 5 Versuche.
- `readSignature()`: `30 00 0n 00` für n=0..2 → erwartet `1E 93 0B`.
- `chipErase()`: `AC 80 00 00`, dann Busy-Poll `F0 00 00 00` (Bit0=0) bzw. 10 ms.
- `writeFlash(buf, len)`: Page-Größe **64 Byte (32 Wörter)**, 128 Pages. Pro Wort `40 00 adr low` / `48 00 adr high`, dann `4C adrMSB adrLSB 00` + Busy-Poll. Leere Pages (nur 0xFF) überspringen.
- `verifyFlash(buf, len)`: `20/28 adrMSB adrLSB 00` lesen und vergleichen.
- Fuses: schreiben `AC A0 00 lf`, `AC A8 00 hf`, `AC A4 00 ef`; lesen `50 00 00 00`, `58 08 00 00`, `50 08 00 00`; danach Rücklesen und vergleichen.
- Lock-Bits (optional): `AC E0 00 lock`.
- `leaveProgMode()`: RESET high, Pins hochohmig.
- Rückgabe als `enum IspResult { OK, NO_RESPONSE, BAD_SIGNATURE, VERIFY_FAIL, FUSE_FAIL, ... }`.

### 4.2 `hexparser.cpp`
- Parst Intel-HEX aus `Stream&` (SD-Datei) oder `const char*` (Embedded) in `uint8_t image[8192]` (vorbefüllt 0xFF).
- Prüft Prüfsummen, Record-Typen 00/01 (02/04 ignorieren bzw. Adresse > 8 KB → Fehler), merkt höchste Adresse.
- Wird beim Start und bei Änderung von Quelle/Datei aufgerufen → Image liegt fertig im RAM, Flashvorgang ist schnell (~1–2 s).

### 4.3 `tools/hex2header.py` + `firmware_embedded.h`
- Wandelt `firmware/target.hex` in `const char EMBEDDED_HEX[] PROGMEM = "..."` (HEX-Text, wird mit demselben Parser geparst → ein Codepfad).
- Einbindung als PlatformIO-`extra_scripts` (pre-build), damit der Header bei jedem Build automatisch neu erzeugt wird.

### 4.4 `config.cpp` – Einstellungen (NVS via `Preferences`)
| Key | Typ | Default |
|---|---|---|
| `sdEnabled` | bool | false |
| `source` | enum embedded/sd | embedded |
| `hexFile` | string | `/target.hex` |
| `apSsid` / `apPass` | string | `fuchsflasher` / `fuchs1234` |
| `wifiOn` | bool | true |
| `writeFuses`, `lfuse`, `hfuse`, `efuse` | bool/u8 | false, 0x62, 0xDF, 0xFF (Werkswerte) |
| `verify` | bool | true |
| `writeLock`, `lock` | bool/u8 | false, 0xFF |
| `counterOk`, `counterFail` | u32 | 0 |

- Ist `source=sd` aber `sdEnabled=false` oder SD nicht lesbar → Fehler-LED + Hinweis im Web, kein Flashen.
- Notfall-Zugang: Taster beim Booten gedrückt halten → WLAN-AP wird unabhängig von `wifiOn` gestartet.

### 4.5 `webui.cpp` – Weboberfläche (Access Point, 192.168.4.1)
Routen:
- `GET /` → Single-Page-UI (Vanilla JS, inline aus `webui_page.h`).
- `GET /api/status` → Zustand, letztes Ergebnis, Signatur, gelesene Fuses, Zähler, Image-Größe, SD-Status.
- `GET /api/config`, `POST /api/config` → Einstellungen lesen/speichern (JSON). Änderung von `sdEnabled`/WLAN → Hinweis „Neustart nötig“.
- `POST /api/flash` → Flashvorgang wie Tastendruck auslösen.
- `POST /api/reboot`, `POST /api/counter/reset`.
- **Nur wenn `sdEnabled`:** `GET /api/files` (Liste *.hex), `POST /api/upload` (Multipart → SD, danach Parse-Check), `POST /api/select?name=`, `DELETE /api/files?name=`.
  Ist SD deaktiviert, liefern diese Routen 404 und die UI blendet den Dateibereich aus.

UI-Bereiche: Status/Zähler · Firmware-Quelle & Dateiverwaltung · Fuses (Hex-Felder + Checkbox) · Optionen (Verify, Lock) · WLAN/Hardware (SD vorhanden?) · Buttons „Jetzt flashen“, „Neustart“.

**Sicherheitsabfrage Fuses:** UI und Firmware blockieren hfuse mit gelöschtem `RSTDISBL` (Bit 7 = 0) oder `SPIEN` (Bit 5 = 1), da der Chip danach per ISP nicht mehr erreichbar ist. Warnung bei externem Takt (CKSEL ≠ 0010/0001), da ohne Quarz/Takt kein weiteres ISP möglich.

### 4.6 `main.cpp` – Ablauf
Zustandsmaschine: `IDLE → BUSY → OK | ERROR → IDLE`
1. Taster (entprellt, 50 ms) oder `/api/flash`.
2. Image vorhanden? Sonst Fehler 4.
3. `powerOn()`, 50 ms → `enterProgMode()` (Fehler 1) → `readSignature()` (Fehler 2).
4. `chipErase()` → `writeFlash()` → wenn aktiv `verifyFlash()` (Fehler 3).
5. wenn aktiv Fuses schreiben + rücklesen (Fehler 5), dann ggf. Lock-Bits.
6. `leaveProgMode()`, `powerOff()`, Zähler + optional Logzeile `/log.csv` auf SD.
7. LED: grün dauerhaft bis zum nächsten Tastendruck bzw. rot blinkend mit Fehlercode (1 = keine Antwort, 2 = falsche Signatur, 3 = Verify, 4 = kein/ungültiges Image, 5 = Fuses).
- Webserver läuft in `loop()` mit `server.handleClient()`; der Flashvorgang blockiert kurz (~2 s), das ist akzeptabel.

## 5. Umsetzungsreihenfolge
0. Plan als `UMSETZUNGSPLAN.md` ins Projekt kopieren, `platformio.ini` + Grundgerüst anlegen.
1. `pins.h`, `isp.cpp`: nur Signatur lesen, über Serial ausgeben → Verdrahtung prüfen.
2. `hexparser.cpp` + `hex2header.py` + Embedded-Firmware; komplettes Flashen per Taster mit LEDs.
3. `config.cpp` (NVS) inkl. Fuses/Verify-Optionen.
4. `webui.cpp` mit Status, Config, „Jetzt flashen“.
5. SD-Unterstützung: Init abhängig von `sdEnabled`, Datei-Upload/Auswahl/Löschen, Log.
6. Feinschliff: Fuse-Sicherheitsprüfung, Fehlercodes, Notfall-AP per Taster beim Boot.

## 6. Verifikation
- **Verdrahtung:** Serial-Log zeigt Signatur `1E 93 0B`; ohne Chip → Fehlercode 1.
- **Flashen:** Blink-Sketch (LED an PB3/PB4) als `target.hex` einbetten, flashen, Chip in Testschaltung → blinkt. Zusätzlich Gegencheck mit `avrdude -c usbasp -p t85 -U flash:v:target.hex` (falls zweiter Programmer vorhanden) oder Verify-Funktion bewusst mit verändertem Image testen → Fehler 3.
- **Fuses:** z.B. `lfuse 0xE2` (8 MHz, CKDIV8 aus) setzen → Blink-Frequenz ändert sich ×8; Web-Status zeigt rückgelesene Fuses. Gesperrte Werte (RSTDISBL) werden von UI/API abgewiesen.
- **Web/Config:** mit Handy ins WLAN `fuchsflasher`, 192.168.4.1 öffnen, Einstellungen ändern, Neustart → Werte bleiben erhalten.
- **SD:** SD aktivieren, Neustart, HEX hochladen, auswählen, flashen; kaputte HEX-Datei hochladen → wird mit Fehlermeldung abgelehnt. SD deaktivieren → Dateibereich verschwindet, `/api/files` liefert 404.
- **Serie:** 10 Chips nacheinander flashen, Zähler im Web prüfen.
