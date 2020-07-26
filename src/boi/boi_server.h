#ifndef __boi_server_h__
#define __boi_server_h__

#include "boi_wifi.h"
#include "ESPAsyncWebServer.h"

class RequestHandler : public AsyncWebHandler {
    public:
        RequestHandler();
        bool canHandle(AsyncWebServerRequest *request);
        void handleRequest(AsyncWebServerRequest *request);
        void GuestCounter();
        int CurrentGuestCount;
    private:
        Preferences preferences;

};

#endif