#include "config.h"
#include <ArduinoOTA.h>
#include <WiFi.h>

unsigned long systemStartTime = millis();

// Wifi
const char* HOSTNAME = CONF_HOSTNAME;
const char* WIFI_SSID = CONF_WIFI_SSID;
const char* WIFI_KEY = CONF_WIFI_KEY;

unsigned long wifiLastConnectionAttemp = systemStartTime;
unsigned long wifiReconnectionDelay = 10000;
unsigned int wifiConnectionAttemps = 0;

// OTA
const char* OTA_HOST = CONF_OTA_HOST;
const char* OTA_PASS = CONF_OTA_PASS;

bool ota = CONF_OTA;
bool otaSetupDone = false;

void connectWifi(){
    // WiFi.disconnect();
    Serial.println(WiFi.status());
    WiFi.begin(WIFI_SSID, WIFI_KEY);
    wifiConnectionAttemps++;
    wifiLastConnectionAttemp = millis();

    Serial.printf("Wifi connection attemp %i to %s\r\n", wifiConnectionAttemps, WIFI_SSID);
}

boolean checkWifiConnection() {
    unsigned long now = millis();
    if (WiFi.status() != WL_CONNECTED && (now - wifiLastConnectionAttemp) >= wifiReconnectionDelay){
        connectWifi();
    }

    if (WiFi.status() == WL_CONNECTED){
        wifiConnectionAttemps = 0;
    }
    return WiFi.status() == WL_CONNECTED;
}

void setupOta() {
    if (ota && WiFi.status() == WL_CONNECTED && otaSetupDone == false) {
        ArduinoOTA.setHostname(OTA_HOST);
        ArduinoOTA.setPassword(OTA_PASS);
        ArduinoOTA.begin();
        Serial.println("OTA configured!");
        otaSetupDone = true;
    }
}

void setup() {
    Serial.begin(115200);
    setupWifi();
    
    Serial.println("Setup done");
}

void setupWifi(){
    WiFi.disconnect();
    delay(100);
    WiFi.mode(WIFI_MODE_STA);
    WiFi.persistent(false);
    WiFi.setHostname(HOSTNAME);
}

void loop() {
    if (checkWifiConnection()){
        if (otaSetupDone == false){
            setupOta();
        }
    }
}
