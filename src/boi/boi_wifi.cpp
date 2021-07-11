#include "boi_wifi.h"
#include "app.h"
#include "WiFi.h"

boi_wifi *_globalBoiWifi;
pthread_mutex_t lock;
pthread_mutex_t lockDone;

void *static_monitor_captive_portal(void *) {
    if(_globalBoiWifi) {
        _globalBoiWifi->monitor_captive_portal();
    }
    return 0;
}

void *static_monitor_smwn(void *) {
    if(_globalBoiWifi) {
        _globalBoiWifi->monitor_smwn();
    }
    return 0;
}

boi_wifi::boi_wifi(boi *boiData, Messages *message_handler, WifiModeEnum NewMode) {
    LEDHandler->StartScript(LED_ENABLE_WIFI, 1);
    this->_boi = boiData;
    this->message_handler = message_handler;
    this->ServerCheckThread = 0;
    this->dnsServer = 0;
    Mode = NewMode;
    _globalBoiWifi = this;

    switch(NewMode) {
        case boi_wifi::BusinessCardMode:
            this->ActivateBusinessCard();
            return;
        case boi_wifi::PartyMode:
            this->ActivateParty();
            return;
        case boi_wifi::RickMode:
            this->ActivateRick();
            return;
        case boi_wifi::SafeModeWithNetworking:
            this->ActivateSafeModeWithNetworking();
            return;
        default:
            this->ActivateParty();
            return;
    }
    
}

boi_wifi::~boi_wifi() {
    this->DisableWiFi();
    //indicate wifi was off
    this->preferences.begin("options");
    this->preferences.putUChar("wifi", 0);
    this->preferences.end();
}

void boi_wifi::DisableWiFi() { //kill the thread
    Serial.println("Disabling wifi");
    LEDHandler->StartScript(LED_DISABLE_WEEFEE, 1);
    pthread_mutex_lock(&lock);
    pthread_mutex_lock(&lockDone);
    pthread_mutex_destroy(&lock);
    pthread_mutex_destroy(&lockDone);
    Serial.println("Wifi disabled");

    if(this->message_handler) {
        this->message_handler->DeregisterWebSocket(); //turn off the DNS and Captive Portal
    }

    this->DeleteWebServer(); //remove and delete the request handler

    if(this->dnsServer) {
        this->dnsServer->stop();
        delete this->dnsServer; //delete the dns memory
    }

        //wipe out the variables from mem
    this->dnsServer = 0;
    this->server = 0;
    this->request_handler = 0;
    this->ServerCheckThread = 0;

    WiFi.enableAP(false);
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true, true);
}

void boi_wifi::monitor_captive_portal() { //contains refresh/sample clock cycles for monitoring sensors on board & ultimately their display on the web portal
    SensorDataStruct SensorData;
    int LoopCount;
    int LoopScanCount;
    pthread_mutex_lock(&lockDone);
    LoopCount = 0;
    LoopScanCount = 0;
    while(1) { // run to infinity and beyond
        if(pthread_mutex_trylock(&lock)) {
            Serial.println("Exiting thread");
            break;
        }
        if(this->dnsServer) {
            this->dnsServer->processNextRequest();
        }
        yield();
        LoopCount++; //increment our loop count
        LoopScanCount++;
        if(LoopCount >= 5) { //causes delay in this while to not be exact due to timing and other activities on the device, but this does not have to be accurate
            this->_boi->get_sensor_data(&SensorData);
            yield();
            this->message_handler->HandleSensorData(&SensorData);
            yield();
            LoopCount = 0; //time to reset & go again
        }
        pthread_mutex_unlock(&lock);
        yield();
        delay(100); //read and send sensor data every X milliseconds
    };
    pthread_mutex_unlock(&lockDone);
}

void boi_wifi::monitor_smwn() { // contains clock cycle for message board pulls
    SensorDataStruct SensorData;
    int LoopCount;
    int LoopScanCount;
    uint64_t NextGetCheck;
    pthread_mutex_lock(&lockDone);
    LoopCount = 0;
    LoopScanCount = 0;
    NextGetCheck = esp_timer_get_time() + (10*1000*1000);
    while(1) {
        if(pthread_mutex_trylock(&lock)) {
            Serial.println("Exiting thread");
            break;
        }
        LoopCount++; //increment our loop count
        LoopScanCount++;
        if(LoopCount >= 5) { //causes delay in this while to not be exact due to timing and other activities on the device, but this does not have to be accurate
            this->_boi->get_sensor_data(&SensorData);
            yield();
            this->message_handler->HandleSensorData(&SensorData);
            yield();
            LoopCount = 0;
        }
        if(esp_timer_get_time() >= NextGetCheck) { //process messages, if we had messages then check every 10 seconds otherwise every minute
            if(this->message_handler->QueryBatteryInternet()) {
                NextGetCheck = esp_timer_get_time() + (10*1000*1000);
            }
            else {
                NextGetCheck = esp_timer_get_time() + (60*1000*1000);
            }
        }
        pthread_mutex_unlock(&lock);
        yield();
        delay(100); //read and send sensor data every X milliseconds
    };
    pthread_mutex_unlock(&lockDone);
}

