#include "boi_wifi.h"
#include "app.h"
#include "html-website-header.h"
#include "html-businesscard-header.h"
#include "html-rick-header.h"
#include "html-party-header.h"
#include "html-resume-header.h"
#include "esp_http_client.h"


boi_wifi *_globalBoiWifi;
pthread_mutex_t lock;
pthread_mutex_t lockDone;
boi_wifi::WifiModeEnum Mode;

RequestHandler::RequestHandler() {}

bool RequestHandler::canHandle(AsyncWebServerRequest *request){
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

void *static_monitor_captive_portal(void *)
{
    if(_globalBoiWifi)
        _globalBoiWifi->monitor_captive_portal();

    return 0;
}


boi_wifi::boi_wifi(boi *boiData, Messages *message_handler, WifiModeEnum NewMode)
{
    LEDHandler->StartScript(LED_ENABLE_WIFI, 1);
    this->_boi = boiData;
    this->message_handler = message_handler;
    this->ServerCheckThread = 0;
    Mode = NewMode;

    _globalBoiWifi = this;

    switch(NewMode)
    {
        case boi_wifi::BusinessCardMode:
            this->ActivateBusinessCard();
            return;

        case boi_wifi::PartyMode:
            this->ActivateParty();
            return;

        case boi_wifi::RickMode:
            this->ActivateRick();
            return;

        default:
            this->ActivateNormal();
            return;
    }
    
}

boi_wifi::~boi_wifi()
{
    this->DisableWiFi();

    //indicate wifi was off
    this->preferences.begin("options");
    this->preferences.putUChar("wifi", 0);
    this->preferences.end();
}

void boi_wifi::DisableWiFi()
{
    //kill the thread
    Serial.println("Disabling wifi");
    LEDHandler->StartScript(LED_DISABLE_WIFI, 1);

    pthread_mutex_lock(&lock);
    pthread_mutex_lock(&lockDone);
    pthread_mutex_destroy(&lock);
    pthread_mutex_destroy(&lockDone);

    Serial.println("Wifi disabled");

    //now turn off the dns and captive portal
    this->message_handler->DeregisterWebSocket();

    //remove and delete the request handler
    this->server->removeHandler(this->request_handler);

    this->dnsServer->stop();
    this->server->end();

    //delete their memory
    delete this->dnsServer;
    delete this->server;

    //wipe out the variables
    this->dnsServer = 0;
    this->server = 0;
    this->request_handler = 0;
    this->ServerCheckThread = 0;

    WiFi.enableAP(false);

    //remove our global
    _globalBoiWifi = 0;
}

void boi_wifi::monitor_captive_portal(){
    SensorDataStruct SensorData;
    int LoopCount;
    int LoopScanCount;

    pthread_mutex_lock(&lockDone);

    LoopCount = 0;
    LoopScanCount = 0;
    while(1)
    {
        if(pthread_mutex_trylock(&lock))
        {
            Serial.println("Exiting thread");
            break;
        }

        this->dnsServer->processNextRequest();
        yield();
        
        //increment our loop count
        LoopCount++;
        LoopScanCount++;
        if(LoopCount >= 5)
        {
            //read and send sensor data roughly every half a second
            //not exact due to timing and other activities on the device but this does not have
            //to be accurate
            this->_boi->get_sensor_data(&SensorData);
            yield();
            this->message_handler->HandleSensorData(&SensorData);
            yield();
            LoopCount = 0;
        }
        pthread_mutex_unlock(&lock);

        delay(100);
    };

    pthread_mutex_unlock(&lockDone);
}

void boi_wifi::monitor_smwn(){
    SensorDataStruct SensorData;
    int LoopCount;
    int LoopScanCount;

    pthread_mutex_lock(&lockDone);

    LoopCount = 0;
    LoopScanCount = 0;
    while(1)
    {
        if(pthread_mutex_trylock(&lock))
        {
            Serial.println("Exiting thread");
            break;
        }

        yield();
        
        //increment our loop count
        LoopCount++;
        LoopScanCount++;
        if(LoopCount >= 5)
        {
            //read and send sensor data roughly every half a second
            //not exact due to timing and other activities on the device but this does not have
            //to be accurate
            this->_boi->get_sensor_data(&SensorData);
            yield();
            this->message_handler->HandleSensorData(&SensorData);
            yield();
            // Monitor Itero for new messages
            

            LoopCount = 0;
        }
        pthread_mutex_unlock(&lock);

        delay(100);
    };

    pthread_mutex_unlock(&lockDone);
}

void boi_wifi::send_post_to_battery_internet(const uint8_t *message, unsigned int length){
    char* uri = "https://batteryinter.net";
    
    const char* CAcert = "-----BEGIN CERTIFICATE-----"\
"MIIFPjCCBCagAwIBAgIQJ3fuKjxlyakKAAAAAEuOjDANBgkqhkiG9w0BAQsFADBC"\
"MQswCQYDVQQGEwJVUzEeMBwGA1UEChMVR29vZ2xlIFRydXN0IFNlcnZpY2VzMRMw"\
"EQYDVQQDEwpHVFMgQ0EgMUQyMB4XDTIwMDcxODIyMzUzOFoXDTIwMTAxNjIyMzUz"\
"OFowGzEZMBcGA1UEAxMQYmF0dGVyeWludGVyLm5ldDCCASIwDQYJKoZIhvcNAQEB"\
"BQADggEPADCCAQoCggEBAOd0BL+QP5AKLJyVFaH4ldRTUQy3qiwIgnpT1Nnwj0/p"\
"kIkBYI/O7sTB3abi4QMc41zYWRDJXjwWXn52m9XUjAE1PP9ZOY/yS275f068ki5r"\
"1YMPJEzbFrzHJbqhp6QCN1S1kpf+jtlaXmvqSmqY6clzVKcCnqQdkD9xMIVwvVYa"\
"mPSiJIud01PSn5EfZOQdIrEWpgziL25n/yuzpg/xKefUvEi4CkDruvDpLEb7/bUs"\
"mB7uLoFOq3IcZ61cBbxPa4h+vuc4QnBafq3PkVQfEGs843zN+KS4THLb5rQ+1NKZ"\
"YVjDQopkJcinYnHBZRwq3z5twLt0esQuUkEEHS3z71MCAwEAAaOCAlUwggJRMA4G"\
"A1UdDwEB/wQEAwIFoDATBgNVHSUEDDAKBggrBgEFBQcDATAMBgNVHRMBAf8EAjAA"\
"MB0GA1UdDgQWBBRbbdKK49/kCSNCxGodAebhWYUl0zAfBgNVHSMEGDAWgBSx3TJd"\
"6Lc3ctLOXM4m/kd54gEI6TBkBggrBgEFBQcBAQRYMFYwJwYIKwYBBQUHMAGGG2h0"\
"dHA6Ly9vY3NwLnBraS5nb29nL2d0czFkMjArBggrBgEFBQcwAoYfaHR0cDovL3Br"\
"aS5nb29nL2dzcjIvR1RTMUQyLmNydDAbBgNVHREEFDASghBiYXR0ZXJ5aW50ZXIu"\
"bmV0MCEGA1UdIAQaMBgwCAYGZ4EMAQIBMAwGCisGAQQB1nkCBQMwLwYDVR0fBCgw"\
"JjAkoCKgIIYeaHR0cDovL2NybC5wa2kuZ29vZy9HVFMxRDIuY3JsMIIBAwYKKwYB"\
"BAHWeQIEAgSB9ASB8QDvAHYA8JWkWfIA0YJAEC0vk4iOrUv+HUfjmeHQNKawqKqO"\
"snMAAAFzZEh9wwAABAMARzBFAiAUfsKpmntR9k6B5Q6SAmYP6yE8pLsPqTByyRVZ"\
"A+GGOAIhAOAiUM2AXjkKYnjWhHNRxuOeWJLqgwprGDtbqgj3vCPWAHUAB7dcG+V9"\
"aP/xsMYdIxXHuuZXfFeUt2ruvGE6GmnTohwAAAFzZEh9ugAABAMARjBEAiAWZI0A"\
"FJPvpfSgOSByY9FVQ2nIYcrkCS4R4gLQ7TCCNwIgXV5twcQh/yUhnkNppuJBXXU6"\
"OsomiKFuIV6tEmKZCfEwDQYJKoZIhvcNAQELBQADggEBAK1p0Y8vd9sn5pGB+EpO"\
"JW7RVxvL4vNfVFb3+vHfdshvl2gXwNZW22Y9Z1gkz3S/7da23E9mWAe4JmSgwm5W"\
"/dXb0n6P2C0Ipz/K18SCaoMVPQFurYMIShZxpmuRhxR0EsNrAlvVusZqwZ7qd18s"\
"7Bqv4KOIBk39X3r5Tly7wC+FquGOZjH7eugoh06JxdU5zUuxCkSXEwf47v1JP05c"\
"JItHIeGtGHRWZ3Nn3mX/XRx4RESjB+WfxaR4IYTGgOurB2vzXec4eEFZC86ftjx9"\
"uIPNal/5Snl4vC+JIJGXfQvxWcQukTlBPHBwTPJsx5SL9nPLn8akD2+TFhQcOAbm"\
"Q+s="\
"-----END CERTIFICATE-----";
    esp_http_client_config_t config = {
        .url = uri,
        .cert_pem = CAcert,
        .method = HTTP_METHOD_POST
    };
    String data; 
    data = "[{'data'='"; 
    data += (char *)message; 
    data += "'}]";

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_post_field(client, data.c_str(),(int) data.length());

    esp_err_t err = esp_http_client_perform(client);

    Serial.println(err);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    }

    esp_http_client_cleanup(client);

}

