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

bool RequestHandler::canHandle(AsyncWebServerRequest *request){
    
    // TODO: Update to pull the IP address from preferences
    
    if(request->host() == "1.1.1.1")
    {
        request->addInterestingHeader("Cache-Control: no-cache, no-store, must-revalidate");
        request->addInterestingHeader("Pragma: no-cache");
        request->addInterestingHeader("Expires: -1");
    }
    return true;
}

void RequestHandler::handleRequest(AsyncWebServerRequest *request) {
    Serial.printf("host: %s, url: %s\n", request->host().c_str(), request->url().c_str());

    if((request->url() == "/whoami"))
    {
        request->send(200, "text/html", HTMLResumeData);
        
    }else if((request->url() == "/GuestCounter"))
    {   
        this->GuestCounter();

        request->send(200, "text/html", String(this->CurrentGuestCount));
    }else{
        //AsyncResponseStream *response = request->beginResponseStream("text/html");
        switch(Mode)
        {
            case boi_wifi::NormalMode:
                request->send(200, "text/html", HTMLWebsiteData);
                break;

            case boi_wifi::PartyMode:
                request->send(200, "text/html", HTMLPartyData);
                break;

            case boi_wifi::BusinessCardMode:
                request->send(200, "text/html", HTMLBusinessCardData);
                break;

            default:
                request->send(200, "text/html", HTMLRickData);
                break;
        }
    }
}

void boi_wifi::SetupRequestServer()
{
    this->server = new AsyncWebServer(80);
    this->request_handler = new RequestHandler();
    this->server->addHandler(this->request_handler);

    //start the server
    this->server->begin();
    Serial.print("Successfully began broadcasting wifi InternetOfBatteries!!!\n\n");
}

void boi_wifi::DeleteWebServer()
{
    //remove and delete the request handler
    this->server->removeHandler(this->request_handler);
    this->server->end();

    //delete the memory
    delete this->server;
}

void boi_wifi::ActivateBusinessCard()
{
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

void boi_wifi::ActivateRick()
{
    const OptionsStruct *Options;
    OptionsStruct NewOptions;
    Mode = RickMode;
    Options = this->message_handler->GetOptions();
    memcpy(&NewOptions, Options, sizeof(OptionsStruct));
    memcpy(NewOptions.WifiName, Options->OriginalWifiName, sizeof(Options->OriginalWifiName));
    this->setup_captive_portal(&NewOptions);

    //indicate wifi was rick
    this->preferences.begin("options");
    this->preferences.putUChar("wifi", boi_wifi::RickMode);
    this->preferences.end();
}

void boi_wifi::ActivateParty()
{
    const OptionsStruct *Options;
    OptionsStruct NewOptions;
    Mode = PartyMode;
    Options = this->message_handler->GetOptions();
    memcpy(&NewOptions, Options, sizeof(OptionsStruct));
    memcpy(NewOptions.WifiName, Options->OriginalWifiName, sizeof(Options->OriginalWifiName));
    this->setup_captive_portal(&NewOptions);

    //indicate wifi was party
    this->preferences.begin("options");
    this->preferences.putUChar("wifi", boi_wifi::PartyMode);
    this->preferences.end();
}

void boi_wifi::ActivateNormal()
{
    const OptionsStruct *Options;
    Mode = NormalMode;
    Options = this->message_handler->GetOptions();
    this->setup_captive_portal(Options);

    //indicate wifi was normal
    this->preferences.begin("options");
    this->preferences.putUChar("wifi", boi_wifi::NormalMode);
    this->preferences.end();
}
void boi_wifi::ActivateSafeModeWithNetworking()
{
    const OptionsStruct *Options;
    Mode = SafeModeWithNetworking;
    Options = this->message_handler->GetOptions();

    this->enter_safe_mode_with_networking(Options);

    //indicate wifi was normal
    this->preferences.begin("options");
    this->preferences.putUChar("wifi", boi_wifi::SafeModeWithNetworking);
    this->preferences.end();
}

void RequestHandler::GuestCounter()
{
    Serial.println("Oh, a visitor!");   
    // Increase guest counter by one 
    this->preferences.begin("options");
    this->CurrentGuestCount = this->preferences.getUInt("guest_counter", 0) + 1;
    this->preferences.putUInt("guest_counter", this->CurrentGuestCount);
    this->preferences.end();
    Serial.printf("We have been visited %d times ^_^\n", this->CurrentGuestCount);
}