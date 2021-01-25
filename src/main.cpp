#include <Arduino.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <WiFiManager.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include "DubRtttl.h"


#define BUZZER_PIN 4

#define MQTT_HOST  "192.168.1.157"  // MQTT host (m21.cloudmqtt.com)
#define MQTT_PORT  11883            // MQTT port (18076)   
#define MQTT_USER  "mfkrdxtb"       // Ingored if brocker allows guest connection
#define MQTT_PASS  "jD-qPTjdtV34"   // Ingored if brocker allows guest connection


DubRtttl    rtttl(BUZZER_PIN);
WiFiManager wifiManager;
WiFiClient  wifiClient;

ESP8266WebServer webServer(80);

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client    mqtt(&wifiClient, MQTT_HOST, MQTT_PORT/*, MQTT_USER, MQTT_PASS*/);
Adafruit_MQTT_Subscribe mqtt_subscribe (&mqtt, "wifi2mqtt/esp_bell/set");
Adafruit_MQTT_Publish   mqtt_publish   (&mqtt, "wifi2mqtt/esp_bell");


void myTone(int freq, int duration)
{
    tone(BUZZER_PIN, freq, duration);
    delay(duration);
}

/**
 * Conect to MQTT broker. Do nothing if already connected
 */
void mqtt_connect()
{

    // Do nothing if already connected.
    if (mqtt.connected())
    {
        return;
    }
    Serial.print("Connecting to MQTT... ");
    uint8_t retries = 10, ret;
    while ((ret = mqtt.connect()) != 0)
    { // connect will return 0 for connected
        Serial.println(mqtt.connectErrorString(ret));
        Serial.println("Retrying MQTT connection in 5 seconds...");
        mqtt.disconnect();
        delay(3000); // wait 3 seconds
        retries--;
        if (retries == 0)
        {
            return;
        }
    }
    Serial.println("MQTT Connected!");
}

void setup() 
{
    // Setup Serial port speed
    Serial.begin(115200);

    // Play 'start' melody
    myTone(800, 100);
    myTone(400, 100);
    myTone(1200, 100);


    String deviceName = "esp-bell";
    WiFi.hostname(deviceName);
    WiFi.mode(WIFI_STA); // no access point after connect

    wifi_set_sleep_type(NONE_SLEEP_T); // prevent wifi sleep (stronger connection)

    // On Access Point started (not called if wifi is configured)
    wifiManager.setAPCallback([](WiFiManager* mgr){
        Serial.println( String("Please connect to Wi-Fi"));
        Serial.println( String("Network: ") + mgr->getConfigPortalSSID());
        Serial.println( String("Password: 12341234"));
        Serial.println( String("Then go to ip: 10.0.1.1"));
    });

    wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
    wifiManager.setConfigPortalTimeout(60);
    wifiManager.autoConnect(deviceName.c_str(), "12341234"); // IMPORTANT! Blocks execution. Waits until connected

    // Restart if not connected
    if (WiFi.status() != WL_CONNECTED) 
    {
        ESP.restart();
    }


    // Setup MQTT subscription for the 'set' topic.
    // It is important to subscribe BEFORE the mqtt connect (!)
    mqtt.subscribe(&mqtt_subscribe);

    mqtt_subscribe.setCallback([](char *str, uint16_t len){
        Serial.print(String("Got mqtt message len="));
        Serial.println(len);
        char buf [len + 1];
        buf[len] = 0;
        strncpy(buf, str, len);

        Serial.println(String("Got mqtt message: ") + buf);
        DynamicJsonDocument doc(1024);

        DeserializationError error = deserializeJson(doc, buf);
        if (error)
            return;
        
        // Fetch values.

        if(doc.containsKey("melody")){
            String melody = doc["melody"];

            // Print values.
            Serial.print("Playing melody: ");
            Serial.println(melody);

            rtttl.play(melody);
        }

    });
    mqtt_connect();


    webServer.begin();

    String menu;
        menu += "<div>";
        menu += "<a href='/'>index</a> ";
        menu += "<a href='/logout'>logout</a> ";
        menu += "</div><hr>";

    webServer.on("/", [menu](){
        String str = ""; 
        str += menu;
        str += "<pre>";
        str += String() + "           Uptime: " + (millis() / 1000) + " \n";
        str += String() + "      FullVersion: " + ESP.getFullVersion() + " \n";
        str += String() + "      ESP Chip ID: " + ESP.getChipId() + " \n";
        str += String() + "         Hostname: " + WiFi.hostname() + " \n";
        str += String() + "       CpuFreqMHz: " + ESP.getCpuFreqMHz() + " \n";
        str += String() + "      WiFi status: " + wifiClient.status() + " \n";
        str += String() + "         FreeHeap: " + ESP.getFreeHeap() + " \n";
        str += String() + "       SketchSize: " + ESP.getSketchSize() + " \n";
        str += String() + "  FreeSketchSpace: " + ESP.getFreeSketchSpace() + " \n";
        str += String() + "    FlashChipSize: " + ESP.getFlashChipSize() + " \n";
        str += String() + "FlashChipRealSize: " + ESP.getFlashChipRealSize() + " \n";
        str += "</pre>";

        webServer.send(200, "text/html; charset=utf-8", str);     
    });

    // Logout (reset wifi settings)
    webServer.on("/logout", [menu](){
        if(webServer.method() == HTTP_POST){
            webServer.send(200, "text/html", "OK");
            wifiManager.resetSettings();
            ESP.reset();
        }
        else{
            String output = "";
            output += menu;
            output += String() + "<pre>";
            output += String() + "Wifi network: " + WiFi.SSID() + " \n";
            output += String() + "        RSSI: " + WiFi.RSSI() + " \n";
            output += String() + "    hostname: " + WiFi.hostname() + " \n";
            output += String() + "</pre>";
            output += "<form method='post'><button>Forget</button></form>";
            webServer.send(200, "text/html", output);
        }
    });

    bool ok = mqtt_publish.publish("started");
    Serial.println(ok ? "Published: OK" : "Published: ERR");

    // Initialize OTA (firmware updates via WiFi)
    ArduinoOTA.begin();

    // Play 'Setup completed' melody
    myTone(1000, 100);
    myTone(500, 100);
    myTone(1500, 100);
}

void loop() 
{
    mqtt_connect();
    ArduinoOTA.handle();
    rtttl.updateMelody();
    mqtt.processPackets(10);
    webServer.handleClient();
}