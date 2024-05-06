#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <map>
#include <list>
#include <functional>

class BemfaConnection {
    friend class BemfaTcp;
public:
    void publish(const char* topic, const char* payload) {
        _client.printf("cmd=2&uid=%s&topic=%s&msg=%s\r\n", _client_id.c_str(), topic, payload);
    };
private:
    typedef std::function<void()> ConnectListener;
    typedef std::function<void(const char* topic, const char* payload)> MessageListener;

    BemfaConnection() :
        _want_connect(false),
        _last_ping_time(0),
        _buf_used(0) {};

    void setServer(const char* host, uint16_t port) {
        _host = host;
        _port = port;
    };

    void setClientId(const char* client_id) {
        _client_id = client_id;
    };

    void setConnect(bool want_connect) {
        _want_connect = want_connect;
    };

    void subscribe(const char* topic) {
        _client.printf("cmd=3&uid=%s&topic=%s\r\n", _client_id.c_str(), topic);
    };

    // void unsubscribe(const char* topic) {};

    void onConnect(ConnectListener listener) {
        _on_connect_listener = listener;
    };

    void onMessage(MessageListener listener) {
        _on_message_listener = listener;
    };

    void loop() {
        if (_want_connect) {
            if (!_client.connected()) {
                if (_client.connect(_host.c_str(), _port)) {
                    if (_on_connect_listener) {
                        _on_connect_listener();
                    }
                }
            } else {
                if (_client.available()) {
                    auto n = _client.read(_buf + _buf_used, sizeof(_buf) - _buf_used);
                    if (n > 0) {
                        _buf_used += n;
                        if (_buf[_buf_used - 1] == '\n') {
                            if ((_buf_used > 2) && (_buf[_buf_used - 2] == '\r')) {
                                _buf[_buf_used - 2] = 0;
                                String msg(_buf);
                                msg.trim();
                                _buf_used = 0;

                                auto topic_key_pos = msg.indexOf("&topic=");
                                String topic;
                                if (topic_key_pos >= 0) {
                                    auto topic_end_pos = msg.indexOf("&", topic_key_pos + 7);
                                    if (topic_end_pos >= 0) {
                                        topic = msg.substring(topic_key_pos + 7, topic_end_pos);
                                    }
                                }
                                auto msg_key_pos = msg.indexOf("&msg=");
                                String msg_str;
                                if (msg_key_pos >= 0) {
                                    auto msg_end_pos = msg.indexOf("&", msg_key_pos + 5);
                                    if (msg_end_pos < 0) {
                                        msg_end_pos = msg.length();
                                    }
                                    msg_str = msg.substring(msg_key_pos + 5, msg_end_pos);
                                }

                                if (topic_key_pos >= 0 && msg_key_pos >= 0) {
                                    if (_on_message_listener) {
                                        _on_message_listener(topic.c_str(), msg_str.c_str());
                                    }
                                }
                            }
                        }
                    }

                    if (_buf_used >= sizeof(_buf)) {
                        _buf_used = 0;
                    }
                }

                // ping every 15 seconds
                if (millis() - _last_ping_time > 15000) {
                    _client.print("ping\r\n");
                    _last_ping_time = millis();
                }
            }
        } else {
            if (_client.connected()) {
                _client.stop();
            }
        }
    };

private:
    String _host;
    uint16_t _port;
    String _client_id;

    ConnectListener _on_connect_listener;
    MessageListener _on_message_listener;

    WiFiClient _client;
    bool _want_connect;

    uint32_t _last_ping_time;
    char _buf[256];
    size_t _buf_used;
};

class BemfaTcp {
public:
    typedef std::function<void(BemfaConnection& conn, const String& topic, const String& msg)> MessageListener;
    typedef std::function<void(BemfaConnection& conn)> ConnectListener;

    typedef struct {
        ConnectListener onConnect;
        MessageListener onMessage;
    } EventListener;

    BemfaTcp(const String& host, int port, const String& client_id)
        : _host(host), _port(port), _client_id(client_id) {
    };

    void onEvent(const String& topic, EventListener listener) {
        _mlsm[topic].push_back(listener);
    };

    void begin() {
        _bemfa_conn.setServer(_host.c_str(), _port);
        _bemfa_conn.setClientId(_client_id.c_str());

        // Bemfa connection events
        _bemfa_conn.onConnect([this]() {
            for (auto it = _mlsm.begin(); it != _mlsm.end(); ++it) {
                _bemfa_conn.subscribe(it->first.c_str());
            }

            for (auto it = _mlsm.begin(); it != _mlsm.end(); ++it) {
                for (auto lsnr = it->second.begin(); lsnr != it->second.end(); ++lsnr) {
                    if (lsnr->onConnect) {
                        lsnr->onConnect(_bemfa_conn);
                    }
                }
            }
        });

        _bemfa_conn.onMessage([this](const char* topic, const char* payload) {
            if (_mlsm.count(topic) == 1) {
                auto lst = _mlsm.at(topic);
                for (auto it = lst.begin(); it != lst.end(); ++it) {
                    MessageListener lsnr = it->onMessage;
                    if (lsnr) {
                        lsnr(_bemfa_conn, topic, payload);
                    }
                }
            }
        });

        // WiFi events
        _got_ip_handler = WiFi.onStationModeGotIP([this](const WiFiEventStationModeGotIP &) {
            _bemfa_conn.setConnect(true);
        });

        _disconnected_handler = WiFi.onStationModeDisconnected([this](const WiFiEventStationModeDisconnected &) {
            _bemfa_conn.setConnect(false); // ensure we don't reconnect to Bemfa while reconnecting to Wi-Fi
        });
    };

    void loop() {
        _bemfa_conn.loop();
    };

    BemfaConnection &getBemfaConnection() {
        return _bemfa_conn;
    };
private:
    String _host;
    int _port;
    String _client_id;

    typedef std::list<EventListener> MqttEventListenerList;
    typedef std::map<String, MqttEventListenerList> MqttEventListenersMap;

    MqttEventListenersMap _mlsm;

    BemfaConnection _bemfa_conn;

    WiFiEventHandler _got_ip_handler;
    WiFiEventHandler _disconnected_handler;
};
