#pragma once

#include <Arduino.h>
#include <AsyncMqttClient.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <map>
#include <list>
#include <functional>

class BemfaMqtt {
public:
    typedef std::function<void(AsyncMqttClient& client, const String& topic, const String& msg)> MessageListener;
    typedef std::function<void(AsyncMqttClient& client, const String& topic, uint8_t qos)> SubscribeListener;
    typedef std::function<void(AsyncMqttClient& client, bool sessionPresent)> ConnectListener;

    typedef struct {
        ConnectListener onConnect;
        MessageListener onMessage;
        SubscribeListener onSubscribe;
    } MqttEventListener;

    BemfaMqtt(const String& host, int port, const String& client_id)
        : _host(host), _port(port), _client_id(client_id) {
    };

    void onEvent(const String& topic, MqttEventListener listener) {
        _mlsm[topic].push_back(listener);
    };

    void begin() {
        _mqtt_client.setServer(_host.c_str(), _port);
        _mqtt_client.setClientId(_client_id.c_str());

        // Mqtt connection events
        _mqtt_client.onConnect([this](bool sessionPresent) {
            for (auto it = _mlsm.begin(); it != _mlsm.end(); ++it) {
                auto pid = _mqtt_client.subscribe(it->first.c_str(), 1);
                _pitm[pid] = it->first;
            }

            for (auto it = _mlsm.begin(); it != _mlsm.end(); ++it) {
                for (auto lsnr = it->second.begin(); lsnr != it->second.end(); ++lsnr) {
                    if (lsnr->onConnect) {
                        lsnr->onConnect(_mqtt_client, sessionPresent);
                    }
                }
            }
        });

        _mqtt_client.onDisconnect([this](AsyncMqttClientDisconnectReason reason) {
            if (WiFi.isConnected()) {
                _reconnect_ticker.once(2, [this]() {
                    _connect();
                });
            }
        });

        // Mqtt subscribe events
        _mqtt_client.onSubscribe([this](uint16_t packetId, uint8_t qos) {
            if (_pitm.count(packetId) == 1) {
                auto topic = _pitm[packetId];
                auto lst = _mlsm.at(topic);
                for (auto it = lst.begin(); it != lst.end(); ++it) {
                    SubscribeListener lsnr = it->onSubscribe;
                    if (lsnr) {
                        lsnr(_mqtt_client, topic, qos);
                    }
                }
            }
        });

        _mqtt_client.onUnsubscribe([this](uint16_t packetId) {
            // Nothing to do
        });

        // Mqtt pub/msg events
        _mqtt_client.onPublish([this](uint16_t packetId) {
            // Nothing to do
        });

        _mqtt_client.onMessage([this](char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
            auto msg = String("");
            for (size_t i = 0; i < len; i ++) {
                msg += payload[i];
            }
            if (_mlsm.count(topic) == 1) {
                auto lst = _mlsm.at(topic);
                for (auto it = lst.begin(); it != lst.end(); ++it) {
                    MessageListener lsnr = it->onMessage;
                    if (lsnr) {
                        lsnr(_mqtt_client, topic, msg);
                    }
                }
            }
        });

        // WiFi events
        _got_ip_handler = WiFi.onStationModeGotIP([this](const WiFiEventStationModeGotIP &) {
            _connect();
        });

        _disconnected_handler = WiFi.onStationModeDisconnected([this](const WiFiEventStationModeDisconnected &) {
            _reconnect_ticker.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
        });
    };

    void loop() {
    };

    AsyncMqttClient &getMqttClient() {
        return _mqtt_client;
    };
private:
    void _connect() {
        _mqtt_client.connect();
    };

private:
    String _host;
    int _port;
    String _client_id;

    typedef std::list<MqttEventListener> MqttEventListenerList;
    typedef std::map<String, MqttEventListenerList> MqttEventListenersMap;
    typedef std::map<uint16_t, String> PacketIdTopicMap;

    MqttEventListenersMap _mlsm;
    PacketIdTopicMap _pitm;

    AsyncMqttClient _mqtt_client;

    WiFiEventHandler _got_ip_handler;
    WiFiEventHandler _disconnected_handler;

    Ticker _reconnect_ticker;
};