void boi_wifi::Reconfigure(const OptionsStruct *Options) {
    char WifiName[20 + 1 + 10];
    // uint8_t BatterySymbol[] = " \xF0\x9F\x94\x8B "; // Standard edition, but who wants that?
    // uint8_t BatterySymbol[] = " \xF0\x9F\x8E\x83 "; // SPOOKY EDITION OOOOOOOOOoOOOoOooooooooo.............
    uint8_t BatterySymbol[] = " \xF0\x9F\x8E\x83 \xF0\x9F\x98\xB7 \xF0\x9F\x8E\x83 "; // SAFE MODE WITH NETWORKING EDITION - WASH YOUR HANDS!!!!!
    memcpy(WifiName, &BatterySymbol[1], 5);

        //reconfigure the wifi with batteries around it
    memcpy(&WifiName[5], Options->WifiName, strlen(Options->WifiName));
    memcpy(&WifiName[5 + strlen(Options->WifiName)], BatterySymbol, 5);
    WifiName[10 + strlen(Options->WifiName)] = 0;
    Serial.printf("Wifi portal name: %s\n", WifiName);

    if(!WiFi.softAP(WifiName, Options->WifiPassword)) {
        Serial.print("Error calling WiFi.softAP\n");
    }
}

void boi_wifi::setup_captive_portal(const OptionsStruct *Options) {
    const byte DNS_PORT = 53; // Capture DNS requests on port 53
    // TODO: Update to internal ip address
    IPAddress ip_address = WiFi.localIP(); //captive portal uses self ip

    //IPAddress apIP(ip_address[0], ip_address[1], ip_address[2], ip_address[3]); // Private network for server
    IPAddress apIP(1,1,1,1); // Private network for server

    if(this->ServerCheckThread) {
        this->DisableWiFi();
        yield();
        delay(1000);
    }
    
    if(!WiFi.mode(WIFI_AP_STA)) {
        Serial.print("Error setting WiFi mode\n");
    }

    this->Reconfigure(Options);

    if(!WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0))) {
        Serial.print("Error calling WiFi.softAPConfig\n");
    }

    this->dnsServer = new DNSServer();
    this->dnsServer->start(DNS_PORT, "*", apIP); // if DNSServer is started with "*" for domain name, it will reply with provided IP to all DNS request

    // reply to all requests with same HTML - TODO is this happening somewhere, or should this be deleted?
    this->SetupRequestServer();
    this->message_handler->RegisterWebsocket(); //setup the web socket

    pthread_mutex_init(&lock, NULL);
    pthread_mutex_init(&lockDone, NULL);

    //setup our thread that will re-send messages that haven't been ack'data - TODO is this happening? looks like we just have a failure handle clearing an obj
    if(pthread_create(&this->ServerCheckThread, NULL, static_monitor_captive_portal, 0)) {
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
    
    LEDHandler->StopScript(LED_ALL);
    LEDHandler->StartScript(LED_ENABLE_SMWN, 1);

    if(this->ServerCheckThread) {
        this->DisableWiFi();
        yield();
        delay(1000);
    }

    //esp_wifi_set_promiscuous(false);

    // Set up Wifi Connection Here
    // Fetch local network name from options
    // Fetch local wifi password from options
    WiFi.disconnect();
    yield();
    delay(100);

    WiFi.enableSTA(true);
    if(!WiFi.mode(WIFI_STA)) {
        Serial.println("Failed to set wifi mode");
    }

    yield();
    delay(100);
    wl_status_t status;
    status = WiFi.begin(Options->SafeModeWifiName,Options->SafeModeWifiPassword);
    int counter = 0;

        //esp32 appears to have a serious issue about connecting first time around, we force an immediate 2nd cycle hence the begin above
        //then not incrementing counter until the end
    while (status != WL_CONNECTED) {
        delay(100);
        status = WiFi.status();
        if(status == WL_CONNECTED) {
            break;
        }
        //if 60 seconds then then activate AP - TODO what's the 60 seconds? not seeing that being tracked anywhere here
        if(counter > 300) {
            Serial.printf("Unable to connect to WiFi Safe Mode with Networking :(... %d\n", WiFi.status());
            this->ActivateNormal();
            LEDHandler->StopScript(LED_ENABLE_SMWN);
            return;
        }
        else if((counter % 100) == 0) {
            Serial.printf("Connecting to WiFi Safe Mode with Networking... %d\n", WiFi.status());
            WiFi.disconnect();
            yield();
            delay(100);
            WiFi.mode(WIFI_STA);
            yield();
            delay(100);
            status = WiFi.begin(Options->SafeModeWifiName,Options->SafeModeWifiPassword);
        }
        counter++;
    }

    Serial.println("Connected to the Local Network for WiFi Safe Mode with Networking ^_^");
    Serial.print("Local IP: ");
    Serial.println(WiFi.localIP().toString());

    // reply to all requests with same HTML - TODO is this happening somewhere, or should this be deleted? (same as above)
    yield();
    this->SetupRequestServer();

    yield();
    this->message_handler->RegisterWebsocket(); //setup the web socket

    pthread_mutex_init(&lock, NULL);
    pthread_mutex_init(&lockDone, NULL);

    pthread_attr_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    pthread_attr_setstacksize(&cfg, 6*1024);

    //setup our thread that will re-send messages that haven't been ack'data - TODO is this happening? looks like we just have a failure handle clearing an obj (same as above)
    if(pthread_create(&this->ServerCheckThread, &cfg, static_monitor_smwn, 0)) {
        //failed
        Serial.println("Failed to setup ServerCheck thread");
        _globalBoiWifi = 0;
    }
    LEDHandler->StopScript(LED_ENABLE_SMWN);
    LEDHandler->StartScript(LED_SMWN_ACTIVE, 1);
    do_battery_checkin();
}
