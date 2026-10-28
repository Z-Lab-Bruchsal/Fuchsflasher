# 🦊 fuchsflasher

**ATtiny85 in Serie programmieren, ohne PC.**
Chip einlegen, Knopf drücken, fertig. Ein ESP32 übernimmt das Programmieren und zeigt das Ergebnis per LED an.
Die Einstellungen machst du bequem im Browser über das eigene WLAN des fuchsflashers.

```
  Chip rein  →  Knopf drücken  →  🟡 läuft …  →  🟢 fertig  →  nächster Chip
```

Pro Chip dauert das etwa 1–3 Sekunden: Signatur prüfen → Chip löschen → Programm schreiben → prüfen (Verify) → optional Fuses und Lock-Bits.

Im **Automatik-Modus** brauchst du nicht einmal den Knopf: Der fuchsflasher erkennt selbst, wenn ein neuer Chip eingelegt wird.

---

## Inhalt

1. [Was du brauchst](#1-was-du-brauchst)
2. [Zusammenbauen](#2-zusammenbauen)
3. [fuchsflasher auf den ESP32 spielen](#3-fuchsflasher-auf-den-esp32-spielen)
4. [Erster Test](#4-erster-test)
5. [Eigenes Programm: vom `.ino` zum fertigen Chip](#5-eigenes-programm-vom-ino-zum-fertigen-chip)
6. [Automatik-Modus: Chip tauschen, fertig](#6-automatik-modus-chip-tauschen-fertig)
7. [Die Weboberfläche](#7-die-weboberfläche)
8. [LED-Anzeige und Fehlercodes](#8-led-anzeige-und-fehlercodes)
9. [Fehlersuche](#9-fehlersuche)
10. [Für Fortgeschrittene: HTTP-API](#10-für-fortgeschrittene-http-api)

---

## 1. Was du brauchst

| Teil | Hinweis |
|---|---|
| ESP32-Entwicklungsboard | z.B. ESP32-DevKitC / „ESP32 WROOM-32“ |
| Sockel für den ATtiny85 | ZIF-Sockel DIP-8 oder SOIC-8-Testclip |
| 1 Taster | Start-Knopf |
| 3 LEDs + 3× 330 Ω | grün, rot, gelb |
| 1× 10 kΩ | Pull-up für RESET |
| 1× 100 nF | Abblockkondensator am ATtiny |
| *optional:* P-MOSFET (z.B. AO3401) + NPN-Transistor | Damit schaltet der ESP32 den Chip strom­los, und du kannst gefahrlos wechseln |
| *optional:* SD-Kartenmodul | Dann kannst du Programme per Browser hochladen, ohne den ESP32 neu zu flashen |

> 💡 Der ATtiny85 läuft problemlos mit den 3,3 V des ESP32. Du brauchst keine Pegelwandler.

---

## 2. Zusammenbauen

### ATtiny85-Pinbelegung

```
            ┌───┬─┬───┐
   RESET  1 │   └─┘   │ 8  VCC
     PB3  2 │         │ 7  PB2 / SCK
     PB4  3 │ ATtiny85│ 6  PB1 / MISO
     GND  4 │         │ 5  PB0 / MOSI
            └─────────┘
```

### Verbindungen

| ESP32 | → | Ziel |
|---|---|---|
| GPIO 14 | → | ATtiny Pin 7 (SCK) |
| GPIO 13 | → | ATtiny Pin 5 (MOSI) |
| GPIO 27 | → | ATtiny Pin 6 (MISO) |
| GPIO 26 | → | ATtiny Pin 1 (RESET) + 10 kΩ nach 3,3 V |
| 3,3 V | → | ATtiny Pin 8 (VCC), 100 nF nach GND (bzw. über MOSFET, siehe unten) |
| GND | → | ATtiny Pin 4 (GND) |
| GPIO 32 | → | Taster → GND |
| GPIO 33 | → | 330 Ω → LED **grün** → GND |
| GPIO 22 | → | 330 Ω → LED **rot** → GND |
| GPIO 21 | → | 330 Ω → LED **gelb** → GND |

**Optional: schaltbare Versorgung**
GPIO 25 steuert über einen NPN-Transistor einen P-MOSFET, der die 3,3 V zum ATtiny durchschaltet (GPIO HIGH = Chip hat Strom).
Ohne diese Schaltung legst du VCC fest an und änderst in [`src/pins.h`](src/pins.h) die Zeile auf `PIN_TARGET_PWR = -1`.

**Optional: SD-Kartenmodul**

| ESP32 | SD-Modul |
|---|---|
| GPIO 18 | SCK |
| GPIO 19 | MISO |
| GPIO 23 | MOSI |
| GPIO 5 | CS |

> ⚠️ Viele SD-Module haben einen 5-V-Regler und funktionieren an 3,3 V nicht zuverlässig. Nimm ein Modul ohne Regler oder versorge es mit 5 V vom ESP32-Board.

---

## 3. fuchsflasher auf den ESP32 spielen

Du brauchst [PlatformIO](https://platformio.org/), am einfachsten als Erweiterung in VS Code.

1. Dieses Repository klonen bzw. herunterladen und den Ordner in VS Code öffnen.
2. ESP32 per USB anschließen.
3. In der PlatformIO-Leiste **Upload and Monitor** klicken, oder im Terminal:

   ```sh
   pio run -t upload -t monitor
   ```

Im seriellen Monitor (115200 Baud) erscheint dann:

```
fuchsflasher startet
Image eingebettet: target.hex: 20 Bytes
WLAN-AP "fuchsflasher" gestartet, http://192.168.4.1
bereit – Chip einlegen und Taster drücken
```

---

## 4. Erster Test

Mitgeliefert ist ein kleines **Blink-Testprogramm** ([`firmware/target.hex`](firmware/target.hex)): Eine LED an PB3 (Pin 2) blinkt etwa im Sekundentakt.

1. **Ohne Chip** den Taster drücken → die rote LED blinkt **1×** (keine Antwort). So soll es sein.
2. ATtiny85 einlegen und den Taster drücken → 🟡 kurz, dann 🟢.
   Im Monitor steht: `Flashen: OK … Signatur 1E930B`
3. Den Chip in eine Testschaltung stecken (LED + 330 Ω an Pin 2) → die LED blinkt.

Klappt das, ist die Verdrahtung korrekt. 🎉

---

## 5. Eigenes Programm: vom `.ino` zum fertigen Chip

```
  deinsketch.ino  ──(1) kompilieren──▶  deinsketch.ino.hex  ──(2) übertragen──▶  fuchsflasher  ──(3) Knopf──▶  ATtiny85
```

### Schritt 1: Sketch in eine `.hex`-Datei kompilieren

#### Variante A: Arduino IDE (empfohlen für Einsteiger)

1. **ATTinyCore installieren** (einmalig):
   - *Datei → Einstellungen → Zusätzliche Boardverwalter-URLs* diese Adresse eintragen:
     ```
     http://drazzy.com/package_drazzy.com_index.json
     ```
   - *Werkzeuge → Board → Boardverwalter* öffnen, nach **ATTinyCore** suchen und installieren.
2. **Board einstellen** unter *Werkzeuge*:
   - Board: **ATtiny25/45/85 (No bootloader)**
   - Chip: **ATtiny85**
   - Clock: z.B. **8 MHz (internal)**. Diesen Wert brauchst du gleich wieder für die Fuses!
3. **Exportieren:** *Sketch → Kompilierte Binärdatei exportieren*
4. Die Datei findest du im Sketch-Ordner unter `build/…/deinsketch.ino.hex`.
   Nimm die Datei **ohne** `with_bootloader` im Namen.

#### Variante B: Kommandozeile (arduino-cli)

```sh
arduino-cli core install ATTinyCore:avr \
  --additional-urls http://drazzy.com/package_drazzy.com_index.json

arduino-cli compile --fqbn ATTinyCore:avr:attinyx5:chip=85,clock=8internal \
  --output-dir build deinsketch/
```

Ergebnis: `build/deinsketch.ino.hex`

#### Variante C: PlatformIO

Ein eigenes Projekt mit `board = attiny85` anlegen und bauen. Ergebnis: `.pio/build/attiny85/firmware.hex`

### Schritt 2: `.hex`-Datei auf den fuchsflasher übertragen

#### Mit SD-Karte (bequem, kein Neu-Flashen des ESP32)

1. In der Weboberfläche unter *WLAN & Hardware* **„SD-Kartenleser angeschlossen“** anhaken, speichern und neu starten (einmalig).
2. Mit dem WLAN **`fuchsflasher`** verbinden (Passwort `fuchs1234`).
3. Im Browser **http://192.168.4.1** öffnen.
4. Unter *Firmware-Quelle* die `.hex`-Datei wählen → **Hochladen** → **Auswählen**.

Das geht auch per Skript:

```sh
curl -F "file=@deinsketch.ino.hex" http://192.168.4.1/api/upload
curl -X POST "http://192.168.4.1/api/select?name=deinsketch.ino.hex"
```

#### Ohne SD-Karte (fest eingebaut)

1. Die Datei als `firmware/target.hex` in dieses Projekt kopieren (die alte Datei ersetzen).
2. Den ESP32 neu flashen: `pio run -t upload`

Die `.hex`-Datei wird beim Build automatisch eingebettet.
Lieber einen anderen Dateinamen? Dann `custom_target_hex` in [`platformio.ini`](platformio.ini) anpassen.

### Schritt 3: Fuses passend zum Takt einstellen

⚠️ Die Clock-Einstellung beim Kompilieren gilt **nur für deinen Code**. Der Chip selbst läuft so, wie es seine **Fuses** vorgeben.
In der Arduino IDE erledigt das normalerweise „Bootloader brennen“. Beim fuchsflasher stellst du es in der Weboberfläche ein:

*Programmier-Optionen* → **„Fuses schreiben“** anhaken und die Werte eintragen:

| Clock beim Kompilieren | L | H | E |
|---|---|---|---|
| 1 MHz (internal), Werkszustand | `62` | `DF` | `FF` |
| **8 MHz (internal)** | **`E2`** | `DF` | `FF` |
| 16 MHz (PLL, internal) | `F1` | `DF` | `FF` |

> Passt der Takt nicht zu den Fuses, läuft alles 8-mal zu schnell oder zu langsam (`delay()`, `millis()`, Serial, Töne …).

> 🔒 Werte, nach denen der Chip nicht mehr programmierbar wäre (RSTDISBL, SPIEN aus), lehnt der fuchsflasher ab.
> Bei externem Quarz oder dem 128-kHz-Oszillator erscheint eine Warnung, denn ohne Quarz bzw. mit so langsamem Takt kann der fuchsflasher den Chip danach nicht mehr erreichen.

### Schritt 4: Serie flashen

Chip einlegen → Knopf → 🟢 → Chip raus → nächster Chip.
Die Zähler für erfolgreiche und fehlgeschlagene Vorgänge siehst du in der Weboberfläche.
Noch bequemer geht es mit dem [Automatik-Modus](#6-automatik-modus-chip-tauschen-fertig).

---

## 6. Automatik-Modus: Chip tauschen, fertig

Im Automatik-Modus erkennt der fuchsflasher selbst, wenn ein neuer Chip eingelegt wird, und flasht ihn sofort.

**Einschalten:** Weboberfläche → *Programmier-Optionen* → **„Automatik-Modus“** anhaken → speichern. Ein Neustart ist nicht nötig.

```
  Sockel leer  ──Chip rein──▶  erkannt (≈1 s)  ──▶  🟡 flashen  ──▶  🟢/🔴  ──Chip raus──▶  Sockel leer …
  🟡 blitzt kurz                                                    wartet auf Entnahme
```

So funktioniert es:

1. **Warten auf Chip:** Die gelbe LED blitzt alle 2 Sekunden kurz auf. Der fuchsflasher fragt den Sockel etwa 3× pro Sekunde ab.
2. **Chip erkannt:** Antwortet ein Chip **3× hintereinander**, wird geflasht. Ein halb eingesteckter Chip oder ein schief sitzender Clip löst also nicht vorschnell aus.
3. **Ergebnis:** 🟢 oder 🔴 bleibt an, solange der Chip im Sockel steckt. Er wird **nicht** ein zweites Mal geflasht.
4. **Chip entnommen:** Antwortet **3× hintereinander** kein Chip mehr, gehen die LEDs aus und der nächste Chip kann rein.

Gut zu wissen:

- Der **Taster funktioniert weiterhin**, z.B. um denselben Chip nach einem Fehler noch einmal zu flashen.
- Einen Chip, der schon beim Einschalten des fuchsflashers im Sockel steckt, flasht die Automatik sofort.
- Wird ein Chip in weniger als ca. 1 Sekunde gezogen und wieder eingesteckt, gilt er nicht als neu. Dann einfach den Taster drücken.
- Am zuverlässigsten ist die Automatik mit der **schaltbaren Versorgung** (MOSFET an GPIO 25): Der Chip ist zwischen den Abfragen stromlos und kann gefahrlos gewechselt werden.
- Ein Chip mit falscher Signatur (z.B. ATtiny45) wird erkannt, mit Fehler 2 abgelehnt und erst nach dem Entnehmen wieder beachtet.

---

## 7. Die Weboberfläche

WLAN **`fuchsflasher`**, Passwort **`fuchs1234`**, Adresse **http://192.168.4.1**

| Bereich | Was du dort machst |
|---|---|
| **Status** | Ergebnis des letzten Vorgangs, gelesene Signatur und Fuses, Zähler, Knopf **„Jetzt flashen“** |
| **Firmware-Quelle** | eingebettet oder SD-Karte; Dateien hochladen, auswählen, löschen; Flash-Log herunterladen |
| **Programmier-Optionen** | Automatik-Modus, Verify an/aus, Fuses, Lock-Bits (`FC` = Auslesen gesperrt) |
| **WLAN & Hardware** | SD-Kartenleser vorhanden?, WLAN an/aus, Name und Passwort |

Alle Einstellungen bleiben auch nach dem Ausschalten erhalten.
Änderungen an SD-Kartenleser und WLAN werden erst **nach einem Neustart** wirksam. Die Seite bietet dafür einen Knopf an.

> 🆘 **WLAN ausgeschaltet oder Passwort vergessen?** Halte den Taster beim Einschalten gedrückt. Dann startet das WLAN trotzdem.

Mit SD-Karte wird jeder Vorgang in `/log.csv` protokolliert (Nummer, Laufzeit, Ergebnis, Signatur, Datei).

---

## 8. LED-Anzeige und Fehlercodes

| LED | Bedeutung |
|---|---|
| 🟡 gelb, blitzt alle 2 s | Automatik-Modus: wartet auf einen Chip |
| 🟡 gelb | Programmierung läuft |
| 🟢 grün | Erfolgreich, nächster Chip kann rein |
| 🔴 rot blinkt *n*-mal, Pause, wiederholt | Fehler, siehe Tabelle |

| Blinken | Fehler | Typische Ursache |
|---|---|---|
| 1× | Keine Antwort vom Chip | Kein Chip, falsch herum eingesteckt, Verdrahtung, Kontakt am Clip |
| 2× | Falsche Signatur | Kein ATtiny85 (z.B. ATtiny45) oder wackeliger Kontakt |
| 3× | Verify fehlgeschlagen | Kontaktproblem während des Schreibens, Chip defekt |
| 4× | Kein gültiges Programm | Keine/ungültige HEX-Datei, SD-Karte fehlt |
| 5× | Fuses fehlgeschlagen | Fuses ließen sich nicht schreiben oder bestätigen |
| 6× | Timeout | Chip reagiert während des Schreibens nicht mehr |

Die Fehlermeldung steht auch im Klartext in der Weboberfläche und im seriellen Monitor.

---

## 9. Fehlersuche

| Problem | Lösung |
|---|---|
| Immer Fehler 1 | MISO/MOSI vertauscht? Pin 1 des Chips an der richtigen Stelle? 10 kΩ an RESET vorhanden? Bekommt der Chip Strom (bei MOSFET-Schaltung: `PIN_TARGET_PWR` richtig)? |
| Chip wurde früher auf 128 kHz oder externen Takt gestellt | Der fuchsflasher erreicht ihn nicht mehr. Abhilfe: Quarz/Takt anschließen oder einen High-Voltage-Programmer verwenden |
| SD-Karte wird nicht erkannt | Als FAT32 formatieren, Modul an 3,3 V prüfen (siehe Hinweis oben), Neustart |
| Programm läuft zu langsam/schnell | Fuses passen nicht zur Clock-Einstellung, siehe [Schritt 3](#schritt-3-fuses-passend-zum-takt-einstellen) |
| Weboberfläche nicht erreichbar | Handy bleibt manchmal im „Kein Internet“-WLAN nicht verbunden. Mobile Daten kurz ausschalten |
| Automatik flasht nicht | Gelbe LED blitzt nicht? Dann ist der Modus aus. Blitzt sie, aber nichts passiert: Kontakt prüfen (der Chip muss 3× hintereinander antworten) |
| `pio run` meldet „Python version must be between 3.10 and 3.13“ | Die ESP32-Plattform unterstützt Python 3.14 noch nicht. PlatformIO mit Python 3.13 betreiben (z.B. per `uv`/`pyenv`) |

---

## 10. Für Fortgeschrittene: HTTP-API

Alle Funktionen der Weboberfläche gibt es auch als JSON-API, praktisch für Skripte:

| Methode | Pfad | Zweck |
|---|---|---|
| `GET` | `/api/status` | Zustand, Automatik (`off`/`waitChip`/`waitRemoval`), letztes Ergebnis, Zähler, aktives Programm |
| `GET` / `POST` | `/api/config` | Einstellungen lesen / setzen (JSON) |
| `POST` | `/api/flash` | Flashvorgang starten |
| `POST` | `/api/reload` | Programm neu laden |
| `POST` | `/api/reboot` | Neustart |
| `POST` | `/api/counter/reset` | Zähler auf 0 setzen |
| `GET` | `/api/files` | HEX-Dateien auf der SD-Karte auflisten |
| `DELETE` | `/api/files?name=…` | Datei löschen |
| `POST` | `/api/select?name=…` | Datei als Quelle auswählen |
| `POST` | `/api/upload` | HEX-Datei hochladen (multipart, Feld `file`) |
| `GET` | `/api/log` | Flash-Log als CSV |

Die Datei-Routen antworten mit `404`, wenn kein SD-Kartenleser aktiv ist.

Beispiel: Automatik einschalten

```sh
curl -X POST http://192.168.4.1/api/config \
  -H "Content-Type: application/json" -d '{"autoFlash": true}'
```

Beispiel: Fuses auf 8 MHz setzen und flashen

```sh
curl -X POST http://192.168.4.1/api/config \
  -H "Content-Type: application/json" \
  -d '{"writeFuses": true, "lfuse": 226, "hfuse": 223, "efuse": 255}'
curl -X POST http://192.168.4.1/api/flash
```

---

## Projektaufbau

```
fuchsflasher/
├── firmware/target.hex     ← fest eingebautes ATtiny-Programm
├── src/
│   ├── pins.h              ← Pinbelegung anpassen
│   ├── isp.cpp             ← ISP-Protokoll ATtiny85
│   ├── hexparser.cpp       ← Intel-HEX einlesen
│   ├── flasher.cpp         ← Ablauf + LEDs
│   ├── firmware_source.cpp ← Programm aus Firmware oder SD
│   ├── config.cpp          ← Einstellungen (dauerhaft gespeichert)
│   ├── webui.cpp           ← WLAN + Webserver + API
│   └── webui_page.h        ← die Weboberfläche
├── tools/hex2header.py     ← bettet target.hex beim Build ein
└── concept.md              ← technisches Konzept
```
