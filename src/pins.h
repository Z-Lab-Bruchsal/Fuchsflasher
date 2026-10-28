#pragma once

// ISP zum ATtiny85 (Bit-Bang)
constexpr int PIN_ISP_SCK   = 14;  // ATtiny85 Pin 7 (PB2)
constexpr int PIN_ISP_MOSI  = 13;  // ATtiny85 Pin 5 (PB0)
constexpr int PIN_ISP_MISO  = 27;  // ATtiny85 Pin 6 (PB1)
constexpr int PIN_ISP_RESET = 26;  // ATtiny85 Pin 1 (RESET), 10k Pull-up nach 3,3 V

// Schaltbare Target-Versorgung (HIGH = an). -1, wenn VCC fest verdrahtet ist.
constexpr int PIN_TARGET_PWR = 25;

// SD-Karte (Hardware-VSPI)
constexpr int PIN_SD_SCK  = 18;
constexpr int PIN_SD_MISO = 19;
constexpr int PIN_SD_MOSI = 23;
constexpr int PIN_SD_CS   = 5;

// Bedienung
constexpr int PIN_BUTTON    = 32;  // Taster gegen GND
constexpr int PIN_LED_OK    = 33;  // grün
constexpr int PIN_LED_ERROR = 22;  // rot
constexpr int PIN_LED_BUSY  = 21;  // gelb