void boi_wifi::Reconfigure(const OptionsStruct *Options)
{
    char WifiName[20 + 1 + 10];
    // uint8_t BatterySymbol[] = " \xF0\x9F\x94\x8B ";
    uint8_t BatterySymbol[] = " \xF0\x9F\x8E\x83 "; // SPOOKY EDITION OOOOOOOOOoOOOoOooooooooo.............

    memcpy(WifiName, &BatterySymbol[1], 5);

    //reconfigure the wifi with batteries around it
    memcpy(&WifiName[5], Options->WifiName, strlen(Options->WifiName));
    memcpy(&WifiName[5 + strlen(Options->WifiName)], BatterySymbol, 5);
    WifiName[10 + strlen(Options->WifiName)] = 0;
    Serial.printf("Wifi portal name: %s\n", WifiName);

    if(!WiFi.softAP(WifiName, Options->WifiPassword))
        Serial.print("Error calling WiFi.softAP\n");
}

void boi_wifi::setup_captive_portal(const OptionsStruct *Options){
    const byte        DNS_PORT = 53;          // Capture DNS requests on port 53
    IPAddress         apIP(1, 1, 1, 1);    // Private network for server

    if(this->ServerCheckThread)
    {
        this->DisableWiFi();
        delay(1000);
        yield();
    }

    if(!WiFi.mode(WIFI_AP_STA))
        Serial.print("Error setting WiFi mode\n");

    this->Reconfigure(Options);

    if(!WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0)))
        Serial.print("Error calling WiFi.softAPConfig\n");

    // if DNSServer is started with "*" for domain name, it will reply with
    // provided IP to all DNS request
    this->dnsServer = new DNSServer();
    this->dnsServer->start(DNS_PORT, "*", apIP);

    // reply to all requests with same HTML
    this->server = new AsyncWebServer(80);
    this->request_handler = new RequestHandler();
    this->server->addHandler(this->request_handler);

    //setup the web socket
    this->message_handler->RegisterWebsocket();

    //start the server
    this->server->begin();
    Serial.print("Successfully began broadcasting legacy-wifi InternetOfBatteries!!!\n\n");

    pthread_mutex_init(&lock, NULL);
    pthread_mutex_init(&lockDone, NULL);

    //setup our thread that will re-send messages that haven't been ack'data
    if(pthread_create(&this->ServerCheckThread, NULL, static_monitor_captive_portal, 0))
    {
        //failed
        Serial.println("Failed to setup ServerCheck thread");
        _globalBoiWifi = 0;
    }
}

