#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <WiFiMulti.h>
#include <OneWire.h>
#include <DallasTemperature.h>


#define WIFI_SSID "netis_2.4G_56336C"
#define WIFI_PASSWORD "password"

#define WS_HOST "c5k96spn7j.execute-api.us-east-1.amazonaws.com"
#define WS_PORT 443
#define WS_URL "/dev"
#define MSG_SIZE 256

WebSocketsClient wsClient;
WiFiMulti wifiMulti;

void sendErrorMessage(const char *error) {
  char msg[MSG_SIZE];
  sprintf(msg, "{\"action\":\"msg\",\"type\":\"error\",\"body\": {\"data\":\"%s\"}}", error);
  wsClient.sendTXT(msg);
}

void sendOkMessage() {
  wsClient.sendTXT("{\"action\":\"msg\",\"type\":\"OK\"}");
}

uint8_t toMode(const char *val) {
  if (strcmp(val, "output") == 0) {
    return OUTPUT;
  }

  return INPUT;
}


void handleMessage(uint8_t * payload) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);

  if(error) {
    Serial.println(error.f_str());
    sendErrorMessage(error.c_str());
    return;
  }

  if (!doc["type"].is<const char *>()) {
    sendErrorMessage("invalid message type format");
    return;
  }

  if (strcmp(doc["type"], "cmd") == 0) {
    if (!doc["body"].is<JsonObject>()) {
      sendErrorMessage("invalid command body");
      return;
    }
    if (strcmp(doc["body"]["type"], "pinMode") == 0) {
      pinMode(doc["body"]["pin"], toMode(doc["body"]["mode"]));
      sendOkMessage();
      return;
    }
    if (strcmp(doc["body"]["type"], "digitalWrite") == 0) {
      digitalWrite(doc["body"]["pin"], doc["body"]["value"]);
      sendOkMessage();
      return;
    }
    if (strcmp(doc["body"]["type"], "digitalRead") == 0) {
      float pin = doc["body"]["pin"];
      auto value = digitalRead(pin);
      char msg[MSG_SIZE];
      sprintf(msg, "{\"action\":\"msg\",\"type\":\"output\",\"body\":%d, \"pin\":%f}",
              value, pin);
      wsClient.sendTXT(msg);
      return;
    }
    if (strcmp(doc["body"]["type"], "getTemp") == 0) {
      Serial.println("Recieve message for temperature");
      float pin = doc["body"]["pin"];
      Serial.println(pin);
      OneWire oneWire(pin);
      DallasTemperature sensor(&oneWire);
      sensor.begin();
      while (1) {
        Serial.println("Read temperature");
        sensor.requestTemperatures();
        delay(500);
        auto tempC = sensor.getTempCByIndex(0);
        Serial.println(tempC);
        char msg[MSG_SIZE]; 
        sprintf(msg, "{\"action\":\"msg\",\"type\":\"temp\",\"body\":%f, \"pin\":%f}",
          tempC, pin);
        wsClient.sendTXT(msg);    
        delay(500);
      }
      return;
    }
    sendErrorMessage("unsupported command type");
    return;
  }
  sendErrorMessage("unsupported message type");
  return;
}

void onWsEventHandler(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_CONNECTED:
      Serial.println("WS Conected");
      break;
    case WStype_DISCONNECTED:
      Serial.println("WS Disonected");
      break;
    case WStype_TEXT:
      Serial.printf("WS Message: %s\n", payload);
      handleMessage(payload);
      break;
  }
}

void setup() {
  Serial.begin(9600);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  while (wifiMulti.run() != WL_CONNECTED) {
    delay(100);
  }

  Serial.println("Wifi conected!");

  wsClient.beginSSL(WS_HOST, WS_PORT, WS_URL, "", "wss");
  wsClient.onEvent(onWsEventHandler);
}

void loop() {
  wsClient.loop();
}

