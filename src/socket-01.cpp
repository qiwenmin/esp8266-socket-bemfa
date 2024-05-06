#include <EEPROM.h>

#include "socket-01.h"

#define LED_BLUE_PIN (15)
#define RELAY_PIN (4)

static Led *led = 0;
static String topic;
static String topic_set;

typedef struct {
    uint32_t magic_number;
    bool relay_is_on;
} saved_state_struct;

const uint32_t so_magic_number = ('s' << 24) | ('o' << 16) | ('0' << 8) | '1';

static saved_state_struct saved_state;

static void save_state() {
    EEPROM.begin(sizeof(saved_state));
    uint8_t *p = (uint8_t *)(&saved_state);
    for (unsigned int addr = 0; addr < sizeof(saved_state); addr++) {
        EEPROM.write(addr, *(p + addr));
    }
    EEPROM.commit();
}

static void load_state() {
    EEPROM.begin(sizeof(saved_state));
    uint8_t *p = (uint8_t *)(&saved_state);
    for (unsigned int addr = 0; addr < sizeof(saved_state); addr++) {
        *(p + addr) = EEPROM.read(addr);
    }

    if (saved_state.magic_number != so_magic_number) {
        saved_state.magic_number = so_magic_number;
        saved_state.relay_is_on = false;
    }
}

static void update_relay(bool is_on) {
    if (is_on) {
        led->on();
        digitalWrite(RELAY_PIN, HIGH);
    } else {
        led->off();
        digitalWrite(RELAY_PIN, LOW);
    }
}

static void publish_state(BemfaConnection& conn, bool is_on) {
    auto msg = is_on ? String("on") : String("off");
    conn.publish(topic_set.c_str(), msg.c_str());
}

static BemfaTcp::EventListener event_listener;

void register_socket_01_handler(ESP8266Boot& boot, BemfaTcp &bemfaTcp, const String& topicPrefix, Led *theLed) {
    led = theLed;

    // init hardware
    pinMode(RELAY_PIN, OUTPUT);

    pinMode(LED_BLUE_PIN, OUTPUT);
    digitalWrite(LED_BLUE_PIN, LOW);

    // register boot event
    boot.setButtonPressListener([&]() {
        saved_state.relay_is_on = !saved_state.relay_is_on;

        update_relay(saved_state.relay_is_on);
        publish_state(bemfaTcp.getBemfaConnection(), saved_state.relay_is_on);

        save_state();
    });

    // register mqtt event
    topic = topicPrefix + "x001"; // socket device
    topic.toLowerCase();
    topic_set = topic + "/set";

    event_listener.onMessage = [](BemfaConnection& conn, const String &topic, const String &msg) {
        if (msg == "on") {
            if (!saved_state.relay_is_on) {
                update_relay(true);

                saved_state.relay_is_on = true;
                save_state();

                publish_state(conn, true);
            }
        } else if (msg == "off") {
            if (saved_state.relay_is_on) {
                update_relay(false);

                saved_state.relay_is_on = false;
                save_state();

                publish_state(conn, false);
            }
        }
    };

    event_listener.onConnect = [](BemfaConnection& conn) {
        // report relay state
        publish_state(conn, saved_state.relay_is_on);
    };

    bemfaTcp.onEvent(topic, event_listener);

    load_state();
    update_relay(saved_state.relay_is_on);
}
