#include "boi_wifi.h"
#include "app.h"
#include "boi_server.h"
#include "html-website-header.h"
#include "html-businesscard-header.h"
#include "html-rick-header.h"
#include "html-party-header.h"
#include "html-resume-header.h"

boi_wifi::WifiModeEnum Mode;

RequestHandler::RequestHandler() {}

bool RequestHandler::canHandle(AsyncWebServerRequest *request) { // this method is our config to tell ESPAsyncWebserver what requests we want to handle
    return true; // this means we attempt to handle ALL traffic, not filtering on anything
    // TODO: we're bypassing this during a fix from Lightning a year ago, is this the code below an incomplete feature??

    // TODO: Update to pull the IP address from preferences
    if(request->host() == WiFi.localIP().toString()) {
        request->addInterestingHeader("Cache-Control: no-cache, no-store, must-revalidate");
        request->addInterestingHeader("Pragma: no-cache");
        request->addInterestingHeader("Expires: -1");
    }
    return true;
}

void RequestHandler::handleRequest(AsyncWebServerRequest *request) { // why are we always sending back empty bodies?
        //TODO: send back index.html & also JS ??
    Serial.println("boi_server.cpp -> handleRequest() called");
    Serial.printf("host: %s, url: %s\n", request->host().c_str(), request->url().c_str());
    if((request->url() == "/whoami")) {
        request->send(200, "text/html", "");
    }else if((request->url() == "/GuestCounter")) {   
        this->GuestCounter();
        request->send(200, "text/html", String(this->CurrentGuestCount));
    }else if((request->url() == "/favicon.ico")) {
        request->send(404, "text/html", "");
    }else if((request->url() == "/css.ico")) {
        request->send(404, "text/html", "");
    }else{
        //AsyncResponseStream *response = request->beginResponseStream("text/html");
/*
        switch(Mode)
        {
            case boi_wifi::NormalMode:
                request->send_P(200, "text/html", HTMLWebsiteData);
                break;

            case boi_wifi::SafeModeWithNetworking:
                request->send_P(200, "text/html", HTMLWebsiteData);
                break;

            case boi_wifi::PartyMode:
                request->send_P(200, "text/html", HTMLPartyData);
                break;

            case boi_wifi::BusinessCardMode:
                request->send_P(200, "text/html", HTMLBusinessCardData);
                break;

            default:
                request->send_P(200, "text/html", HTMLRickData);
                break;
        }
    }
*/
            // https://github.com/me-no-dev/ESPAsyncWebServer#send-large-webpage-from-progmem-and-extra-headers
            // https://github.com/me-no-dev/ESPAsyncWebServer#send-binary-content-from-progmem
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", HTMLWebsiteData, HTMLWebsiteDataLen);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
        //request->send_P(200, "text/html", HTMLWebsiteData);
    }
    Serial.printf("handled request\n");
}

void boi_wifi::SetupRequestServer() {
    this->server = new AsyncWebServer(80);
    this->request_handler = new RequestHandler();
    this->server->addHandler(this->request_handler);
    this->server->begin(); //start the server
    Serial.print("Successfully began broadcasting wifi InternetOfBatteries!!!\n\n");
}

void boi_wifi::DeleteWebServer() {
    //remove and delete the request handler
    if(!this->server) {
        return;
    }
    this->server->removeHandler(this->request_handler);
    this->server->end();
    delete this->server; //delete the memory
}

void boi_wifi::ActivateBusinessCard() {
    const OptionsStruct *Options;
    OptionsStruct NewOptions;
    Mode = BusinessCardMode;
    Options = this->message_handler->GetOptions();
    memcpy(&NewOptions, Options, sizeof(OptionsStruct));
    NewOptions.WifiPassword[0] = 0;
    this->setup_captive_portal(&NewOptions);
        //indicate wifi was business card
    this->preferences.begin("options");
    this->preferences.putUChar("wifi", boi_wifi::BusinessCardMode);
    this->preferences.end();
}

void boi_wifi::ActivateRick() {
    const OptionsStruct *Options;
    OptionsStruct NewOptions;
    Mode = RickMode;
    Options = this->message_handler->GetOptions();
    memcpy(&NewOptions, Options, sizeof(OptionsStruct));
    if(strlen(Options->OriginalWifiName)) {
        memcpy(NewOptions.WifiName, Options->OriginalWifiName, sizeof(Options->OriginalWifiName));
    }
    this->setup_captive_portal(&NewOptions);
        //indicate wifi was rick    
    this->preferences.begin("options");
    this->preferences.putUChar("wifi", boi_wifi::RickMode);
    this->preferences.end();
}

void boi_wifi::ActivateParty() {
    const OptionsStruct *Options;
    OptionsStruct NewOptions;
    Mode = PartyMode;
    Options = this->message_handler->GetOptions();
    memcpy(&NewOptions, Options, sizeof(OptionsStruct));
    if(strlen(Options->OriginalWifiName)) {
        memcpy(NewOptions.WifiName, Options->OriginalWifiName, sizeof(Options->OriginalWifiName));
    }
    this->setup_captive_portal(&NewOptions);
        //indicate wifi was party
    this->preferences.begin("options");
    this->preferences.putUChar("wifi", boi_wifi::PartyMode);
    this->preferences.end();
}

void boi_wifi::ActivateNormal() { // setup Server to host Captive Portal
    const OptionsStruct *Options;
    Mode = NormalMode;
    Options = this->message_handler->GetOptions();
    this->setup_captive_portal(Options);
        //indicate wifi was normal
    this->preferences.begin("options");
    this->preferences.putUChar("wifi", boi_wifi::NormalMode);
    this->preferences.end();
}

void boi_wifi::ActivateSafeModeWithNetworking() {
    const OptionsStruct *Options;
    Mode = SafeModeWithNetworking;
    Options = this->message_handler->GetOptions();
    this->enter_safe_mode_with_networking(Options);
        //indicate wifi was normal
    this->preferences.begin("options");
    this->preferences.putUChar("wifi", boi_wifi::SafeModeWithNetworking);
    this->preferences.end();
}

void RequestHandler::GuestCounter(){
    //Serial.println("Oh, a visitor!");   
    this->preferences.begin("options");
    this->CurrentGuestCount = this->preferences.getUInt("guest_counter", 0) + 1; // Increase guest counter by one 
    this->preferences.putUInt("guest_counter", this->CurrentGuestCount);
    this->preferences.end();
    Serial.printf("We have been visited %d times ^_^\n", this->CurrentGuestCount);
}

bool boi_wifi::shouldWeEnterSafeModeWithNetworking() { //check if device is configured for SMWN or if Captive Portal is required for user to enter config still
    const OptionsStruct *Options;
    Options = this->message_handler->GetOptions();
    bool return_bool = 0;
    if((Options->SafeModeWifiName != NULL) && (Options->SafeModeWifiPassword != NULL)){
        return_bool = 1;
    }
    return return_bool;
}
