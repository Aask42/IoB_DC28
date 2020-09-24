#ifndef __boi_wifi_h__
#define __boi_wifi_h__

#include <Arduino.h>
#include "DNSServer.h"                  // Patched lib
#include "boi.h"
#include <pthread.h>
#include "messages.h"
#include "app.h"

typedef class HTTPClient HTTPClient;
extern HTTPClient http;

class boi;
class Messages;
struct OptionsStruct;

typedef class AsyncWebServer AsyncWebServer;
typedef class RequestHandler RequestHandler;

class boi_wifi
{
public:
    typedef enum WifiModeEnum
    {
        NormalMode = 1,
        BusinessCardMode = 2,
        RickMode = 3,
        PartyMode = 4,
        SafeModeWithNetworking = 5
    } WifiModeEnum;

    boi_wifi(boi *boiData, Messages *message_handler, WifiModeEnum NewMode);
    ~boi_wifi();

    void handleRoot();
    void monitor_captive_portal();
    void monitor_smwn();
    void RefreshSettings();


    private:
        boi *_boi;
        char *HandleName;
        char *CaptivePortalName;
        DNSServer *dnsServer;
        AsyncWebServer *server;
        Messages *message_handler;
        RequestHandler *request_handler;

        pthread_t ServerCheckThread;
        Preferences preferences;

        void setup_captive_portal(const OptionsStruct *Options);
        void enter_safe_mode_with_networking(const OptionsStruct *Options);

        void Reconfigure(const OptionsStruct *Options);
        void DisableWiFi();
        void SetupRequestServer();
        void DeleteWebServer();
        
        void ActivateBusinessCard();
        void ActivateRick();
        void ActivateParty();
        void ActivateNormal();
        void ActivateSafeModeWithNetworking();
};

extern boi_wifi::WifiModeEnum Mode;
extern boi_wifi *_globalBoiWifi;
void send_post_to_battery_internet(const uint8_t *message, unsigned int length);
void do_battery_checkin();

#endif
