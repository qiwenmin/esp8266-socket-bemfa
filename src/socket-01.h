#pragma once

#include "ESP8266Boot.h"
#include "bemfa.h"
#include "devices.h"

void register_socket_01_handler(ESP8266Boot& boot, BemfaTcp &bemfaTcp, const String& topicPrefix, Led *ledToggle);
