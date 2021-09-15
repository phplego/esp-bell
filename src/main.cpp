#include <Arduino.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <WiFiManager.h>
#include <MQTT.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>

#include <SPI.h>
#include <RH_NRF24.h>

#include "DubRtttl.h"


#define BUZZER_PIN D3


#define LED D4 //(D4 - built in)

#define MQTT_HOST  "192.168.1.157"  // MQTT host (m21.cloudmqtt.com)
#define MQTT_PORT  11883            // MQTT port (18076)   
#define MQTT_USER  "mfkrdxtb"       // Ingored if brocker allows guest connection
#define MQTT_PASS  "jD-qPTjdtV34"   // Ingored if brocker allows guest connection


// Singleton instance of the radio driver
RH_NRF24 nrf24(D1, D2); // use this for NodeMCU Amica/AdaFruit Huzzah ESP8266 Feather

DubRtttl    rtttl(BUZZER_PIN);
WiFiManager wifiManager;
WiFiClient  wifiClient;

ESP8266WebServer webServer(80);



MQTTClient mqttClient(10000);


void myTone(int freq, int duration)
{
    tone(BUZZER_PIN, freq, duration);
    delay(duration);
}




void messageReceived(String &topic, String &payload) {
    Serial.println("incoming: " + topic + " - " + payload);

    DynamicJsonDocument doc(10000);

    DeserializationError error = deserializeJson(doc, payload);
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
}

void mqtt_connect() {
    Serial.print("checking wifi...");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(1000);
    }

    Serial.print("\nconnecting...");
    while (!mqttClient.connect("esp-bell", MQTT_USER, MQTT_PASS)) {
        Serial.print(".");
        delay(1000);
    }

    Serial.println("\nconnected!");

    mqttClient.subscribe("wifi2mqtt/esp-bell/set");
}

void setup() 
{
    // Setup Serial port speed
    Serial.begin(115200);

    // Play 'start' melody
    myTone(800, 100);
    myTone(400, 100);
    myTone(1200, 100);


    // NRF setup
    nrf24.init();

    if (!nrf24.setChannel(4)) {
        Serial.println("setChannel failed");
    }

    if (!nrf24.setRF(RH_NRF24::DataRate2Mbps, RH_NRF24::TransmitPower0dBm)) {
        Serial.println("setRF failed");
    }

    pinMode(LED, OUTPUT);


    if(!SPIFFS.begin()){
        SPIFFS.format();
        SPIFFS.begin();
    }

    String deviceName = "esp-bell";
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

    // Host name should be set AFTER the wifi connect
    WiFi.hostname(deviceName);

    mqttClient.begin(MQTT_HOST, MQTT_PORT, wifiClient);
    mqttClient.onMessage(messageReceived);

    mqtt_connect();

    webServer.begin();

    String menu;
        menu += "<div>";
        menu += "<a href='/'>index</a> ";
        menu += "<a href='/play'>play</a> ";
        menu += "<a href='/fs'>fs</a> ";
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

    // Show filesystem list of files
    webServer.on("/fs", [menu](){
        String output = "";
        output += menu;

        output += String() + "<pre>";

        File f = SPIFFS.open("/manual.txt", "w");
        f.print("hello");
        f.close();
        Dir dir = SPIFFS.openDir("/");
        while (dir.next()) {
            output += String() + dir.fileSize() + "B " + dir.fileName() + "\n";
        }

        output += String() + "</pre>";
        webServer.send(400, "text/html", output);
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

    // Play melody 
    webServer.on("/play", [menu](){
        String output = "";
        output += menu;
        String melody = "Intel:d=16,o=5,b=320:d,p,d,p,d,p,g,p,g,p,g,p,d,p,d,p,d,p,a,p,a,p,a,2p";
        if(webServer.method() == HTTP_POST){
            melody = webServer.arg("melody");
            rtttl.play(melody);
            output += "Playing melody: " + melody;
        }
        output += "<form method='POST'><textarea name='melody' rows=10>"+melody+"</textarea><br><button>play</button></form>";
        webServer.send(200, "text/html", output);
    });


    bool ok = mqttClient.publish("wifi2mqtt/esp-bell", "started");
    Serial.println(ok ? "Published: OK" : "Published: ERR");

    // Initialize OTA (firmware updates via WiFi)
    ArduinoOTA.begin();

    // Play 'Setup completed' melody
    myTone(1000, 100);
    myTone(500, 100);
    myTone(1500, 100);
}

void radioLoop()
{
    static uint32 lastCounter = 0;
    
    if (nrf24.available())
    {
        // Should be a message for us now
        uint8_t buf[RH_NRF24_MAX_MESSAGE_LEN];
        uint8_t len = sizeof(buf);


        if(nrf24.waitAvailableTimeout(100))
        {
            digitalWrite(LED, LOW); // turn led on
            delay(10);
            while(nrf24.recv(buf, &len))
            {
                uint32 counter = ((uint32 *) buf)[0];
                Serial.print("Received:");
                Serial.println(counter);
                if(lastCounter && counter > lastCounter + 1)
                  Serial.println(String() + "Failed packets " + (counter - lastCounter - 1));
                lastCounter = counter;  

                // Send a reply
                {
                String replay = "Confirm ";
                replay += counter;
                //nrf24.send(sdata, sizeof(sdata));
                bool ok = nrf24.send((uint8_t *) replay.c_str(), replay.length());
                if(!ok) Serial.println("Send failed");
                }

                bool ok = nrf24.waitPacketSent();
                if(!ok) Serial.println("Wait sent failed");
            }
            digitalWrite(LED, HIGH); // turn led off
        } else {
            // no new message
        }
    }
}

void loop() 
{
    ArduinoOTA.handle();
    rtttl.updateMelody();
    mqttClient.loop();

    if (!mqttClient.connected()) {
        mqtt_connect();
    }

    webServer.handleClient();
    radioLoop();
}