#include "boi_wifi.h"
#include <HTTPClient.h>

pthread_mutex_t http_lock;
HTTPClient http;

void send_post_to_battery_internet(const uint8_t *message, unsigned int length){
    uint8_t *Data;

    if(!_globalBoiWifi || !(Mode == boi_wifi::SafeModeWithNetworking) || (WiFi.status() != WL_CONNECTED))
        return;

    pthread_mutex_lock(&http_lock);

    //can send the message
    Serial.print("[HTTP] begin...\n");
    if (http.begin("http://batteryinter.net/battery.php")) {  // HTTP
        Serial.printf("[HTTP] POSTing %d bytes...\n", length + 17);

        // start connection and send HTTP data
        Data = (uint8_t *)malloc(length + 17);
        if(!Data)
        {
            printf("Out of memory\n");
            return;
        }

        memcpy(Data, WiFi.macAddress().c_str(), 17);
        memcpy(&Data[17], message, length);
    
        //content length unneeded, auto added by POST
        http.addHeader("Content-Type", "text/plain");
        int httpCode = http.POST(Data, length + 17);
        free(Data);

        // httpCode will be negative on error
        if (httpCode > 0) {
            // HTTP header has been send and Server response header has been handled
            Serial.printf("[HTTP] POST... code: %d\n", httpCode);

            // file found at server
            if (httpCode == HTTP_CODE_OK) {
                Serial.println("[HTTP] POST complete");
            }
        } else {
            Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
        }

        http.end();
    } else {
        Serial.printf("[HTTP] Unable to connect\n");
        http.end();
    }

    pthread_mutex_unlock(&http_lock);
}

void do_battery_checkin(){
    uint8_t *Data;

    if(!_globalBoiWifi || !(Mode == boi_wifi::SafeModeWithNetworking) || (WiFi.status() != WL_CONNECTED))
        return;

    pthread_mutex_lock(&http_lock);

    //can send the message
    Serial.print("[HTTP] begin...\n");
    if (http.begin("http://batteryinter.net/checkin")) {  // HTTP
        Serial.println("[HTTP] POSTing to checkin...");

        // start connection and send HTTP data
        String LocalIP = WiFi.localIP().toString();
        Data = (uint8_t *)malloc(17 + LocalIP.length());
        if(!Data)
        {
            printf("Out of memory\n");
            return;
        }

        memcpy(Data, WiFi.macAddress().c_str(), 17);
        memcpy(&Data[17], LocalIP.c_str(), LocalIP.length());
    
        //content length unneeded, auto added by POST
        http.addHeader("Content-Type", "text/plain");
        int httpCode = http.POST(Data, LocalIP.length() + 17);
        free(Data);

        // httpCode will be negative on error
        if (httpCode > 0) {
            // HTTP header has been send and Server response header has been handled
            Serial.printf("[HTTP] POST... code: %d\n", httpCode);

            // file found at server
            if (httpCode == HTTP_CODE_OK) {
                Serial.println("[HTTP] POST complete");
            }
        } else {
            Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
        }

        http.end();
    } else {
        Serial.printf("[HTTP] Unable to connect\n");
        http.end();
    }

    pthread_mutex_unlock(&http_lock);
}