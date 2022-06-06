#include <Arduino.h>

#include "ESP8266Boot.h"
#include "bemfa.h"

#include "hw.h"
#include "bemfa.inc"

#include "socket-01.h"

ESP8266Boot boot;

BemfaMqtt bemfaMqtt(bemfa_mqtt_server, bemfa_mqtt_port, bemfa_mqtt_client_id);

static String hostname;

void setup() {
    auto chipIdHex = String(ESP.getChipId(), HEX);
    hostname = "so01x" + chipIdHex;

    // Init bemfaMqtt
    register_socket_01_handler(boot, bemfaMqtt, hostname, boot.getLed());

    bemfaMqtt.begin();

    // Init boot
    boot.setLed(LED_PIN, LOW);
    boot.setButton(BTN_PIN);
    boot.setHostname(hostname);
    boot.setOTAPassword(String(ESP.getChipId(), DEC));
    boot.begin();
}

void loop() {
    boot.loop();
    bemfaMqtt.loop();

    delay(100); // for power saving
}
