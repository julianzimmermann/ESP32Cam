#include "config.h"
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <WebServer.h>

unsigned long systemStartTime = millis();
unsigned long second = 1000;
unsigned long minute = second * 60;

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

// Soft AP
#ifdef CONF_AP_WIFI_SSID
    const char* AP_WIFI_SSID = CONF_AP_WIFI_SSID
#else
    const char* AP_WIFI_SSID = "ESP32_CAM";
#endif
#ifdef CONF_AP_WIFI_KEY
    const char* AP_WIFI_KEY = CONF_AP_WIFI_KEY;
#else
    const char* AP_WIFI_KEY = "123ESP32123";
#endif

unsigned long apActiveBeforeWifiReconnectInMinutes = 5;
unsigned long apActivationTime = 0;
int wifiReconnectsBeforApWillBeActivated = 10;
bool softApEnabled = false;


IPAddress localIp(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

//WebServer
int webserverPort = 80;
WebServer server(webserverPort);

void connectWifi(){
    Serial.println(WiFi.status());
    WiFi.begin(WIFI_SSID, WIFI_KEY);
    wifiConnectionAttemps++;
    wifiLastConnectionAttemp = millis();

    Serial.printf("Wifi connection attemp %i to %s\r\n", wifiConnectionAttemps, WIFI_SSID);
}

boolean checkWifiConnection() {
    unsigned long now = millis();

    if (softApEnabled == true && now >= (apActiveBeforeWifiReconnectInMinutes * minute + apActivationTime)){
        Serial.printf("Closing AccessPoint after activation time timeout of %d minutes", static_cast<unsigned long int>(apActiveBeforeWifiReconnectInMinutes));
        Serial.println("Trying reconnect to wifi...");
        return stopSoftAp();
    }

    if (wifiConnectionAttemps == wifiReconnectsBeforApWillBeActivated){
        return createSoftAp();
    }

    if (WiFi.status() != WL_CONNECTED && (now - wifiLastConnectionAttemp) >= wifiReconnectionDelay){
        connectWifi();
    }

    if (WiFi.status() == WL_CONNECTED){
        wifiConnectionAttemps = 0;
    }
    return WiFi.status() == WL_CONNECTED;
}

void activateOta() {
    if (ota && WiFi.status() == WL_CONNECTED && otaSetupDone == false) {
        ArduinoOTA.setHostname(OTA_HOST);
        ArduinoOTA.setPassword(OTA_PASS);
        ArduinoOTA.begin();
        Serial.println("OTA configured!");
        otaSetupDone = true;
    }
}

//################################## WebServer Stuff START
void setupWebServer(){
    server.on("/", handleIndexAction);
    server.on("/settings", handleSettingsAction);
    server.begin();
}

String indexPage = "<!DOCTYPE html>\
<html>\
<body>\
<h1>ESP32Cam Webserver - Index</h1>\
</body>\
</html>";

String settingsPage = "<!DOCTYPE html>\
<html>\
<body>\
<h1>ESP32Cam Webserver - Settings</h1>\
</body>\
</html>";

void handleIndexAction() {
  server.send(200, "text/html", indexPage);
}



void handleSettingsAction() {
  server.send(200, "text/html", settingsPage);
}

//################################## WebServer Stuff END

bool createSoftAp(){
    try
    {
        if (softApEnabled == false){
            WiFi.enableAP(true);
            WiFi.softAP(AP_WIFI_SSID, AP_WIFI_KEY);
            WiFi.softAPConfig(localIp, gateway, subnet);
            apActivationTime = millis();
            softApEnabled = true;
            Serial.printf("AccessPoint created with SSID %s and KEY %s and settings page http://%s:%d/settings\r\n", AP_WIFI_SSID, AP_WIFI_KEY, WiFi.localIP(), webserverPort);
            return true;
        }
    }
    catch(const std::exception& e)
    {
        softApEnabled = false;
        Serial.printf("Exception %s\r\n", e.what());
    }

    if (softApEnabled == true){
        return true;
    }
    
    Serial.printf("Could not create AccessPoint! %i\r\n", WiFi.status());
    return false;
}

bool stopSoftAp(){
    Serial.println('Closing AccesPoint');
    wifiConnectionAttemps = 0;
    softApEnabled = false;
    apActivationTime = 0;
    return WiFi.enableAP(false);
}

void setup() {
    Serial.begin(115200);
    setupWifi();
    setupWebServer();
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
            activateOta();
        }

        server.handleClient();
    }
}
