#define CAMERA_MODEL_AI_THINKER

#include "config.h"
#include "espressifEsp32Cam.h"
#include "camera_pins.h"
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
const char* AP_WIFI_SSID = CONF_AP_WIFI_SSID;
const char* AP_WIFI_KEY = CONF_AP_WIFI_KEY;

unsigned long apActiveBeforeWifiReconnectInMinutes = 4;
unsigned long apActivationTime = 0;
int wifiReconnectsBeforApWillBeActivated = 10;
bool softApEnabled = false;


IPAddress localIp(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

//WebServer
int webserverPort = 8080;
WebServer server(webserverPort);

bool cameraServerStarted = false;

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
            Serial.printf("AccessPoint created with SSID %s and KEY %s and settings page http://%s:%d/settings\r\n", AP_WIFI_SSID, AP_WIFI_KEY, localIp.toString(), webserverPort);
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
    setupCameraServer();
    Serial.println("Setup done");
}

void setupCameraServer(){
    camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif
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

    if (cameraServerStarted == false && WiFi.status() == WL_CONNECTED){
        startCameraServer();
        cameraServerStarted = true;
    }
}