void boi_wifi::enter_safe_mode_with_networking(const OptionsStruct *Options){
    //3 button presses to enter safe_mode_with_networking
    // Connection code to connect to an AP
    // No DNS
    // Initialize the webserver still
    // Initialize the mesh network

    if(this->ServerCheckThread)
    {
        this->DisableWiFi();
        delay(1000);
        yield();
    }

    // Set up Wifi Connection Here
    // Fetch local network name from options
    // Fetch local wifi password from options
    WiFi.begin("Asgard","notaplace!");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("Connecting to WiFi Safe Mode with Networking...");
    }
 
    Serial.println("Connected to the WiFi for Safe Mode with Networking");

    // reply to all requests with same HTML
    this->server = new AsyncWebServer(80);
    this->request_handler = new RequestHandler();
    this->server->addHandler(this->request_handler);

    //setup the web socket
    this->message_handler->RegisterWebsocket();

    //start the server
    this->server->begin();
    Serial.print("Successfully began broadcasting legacy-wifi InternetOfBatteries!!!\n\n");

    pthread_mutex_init(&lock, NULL);
    pthread_mutex_init(&lockDone, NULL);

    //setup our thread that will re-send messages that haven't been ack'data
    if(pthread_create(&this->ServerCheckThread, NULL, static_monitor_captive_portal, 0))
    {
        //failed
        Serial.println("Failed to setup ServerCheck thread");
        _globalBoiWifi = 0;
    }

    // Post out to batteryinter.net with our local IP address


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