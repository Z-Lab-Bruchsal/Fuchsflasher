#pragma once

// WLAN-Access-Point + Weboberfläche/JSON-API (http://192.168.4.1).
namespace webui {

void begin();
// Aus loop() aufrufen.
void handle();

}  // namespace webui
