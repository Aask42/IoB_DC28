#ifndef __leds_h
#define __leds_h

#include <Arduino.h>
#include "leds-data.h"

#if BOI_VERSION == 1
typedef enum LEDEnum
{
    LED_LEV20,
    LED_LEV40,
    LED_LEV60,
    LED_LEV80,
    LED_LEV100,
    LED_S_NODE,
    LED_S_BATT,
    LED_STAT,
    LED_POUT,
    LED_STAT2,
    LED_Count
} LEDEnum;

#define LED_Count_Battery 5

#elif BOI_VERSION == 2
typedef enum LEDEnum
{
    LED_0,
    LED_1,
    LED_2,
    LED_3,
    LED_4,
    LED_5,
    LED_6,
    LED_7,
    LED_8,
    LED_TL,
    LED_TR,
    LED_BL,
    LED_BR,
    LED_Count
} LEDEnum;

#define LED_Count_Battery 9

#define LED_LEV100 LED_0
#define LED_LEV80 LED_1
#define LED_LEV60 LED_2
#define LED_LEV40 LED_3
#define LED_LEV20 LED_4
#define LED_S_NODE LED_TL
#define LED_S_BATT LED_TR
#define LED_POUT LED_BL
#endif

#define LED_OVERRIDE_INFINITE 0xffffffff

//callback function indicating a script hit the end. If Finished is true then the script
//is done running otherwise it is looping
typedef void (*LEDCallbackFunc)(const uint16_t ID, bool Finished);

class LEDs
{
    public:
        virtual void AddScript(uint8_t ID, const char *name, const uint8_t *data, uint16_t len);
        virtual void StartScript(uint16_t ID, bool TempOverride);
        virtual void StopScript(uint16_t ID);
        virtual void SetGlobalVariable(uint8_t ID, double Value);
        virtual void SetGlobalVariable(uint8_t ID, uint32_t Value);
        virtual void SetLocalVariable(uint16_t VarID, double Value);
        virtual void SetLocalVariable(uint16_t VarID, uint32_t Value);
        virtual uint32_t GetLEDValue(LEDEnum LED);
        virtual void SetLEDValue(LEDEnum LED, double Value, uint32_t LengthMS);
        virtual void SetLEDValue(LEDEnum LED, uint32_t Value, uint32_t LengthMS);
        virtual uint16_t GetAmbientSensor();;
        virtual const char *LEDScriptIDToStr(uint8_t ID);

        virtual void SetLEDBrightness(float BrightnessPercent);
        virtual float GetLEDBrightness();
        virtual void SetLEDCap(uint8_t Count);
};

//LED initializer
LEDs *NewLEDs(LEDCallbackFunc Callback, bool DisableThread);

#endif