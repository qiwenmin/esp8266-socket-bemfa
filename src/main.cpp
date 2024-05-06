#include <Arduino.h>

#include "ESP8266Boot.h"
#include "bemfa.h"

#include "hw.h"
#include "bemfa.inc"

#include "socket-01.h"

ESP8266Boot boot;

BemfaTcp bemfaTcp(bemfa_tcp_server, bemfa_tcp_port, bemfa_tcp_client_id);

static String hostname;

void setup() {
    auto chipIdHex = String(ESP.getChipId(), HEX);
    hostname = "so01x" + chipIdHex;

    // Init bemfaMqtt
    register_socket_01_handler(boot, bemfaTcp, hostname, boot.getLed());

    bemfaTcp.begin();

    // Init boot
    boot.setLed(LED_PIN, LOW);
    boot.setButton(BTN_PIN);
    boot.setHostname(hostname);
    boot.setOTAPassword(String(ESP.getChipId(), DEC));
    boot.begin();
}

void loop() {
    boot.loop();
    bemfaTcp.loop();

    delay(100); // for power saving
}
