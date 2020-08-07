/*
 * app.h
 *
*/
// Don't import yourself twice
#ifndef __app_h__
#define __app_h__

#if BOI_VERSION == 1
#elif BOI_VERSION == 2
#else
#error Unknown BOI_VERSION, please correct all code appropriately
#endif

#include <Arduino.h>
#include "boi/boi.h" // Class library
#include "boi/boi_wifi.h"
#include "leds.h"

class boi;
class boi_wifi;
class LEDs;
class Messages;
extern boi_wifi *BatteryWifi;
extern LEDs *LEDHandler;
extern Messages *MessageHandler;

#if BOI_VERSION == 2
#include "spi_parser.h"
class SPIParser;
extern SPIParser *SPIHandler;
extern SPIParser::SPIDataStruct SPIData;
#endif

#endif