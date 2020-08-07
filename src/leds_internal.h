#ifndef _leds_internal_h
#define _leds_internal_h

#include "leds.h"

#if BOI_VERSION == 1
#define LED_LEV20_PIN 25
#define LED_LEV40_PIN 26
#define LED_LEV60_PIN 27
#define LED_LEV80_PIN 14
#define LED_LEV100_PIN 16
#define LED_S_NODE_PIN 17
#define LED_S_BATT_PIN 5
#define LED_POUT_PIN 18
#define LED_STAT_PIN 2
#define LED_STAT2_PIN 4
#elif BOI_VERSION == 2
#define LED_TL_PIN 16
#define LED_TR_PIN 27
#define LED_BL_PIN 17
#define LED_BR_PIN 26
#endif

class LEDsInternal : public LEDs
{
    public:
        LEDsInternal(LEDCallbackFunc Callback, bool DisableThread);

        void AddScript(uint8_t ID, const char *name, const uint8_t *data, uint16_t len);
        void StartScript(uint16_t ID, bool TempOverride);
        void StopScript(uint16_t ID);

        void SetGlobalVariable(uint8_t ID, double Value);
        void SetGlobalVariable(uint8_t ID, uint32_t Value);
        void SetLocalVariable(uint16_t VarID, double Value);
        void SetLocalVariable(uint16_t VarID, uint32_t Value);
        uint32_t GetLEDValue(LEDEnum LED);
        void SetLEDValue(LEDEnum LED, double Value, uint32_t LengthMS);
        void SetLEDValue(LEDEnum LED, uint32_t Value, uint32_t LengthMS);
        uint16_t GetAmbientSensor();
        const char *LEDScriptIDToStr(uint8_t ID);

#if BOI_VERSION == 1
        void SetLEDBrightness(float BrightnessPercent);
#endif
        float GetLEDBrightness();
        void SetLEDCap(uint8_t Count);

        void Run();

    private:
        typedef struct LEDInfoStruct
        {
            uint16_t Channel;
            uint32_t OverrideValue;
            int64_t OverrideTime;
            uint32_t CurrentVal;
            bool Enabled;
        } LEDInfoStruct;

        typedef struct ScriptLEDInfoStruct
        {
            uint32_t StartVal;
            uint32_t CurrentVal;
            uint32_t EndVal;
            uint16_t Time[3];              //how long the animation should take
            int64_t StartTime[3];
        } ScriptLEDInfoStruct;

        typedef struct VariableStruct
        {
            uint32_t Value;
            uint8_t Persist;
        } VariableStruct;

        typedef struct ScriptInfoStruct
        {
            uint8_t ID;
            const uint8_t *data;
            uint16_t len;
            uint16_t active_pos;
            int64_t DelayEndTime;
            uint8_t StopSet;
            bool active;
            bool TempOverride;
            VariableStruct Variable[16];
            uint16_t LEDMask;
            uint8_t Version;
            ScriptLEDInfoStruct leds[LED_Count];            
            ScriptInfoStruct *stackfirst;
            ScriptInfoStruct *stackprev;
            ScriptInfoStruct *stacknext;
            ScriptInfoStruct *next;
        } ScriptInfoStruct;

        void set_led_pin(LEDEnum led, uint8_t pin, uint8_t ledChannel);
        uint32_t *GetDestination(ScriptInfoStruct *script, uint32_t **CurValPtr, uint8_t *destdata);
        uint32_t GetValue(ScriptInfoStruct *cur_script, uint8_t Command, uint8_t destdata, uint8_t yy);
        void SetLED(ScriptInfoStruct *cur_script, uint8_t entry);
        void ReadAmbientSensor();
        void AddToStack(ScriptInfoStruct *script);
        void RemoveFromStack(ScriptInfoStruct *script);
        ScriptInfoStruct *FindScriptForLED(uint16_t LED);

        ScriptInfoStruct *scripts;
        LEDInfoStruct leds[LED_Count];
        pthread_t LEDThread;
        uint32_t GlobalVariables[16];
        LEDCallbackFunc Callback;
        uint16_t AmbientSensorValue;
        int64_t LastAmbientReading;
        uint16_t MaxBrightness;
        float MaxBrightnessPercent;
        uint8_t LEDCap;
};

#endif
