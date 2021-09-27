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
#include "Logger.h"


#define BUZZER_PIN  D3
#define LED         D4 //(D4 - built in)

#define MQTT_HOST  "xx.xx.xx.xx"    // MQTT host (m21.cloudmqtt.com)
#define MQTT_PORT  1883             // MQTT port (18076)   
#define MQTT_USER  "default_user"   // Ingored if brocker allows guest connection
#define MQTT_PASS  "KJdu3423bD"     // Ingored if brocker allows guest connection


#if __has_include("local-constants.h")
#include "local-constants.h"                    // Override some constants if local file exists
#endif


// Singleton instance of the radio driver
RH_NRF24            nrf24(D1, D2);
WiFiManager         wifiManager;
WiFiClient          wifiClient;
ESP8266WebServer    webServer(80);
MQTTClient          mqttClient(10000);
DubRtttl            rtttl(BUZZER_PIN);
Logger              logger;

String gDeviceName  = String() + "esp-bell-" + ESP.getChipId();
String gTopic       = "wifi2mqtt/esp-bell";


void myTone(int freq, int duration)
{
    tone(BUZZER_PIN, freq, duration);
    delay(duration);
}




void messageReceived(String &topic, String &payload) {
    logger.log("incoming: " + topic + " - " + payload);

    DynamicJsonDocument doc(10000);

    DeserializationError error = deserializeJson(doc, payload);
    if (error)
        return;

    // Fetch values.

    if(doc.containsKey("melody")){
        String melody = doc["melody"];

        // Print values.
        logger.log_no_ln("Playing melody: ");
        logger.print(melody);

        rtttl.play(melody);
    }
}

void mqtt_connect()
{
    static unsigned long stLastConnectTryTime = 0;

    // retring to connect not too often (every minute)
    if(stLastConnectTryTime && millis() - stLastConnectTryTime < 60000)
        return;

    stLastConnectTryTime = millis();

    logger.log_no_ln("Connecting to MQTT...");
    
    if(mqttClient.connect(gDeviceName.c_str(), MQTT_USER, MQTT_PASS))
    {
        logger.println(" connected!");
        mqttClient.subscribe(gTopic + "/set");
        mqttClient.subscribe(gTopic + "/play");
    }
    else
        logger.println(" failed. Retry in 60 sec..");
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

    if (!nrf24.setChannel(0x66)) {
        logger.log("setChannel failed");
    }

    if (!nrf24.setRF(RH_NRF24::DataRate2Mbps, RH_NRF24::TransmitPower0dBm)) {
        logger.log("setRF failed");
    }

    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH); // turn led off


    if(!SPIFFS.begin()){
        SPIFFS.format();
        SPIFFS.begin();
    }

    WiFi.mode(WIFI_STA); // no access point after connect

    wifi_set_sleep_type(NONE_SLEEP_T); // prevent wifi sleep (stronger connection)

    // On Access Point started (not called if wifi is configured)
    wifiManager.setAPCallback([](WiFiManager* mgr){
        logger.log( String("Please connect to Wi-Fi"));
        logger.log( String("Network: ") + mgr->getConfigPortalSSID());
        logger.log( String("Password: 12341234"));
        logger.log( String("Then go to ip: 10.0.1.1"));
    });

    wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
    wifiManager.setConfigPortalTimeout(60);
    wifiManager.autoConnect(gDeviceName.c_str(), "12341234"); // IMPORTANT! Blocks execution. Waits until connected

    // Restart if not connected
    if (WiFi.status() != WL_CONNECTED) 
    {
        ESP.restart();
    }

    // Host name should be set AFTER the wifi connect
    WiFi.hostname(gDeviceName.c_str());
    logger.log("Hostname",  gDeviceName);

    mqttClient.begin(MQTT_HOST, MQTT_PORT, wifiClient);
    logger.log("mqtt host", String()+MQTT_HOST);
    logger.log("mqtt port", String()+MQTT_PORT);

    mqttClient.onMessage(messageReceived);

    mqtt_connect();

    webServer.begin();

    String menu;
        menu += "<div>";
        menu += "<a href='/'>index</a> ";
        menu += "<a href='/play'>play</a> ";
        menu += "<a href='/fs'>fs</a> ";
        menu += "<a href='/logs'>logs</a> ";
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

    // Show logs page
    webServer.on("/logs", [menu](){
        String output = "";
        String output2 = "";
        output += menu;
        output += String() + "millis: <span id='millis'>"+millis()+"</span>";
        output += String() + "<br>size="+strlen(logger.buffer);
        output += String() + "<pre id='text'>";
        //output += String() + logger.buffer;
        output2 += String() + "</pre>\n";
        output2 += String() + "<script>\n";
        output2 += String() + "const millis = " + millis()+"\n";
        output2 += String() + "const replaceFunc = x => {\n";
        output2 += String() +    "const deltaMillis = millis - parseInt(x.replace('.', ''))\n";        
        output2 += String() +    "return new Date(Date.now() - deltaMillis).toLocaleString('RU')\n";
        output2 += String() + "}\n";
        output2 += String() + "document.getElementById('text').innerHTML = document.getElementById('text').innerHTML.replaceAll(/^\\d{3,}\\.\\d{3}/mg, replaceFunc)\n";
        output2 += String() + "</script>";
        webServer.setContentLength(output.length() + strlen(logger.buffer) + output2.length());
        webServer.send(200, "text/html", output);
        webServer.sendContent(String() + logger.buffer);
        webServer.sendContent(output2);
    });
    
    bool ok = mqttClient.publish("wifi2mqtt/esp-bell", "started");
    logger.log(ok ? "Published: OK" : "Published: ERR");

    // Initialize OTA (firmware updates via WiFi)
    ArduinoOTA.begin();

    // Play 'Setup completed' melody
    myTone(1000, 100);
    myTone(500, 100);
    myTone(1500, 100);
}

void radioLoop()
{

    if (nrf24.available())
    {
        // Should be a message for us now
        uint8_t buf[RH_NRF24_MAX_MESSAGE_LEN];
        uint8_t len = sizeof(buf);


        if(nrf24.waitAvailableTimeout(10))
        {
            digitalWrite(LED, LOW); // turn led on
            myTone(700, 10);
            while(nrf24.recv(buf, &len))
            {
                buf[len] = 0;
                String payload ((char*)buf);

                logger.log("< NRF msg:", payload);

                mqttClient.publish(gTopic, String()+"{\"nrf\":\""+payload+"\"}");

                // Send a reply
                {
                    bool ok = nrf24.send(buf, len); // same payload as replay
                    if(!ok) logger.log("Send failed");
                }

                bool ok = nrf24.waitPacketSent();
                if(!ok) logger.log("Wait sent failed");
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