#include "boi_wifi.h"
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
//#include "esp_http_client.h"


void send_post_to_battery_internet(const char *message, unsigned int length){
    return;

    WiFiMulti WiFiMulti;

    WiFiMulti.addAP("stupid_network", "stupid_password");

    // wait for WiFi connection
    Serial.print("Waiting for WiFi to connect...");
    while ((WiFiMulti.run() != WL_CONNECTED)) {
      Serial.print(".");
    }
    Serial.println(" connected");

    WiFiClientSecure *client = new WiFiClientSecure;
    if(client) {
        //client -> setCACert(rootCACertificate);

        {
        // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is 
        HTTPClient https;

        Serial.print("[HTTPS] begin...\n");
        if (https.begin(*client, "batteryinter.net")) {  // HTTPS
            Serial.print("[HTTPS] POST...\n");
            // start connection and send HTTP header
            String jsonData = "{\"RNG\"=\"FROSTEDBUTTS\"}";
            
            https.addHeader("Content-Type", "application/json", "Content-Length", jsonData.length());

            Serial.println("POSTING is hard work, and still under construction");

            int httpCode = https.POST(jsonData.c_str());

            // httpCode will be negative on error
            if (httpCode > 0) {
            // HTTP header has been send and Server response header has been handled
            Serial.printf("[HTTPS] POST... code: %d\n", httpCode);

            // file found at server
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
                String payload = https.getString();
                Serial.println(payload);
            }
            } else {
            Serial.printf("[HTTPS] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
            }

            https.end();
        } else {
            Serial.printf("[HTTPS] Unable to connect\n");
        }

        // End extra scoping block
        }

        delete client;
    } else {
        Serial.println("Unable to create client");
    }
}