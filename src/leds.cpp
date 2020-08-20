#include "leds_internal.h"
#include <Preferences.h>
#include "app.h"

const char *LEDScriptNames[LED_TOTAL_SCRIPT_COUNT];

LEDsInternal *_globalLEDs;
pthread_mutex_t led_lock;

#define FREQUENCY 5000
#define BIT_RESOLUTION 15
#define MAX_RESOLUTION (1 << BIT_RESOLUTION)
#define POWER_LAW_A 0.4

LEDs *NewLEDs(LEDCallbackFunc Callback, bool DisableThread)
{
    LEDsInternal *LED = new LEDsInternal(Callback, DisableThread);
    return LED;
}
void *static_run_leds(void *)
{
    if(_globalLEDs)
        _globalLEDs->Run();

    return 0;
}

LEDsInternal::LEDsInternal(LEDCallbackFunc Callback, bool DisableThread)
{
    ScriptInfoStruct *script;

    //initialize
    this->scripts = 0;
    this->Callback = Callback;

    memset(this->leds, 0, sizeof(this->leds));

    // LED assignment of pin and ledChannel
#if BOI_VERSION == 1
    this->set_led_pin(LED_LEV20, LED_LEV20_PIN, 0);
    this->set_led_pin(LED_LEV40, LED_LEV40_PIN, 1);
    this->set_led_pin(LED_LEV60, LED_LEV60_PIN, 2);
    this->set_led_pin(LED_LEV80, LED_LEV80_PIN, 3);
    this->set_led_pin(LED_LEV100, LED_LEV100_PIN, 4);
    this->set_led_pin(LED_S_NODE, LED_S_NODE_PIN, 5);
    this->set_led_pin(LED_S_BATT, LED_S_BATT_PIN, 6);
    this->set_led_pin(LED_POUT, LED_POUT_PIN, 7);
    this->set_led_pin(LED_STAT, LED_STAT_PIN, 8);
    this->set_led_pin(LED_STAT2, LED_STAT2_PIN, 9);
#elif BOI_VERSION == 2
    for(int i = 0; i < LED_Count; i++)
        this->leds[i].Enabled = 1;

    this->set_led_pin(LED_TL, LED_TL_PIN, 0);
    this->set_led_pin(LED_TR, LED_TR_PIN, 1);
    this->set_led_pin(LED_BL, LED_BL_PIN, 2);
    this->set_led_pin(LED_BR, LED_BR_PIN, 3);
#endif

    //allocate a default led setup
    script = (ScriptInfoStruct *)malloc(sizeof(ScriptInfoStruct));
    memset(script, 0, sizeof(ScriptInfoStruct));

#if BOI_VERSION == 1
    //setup LED_STAT2 pin so that we can use STAT properly
    ledcWrite(this->leds[LED_STAT2].Channel, MAX_RESOLUTION); // - 1000);
#endif

    //set a default ID and assign to the script list
    script->ID = 0xff;
    this->scripts = script;

#if BOI_VERSION == 1
    //now set the brightness default, this will force STAT and STAT2 to be updated too
    script->active = 1;
    this->SetLEDBrightness(0.09);
    script->active = 0;
#endif

    //wipe out the global variables
    memset(this->GlobalVariables, 0, sizeof(this->GlobalVariables));
    this->AmbientSensorValue = 0.0;
    this->LastAmbientReading = 0;

    _globalLEDs = this;

    //if we are to disable threads then exit
    if(DisableThread)
        return;

    //setup our thread that will run the LEDs themselves
    pthread_mutex_init(&led_lock, NULL);
    if(pthread_create(&this->LEDThread, NULL, static_run_leds, 0))
    {
        //failed
        Serial.println("Failed to setup LED thread");
        _globalLEDs = 0;
        return;
    }
}

void LEDsInternal::set_led_pin(LEDEnum led, uint8_t pin, uint8_t ledChannel)
{
    this->leds[led].Channel = ledChannel;

    pinMode(pin, OUTPUT);
    ledcSetup(this->leds[led].Channel, FREQUENCY, BIT_RESOLUTION);
    ledcAttachPin(pin, this->leds[led].Channel);
    this->leds[led].Enabled = 1;

#if BOI_VERSION == 1
    //turn the LED off, MAX results in full off, 0 is full bright
    ledcWrite(this->leds[led].Channel, MAX_RESOLUTION);
#elif BOI_VERSION == 2
    //for an unknown reason the new badge has 0 as off and MAX is full bright
    ledcWrite(this->leds[led].Channel, 0);
#endif
}

uint32_t LEDsInternal::GetValue(ScriptInfoStruct *cur_script, uint8_t Command, uint8_t destdata, uint8_t yy)
{
    uint8_t data;
    uint8_t zz;
    uint32_t output;
    uint8_t IsShiftRotate;
    uint8_t yy2;

    //get zz from the previous byte before moving forward
    zz = destdata >> 6;
    yy2 = 0;
    output = 0;

    //determine if this is a shift or rotate command
    IsShiftRotate = 0;
    if((Command & 0xc0) == 0)
        IsShiftRotate = ((Command & 0xf) >= 0xb);

    switch(zz)
    {
        case 0:
            //fixed value
            if(yy == 5)
            {
                //magic flag indicating that we have 2 bytes following
                output = *(uint16_t *)(&cur_script->data[cur_script->active_pos]);
                cur_script->active_pos+=2;
                output &= 0x0000ffff;
                yy = 0;
                break;
            }
            else if(yy || IsShiftRotate)
            {
                //if yy or the command is one of the shl/shr/rol/rol then a single byte is used
                output = cur_script->data[cur_script->active_pos];
                cur_script->active_pos++;
                break;
            }
            else
            {
                output = *(uint32_t *)(&cur_script->data[cur_script->active_pos]);
                cur_script->active_pos+=3;
                output &= 0x00ffffff;
                break;
            }; 
               
        case 1:
            data = cur_script->data[cur_script->active_pos];
            cur_script->active_pos++;

            //LED or potentially random, check ww
            switch(data >> 6)
            {
                case 0:
                    //led value
                    output = cur_script->leds[data & 0xf].EndVal;
                    yy2 = (data & 0x30) >> 4;
                    break;

                case 1:
                    //destination led value
                    output = cur_script->leds[destdata & 0xf].CurrentVal;
                    yy2 = (data & 0x30) >> 4;
                    break;

                case 2:
                    //actual current led value
                    output = this->leds[destdata & 0xf].CurrentVal;
                    yy2 = (data & 0x30) >> 4;
                    break;

                case 3:
                {
                    uint32_t Rand[2];
                    uint8_t i;
                    uint8_t tempdata = data;

                    //random, determine what random combo
                    for(i = 0; i < 2; i++, tempdata <<= 2)
                    {
                        switch((tempdata >> 2) & 0x3)
                        {
                            case 0:
                                if(yy && (yy != 5))
                                {
                                    Rand[i] = cur_script->data[cur_script->active_pos];
                                    cur_script->active_pos++;
                                }
                                else
                                {
                                    Rand[i] = *(uint32_t *)(&cur_script->data[cur_script->active_pos]);
                                    Rand[i] &= 0x00ffffff;
                                    cur_script->active_pos += 3;
                                }
                                break;

                            case 1:
                            case 2:
                            case 3:
                                yy2 = (data & 0x30) >> 4;
                                Rand[i] = this->GetValue(cur_script, Command, (((tempdata >> 2) & 0x3) << 6), yy2);
                                break;
                        }
                    }

                    //both random values should be filled in now, pick something between them
                    //get a random value between val1 and val2 inclusive
                    uint32_t Range;
                    uint32_t RandVal;
                    Range = Rand[1] - Rand[0] + 1;
                    if(Range)
                    {
                        RandVal = esp_random();
                        output = (RandVal % Range) + Rand[0];
                    }

                    //make sure output is not shifted
                    yy2 = 0;
                }
            }
            break;

        case 2:
            data = cur_script->data[cur_script->active_pos];
            cur_script->active_pos++;

            //global variable
            output = this->GlobalVariables[data & 0xf];
            yy2 = (data & 0x30) >> 4;
            break;

        case 3:
            data = cur_script->data[cur_script->active_pos];
            cur_script->active_pos++;

            //local variable
            output = cur_script->Variable[data & 0xf].Value;
            yy2 = (data & 0x30) >> 4;
            break;
    };

    //if the input value has a yy2 set then adjust the output byte value to align properly
    if(yy2)
        output = (output >> (24-(yy2*8))) & 0xff;

    //shift output based on yy or command type to align with the final destination
    if(IsShiftRotate)
        output = output & 0xff;
    else if(yy && (yy != 5))
        output = (output & 0xff) << (24 - (yy * 8)); //shift the byte into location
    else
        output &= 0x00ffffff;

    return output;
}

uint32_t *LEDsInternal::GetDestination(ScriptInfoStruct *script, uint32_t **CurValPtr, uint8_t *destdata)
{
    //get the next byte and parse it up for destination
    *destdata = script->data[script->active_pos];
    script->active_pos++;
    uint8_t ID = *destdata & 0xf;

    switch((*destdata & 0x30) >> 4)
    {
        case 0:
            //led
            *CurValPtr = &script->leds[ID].CurrentVal;
            return &script->leds[ID].EndVal;

        case 1:
            //global variable
            *CurValPtr = &(this->GlobalVariables[ID]);
            return &(this->GlobalVariables[ID]);

        case 2:
            //local variable
            *CurValPtr = &(script->Variable[ID].Value);
            return &(script->Variable[ID].Value);
    }

    return 0;
}

void LEDsInternal::Run()
{
    uint8_t Command;
    uint8_t LogicSeen;
    uint8_t i;
    uint16_t TempPos;
    uint16_t LEDsUpdated;
    uint32_t *DestPtr;
    uint32_t *CurValPtr;
    uint32_t Value;
    uint32_t Value2;
    uint32_t Mask;
    uint32_t DestMask;
    uint8_t ID;
    uint8_t DestData;
    uint8_t yy;
    uint8_t yy2;

    ScriptInfoStruct *cur_script;

    Serial.println("LED thread running\n");
    while(1)
    {
        //lock the mutex so that we avoid anyone modifying the data while we need it
        pthread_mutex_lock(&led_lock);

        //execute each script that is active
        cur_script = this->scripts;
        if(!cur_script)
        {
            //not done setting scripts up
            pthread_mutex_unlock(&led_lock);
            yield();
            delay(500);
            continue;
        }

        while(cur_script)
        {
            //if not active skip it
            if(!cur_script->active)
            {
                cur_script = cur_script->next;
                continue;
            }

            //execute all instructions until we hit an instruction to wait on
            //if we are not currently delayed
            LogicSeen = 0;
            LEDsUpdated = 0;
            while(!cur_script->DelayEndTime && !LogicSeen)
            {
                Command = cur_script->data[cur_script->active_pos];
                cur_script->active_pos++;


                //top bit indicates math or other command
                if(!(Command & 0x80))
                {
                    //math operations

                    //get all of our fields

                    //determine the math operation to perform
                    yy = (Command & 0x30) >> 4;
                    DestPtr = this->GetDestination(cur_script, &CurValPtr, &DestData);
                    ID = DestData & 0xf;
                    if(!((DestData & 0x30) >> 4))
                    {
                        LEDsUpdated |= (1 << ID);

                        //doing a math operation, set start to end as the result will be the start value
                        //and all math works on what the final value would be
                        cur_script->leds[ID].StartVal = cur_script->leds[ID].EndVal;

                        //reset the times according to what destination we are modifying if a led
                        if(yy)
                            cur_script->leds[ID].Time[yy - 1] = 0;
                        else
                        {
                            cur_script->leds[ID].Time[0] = 0;
                            cur_script->leds[ID].Time[1] = 0;
                            cur_script->leds[ID].Time[2] = 0;
                        }
                    }

                    //if "not" then ignore value
                    if((Command & 0xf) != 0xa)
                        Value = this->GetValue(cur_script, Command & 0xf, DestData, yy);
                    else
                        Value = *CurValPtr;

                    //determine the masking based on dest
                    if(yy)
                        Mask = 0xff << (24 - (yy*8));
                    else
                        Mask = 0x00ffffff;


                    //generate the destination mask
                    DestMask = 0xffffff & ~Mask;

                    //look up the math operation to perform
                    switch(Command & 0xf)
                    {
                        case 0x0:   //set
                            *DestPtr = (*DestPtr & DestMask) | (Value & Mask);
                            break;

                        case 0x1:   //move
                            DestData = cur_script->data[cur_script->active_pos];
                            cur_script->active_pos++;
                            Value2 = this->GetValue(cur_script, Command & 0xf, DestData, yy);

                            //go alter the LED info
                            cur_script->leds[ID].StartVal = (cur_script->leds[ID].StartVal & DestMask) | (Value & Mask);
                            cur_script->leds[ID].EndVal = (cur_script->leds[ID].EndVal & DestMask) | (Value2 & Mask);
                            DestData = cur_script->data[cur_script->active_pos];
                            cur_script->active_pos++;
                            if(yy)
                            {
                                cur_script->leds[ID].Time[yy-1] = this->GetValue(cur_script, Command & 0xf, DestData, 5);
                                cur_script->leds[ID].StartTime[yy-1] = esp_timer_get_time() / 1000;
                            }
                            else
                            {
                                cur_script->leds[ID].Time[0] = this->GetValue(cur_script, Command & 0xf, DestData, 5);
                                cur_script->leds[ID].Time[1] = cur_script->leds[ID].Time[0];
                                cur_script->leds[ID].Time[2] = cur_script->leds[ID].Time[0];
                                cur_script->leds[ID].StartTime[0] = esp_timer_get_time() / 1000;
                                cur_script->leds[ID].StartTime[1] = cur_script->leds[ID].StartTime[0];
                                cur_script->leds[ID].StartTime[2] = cur_script->leds[ID].StartTime[0];
                            }

                            break;

                        case 0x2:   //add
                            if(cur_script->Version == 1)
                            {
                                float a = (float)(((int)(*CurValPtr << 8)) >> 8) / 100.0;
                                float b = (float)(((int)(Value << 8)) >> 8) / 100.0;
                                *DestPtr = (int)((a + b) * 100.0) & 0xffffff;
                            }
                            else
                                *DestPtr = (*DestPtr & DestMask) | (((*CurValPtr & Mask) + Value) & Mask);
                            break;

                        case 0x3:   //sub
                            if(cur_script->Version == 1)
                            {
                                float a = (float)(((int)(*CurValPtr << 8)) >> 8) / 100.0;
                                float b = (float)(((int)(Value << 8)) >> 8) / 100.0;
                                *DestPtr = (int)((a - b) * 100.0) & 0xffffff;
                            }
                            else
                                *DestPtr = (*DestPtr & DestMask) | (((*CurValPtr & Mask) - Value) & Mask);
                            break;

                        case 0x4:   //mul
                            *DestPtr = (*DestPtr & DestMask) | (((*CurValPtr & Mask) * Value) & Mask);
                            break;

                        case 0x5:   //div
                            if(Value == 0)
                                *DestPtr = 0;
                            else
                                *DestPtr = (*DestPtr & DestMask) | (((*CurValPtr & Mask) / Value) & Mask);
                            break;

                        case 0x6:   //mod
                            if(Value == 0)
                                *DestPtr = 0;
                            else
                                *DestPtr = (*DestPtr & DestMask) | (((*CurValPtr & Mask) % Value) & Mask);
                            break;

                        case 0x7:   //and
                            *DestPtr = (*DestPtr & DestMask) | (((*CurValPtr & Mask) & Value) & Mask);
                            break;

                        case 0x8:   //or
                            *DestPtr = (*DestPtr & DestMask) | (((*CurValPtr | Mask) & Value) & Mask);
                            break;

                        case 0x9:   //xor
                            *DestPtr = (*DestPtr & DestMask) | (((*CurValPtr ^ Mask) & Value) & Mask);
                            break;

                        case 0xa:   //not
                            *DestPtr = (*DestPtr & DestMask) | ((!(*CurValPtr & Mask)) & Mask);
                            break;

                        case 0xb:   //shl
                            *DestPtr = (*DestPtr & DestMask) | (((*CurValPtr & Mask) << Value) & Mask);
                            break;

                        case 0xc:   //shr
                            *DestPtr = (*DestPtr & DestMask) | (((*CurValPtr & Mask) >> Value) & Mask);
                            break;

                        case 0xd:   //rol
                            if(yy)
                                *DestPtr = (*DestPtr & DestMask) |
                                        ((((*CurValPtr & Mask) << Value) | ((*CurValPtr & Mask) >> (8 - Value))) & Mask);
                            else    //24bit
                                *DestPtr = (*DestPtr & DestMask) |
                                        ((((*CurValPtr & Mask) << Value) | ((*CurValPtr & Mask) >> (24 - Value))) & Mask);
                            
                            break;

                        case 0xe:   //ror
                            if(yy)
                                *DestPtr = (*DestPtr & DestMask) |
                                        ((((*CurValPtr & Mask) >> Value) | ((*CurValPtr & Mask) << (8 - Value))) & Mask);
                            else    //24bit
                                *DestPtr = (*DestPtr & DestMask) |
                                        ((((*CurValPtr & Mask) >> Value) | ((*CurValPtr & Mask) << (24 - Value))) & Mask);
                            break;

                    }
                    //break;
                }
                else
                {
                    yy = (Command & 0x78) >> 3;
                    LogicSeen = 1;

                    //other commands
                    switch(Command & 0x7)
                    {
                        case 0x0:  //delay
                        {
                            uint32_t DelayAmount = this->GetValue(cur_script, 0, yy, 5);
                            cur_script->DelayEndTime = esp_timer_get_time() + (DelayAmount * 1000);
                            break;
                        }

                        case 0x1:   //stop
                        {
                            if(yy == 0xf)
                                cur_script->DelayEndTime = 0xffffffff;      //hard stop
                            else
                            {
                                uint32_t DelayAmount = this->GetValue(cur_script, 0, yy << 6, 5);
                                cur_script->DelayEndTime = esp_timer_get_time() + (DelayAmount * 1000);
                            }
                            cur_script->StopSet = 2;
                            break;
                        }

                        case 0x2:  //if
                        case 0x3:  //wait
                        {
                            TempPos = cur_script->active_pos;

                            yy2 = cur_script->data[cur_script->active_pos];
                            cur_script->active_pos++;

                            DestPtr = this->GetDestination(cur_script, &CurValPtr, &DestData);
                            ID = DestData & 0xf;

                            //if a LED is being checked and was modified in this cycle then wait a cycle to validate
                            if(!(DestData & 0x30) && (LEDsUpdated & (1 << ID)))
                            {
                                cur_script->active_pos = TempPos - 1;
                                LogicSeen = 1;
                                continue;
                            }

                            //get the value
                            uint32_t CompareValue = this->GetValue(cur_script, 0, DestData, yy2);

                            //determine the destination value to compare based on dest
                            if(yy2)
                                Mask = 0xff << (24 - (yy2*8));
                            else
                                Mask = 0x00ffffff;

                            //compare them
                            uint8_t ValueMatched = 0;
                            switch(yy)
                            {
                                case 0: //greater than
                                    //shift left then right incase it is negative as we keep masking off the top byte
                                    if((cur_script->Version == 1) &&
                                        (((float)(((int)(*CurValPtr << 8)) >> 8) / 100.0) > ((float)(((int)(CompareValue << 8)) >> 8) / 100.0)))
                                        ValueMatched = 1;
#if BOI_VERSION == 2
                                    else if((*CurValPtr & Mask) > (CompareValue & Mask))
                                        ValueMatched = 1;
#endif
                                    break;

                                case 1: //lesser than
                                    if((cur_script->Version == 1) &&
                                        (((float)(((int)(*CurValPtr << 8)) >> 8) / 100.0) < ((float)(((int)(CompareValue << 8)) >> 8) / 100.0)))
                                        ValueMatched = 1;
#if BOI_VERSION == 2
                                    else if((*CurValPtr & Mask) < (CompareValue & Mask))
                                        ValueMatched = 1;
#endif
                                    break;

                                case 2: //equal
                                    if((cur_script->Version == 1) &&
                                        (((float)(((int)(*CurValPtr << 8)) >> 8) / 100.0) == ((float)(((int)(CompareValue << 8)) >> 8) / 100.0)))
                                        ValueMatched = 1;
#if BOI_VERSION == 2
                                    else if((*CurValPtr & Mask) == (CompareValue & Mask))
                                        ValueMatched = 1;
#endif
                                    break;

                                case 3: //greater than or equal
                                    if((cur_script->Version == 1) &&
                                        (((float)(((int)(*CurValPtr << 8)) >> 8) / 100.0) >= ((float)(((int)(CompareValue << 8)) >> 8) / 100.0)))
                                        ValueMatched = 1;
#if BOI_VERSION == 2
                                    else if((*CurValPtr & Mask) >= (CompareValue & Mask))
                                        ValueMatched = 1;
#endif
                                    break;

                                case 4: //lesser than or equal
                                    if((cur_script->Version == 1) &&
                                        (((float)(((int)(*CurValPtr << 8)) >> 8) / 100.0) <= ((float)(((int)(CompareValue << 8)) >> 8) / 100.0)))
                                        ValueMatched = 1;
#if BOI_VERSION == 2
                                    else if((*CurValPtr & Mask) <= (CompareValue & Mask))
                                        ValueMatched = 1;
#endif
                                    break;

                                case 5: //not equal
                                    if((*CurValPtr & Mask) != (CompareValue & Mask))
                                        ValueMatched = 1;
                                    break;
                            };

                            //if we match then branch to the proper location otherwise just advance
                            if(ValueMatched)
                            {
                                //if, not wait then follow the label
                                if((Command & 0x7) == 0x2)
                                {
                                    cur_script->active_pos = *(uint16_t *)(&cur_script->data[cur_script->active_pos]);
                                    cur_script->active_pos += 2;
                                }
                            }
                            else if((Command & 0x7) == 0x3)
                            {
                                //wait failed, go back to beginning of wait
                                cur_script->active_pos = TempPos - 1;
                            }
                            else
                            {
                                //value didn't match and must be an if statement, advance past it
                                cur_script->active_pos += 2;
                            }

                            //indicate we saw logic
                            LogicSeen = 1;
                            break;
                        }

                        case 0x4:  //goto
                        {
                            //change the active position
                            cur_script->active_pos = *(uint16_t *)(&cur_script->data[cur_script->active_pos]);
                            LogicSeen = 1;
                            break;
                        }

                        case 0x5: //set
                            cur_script->Variable[yy].Persist = 1;
                            break;

                        case 0x6: //clear
                            cur_script->Variable[yy].Persist = 0;
                            break;
                    }
                };

                //if the active position is ahead then set it to 0 and indicate we are done
                if(cur_script->active_pos >= cur_script->len)
                {
                    //first 3 bytes are version and led bit mask
                    cur_script->active_pos = 3;
                    LogicSeen = 1;

                    //if we have a callback then let go of the mutex incase they try to call start/stop
                    //note, we also need to make sure we aren't in a final stop state as we are reporting loop here
                    if(this->Callback && (!cur_script->StopSet || (cur_script->DelayEndTime != 0xffffffff)))
                    {
                        pthread_mutex_unlock(&led_lock);
                        this->Callback(cur_script->ID, false);
                        pthread_mutex_lock(&led_lock);
                    }                    
                }
            }

            //adjust our delay end time
            if(esp_timer_get_time() >= cur_script->DelayEndTime)
            {
                cur_script->DelayEndTime = 0;
                cur_script->StopSet = 0;
            }

            //if stop is set then delay shortly and loop around
            if(cur_script->StopSet)
            {
                //if this is 1 then delay otherwise allow a single cycle through so any updates can occur
                if(cur_script->StopSet == 1)
                {
                    //if delay time is infinite then turn the script off
                    if(cur_script->DelayEndTime == 0xffffffff)
                    {
                        //deactivate the script
                        cur_script->active = 0;

                        pthread_mutex_unlock(&led_lock);
                        this->StopScript(cur_script->ID);

                        //handle callback indicating done                        
                        if(this->Callback)
                            this->Callback(cur_script->ID, true);

                        pthread_mutex_lock(&led_lock);                        
                    }
                    continue;
                }

                //subtract 1 so we can do a single cycle on the values
                cur_script->StopSet--;
            }

            //next script
            yield();
            cur_script = cur_script->next;
        };

        pthread_mutex_unlock(&led_lock);

        //done executing instructions, go through each LED and update it's status
        cur_script = 0;
        for(i = 0; i < LED_Count; i++)
        {
            float Gap;
            float PerStepIncrement;
            float TempStartVal;
            float TempEndVal;
            int64_t CurrentTime = esp_timer_get_time() / 1000;
            int64_t LapsedTime;

            //see if we need to adjust over time
            if(this->leds[i].OverrideTime)
            {
                //we are in override, see if we need to turn it off
                if((this->leds[i].OverrideTime != LED_OVERRIDE_INFINITE) && (this->leds[i].OverrideTime <= CurrentTime))
                    this->leds[i].OverrideTime = 0;
            }
            else
            {
                //determine the active script impacting this led
                //we allow for multiple scripts to be active without using the same
                //led so re-check if the mask doesn't have the led
                if(!cur_script || !(cur_script->LEDMask & (1 << i)))
                    cur_script = this->FindScriptForLED(i);

                //if no script then continue
                if(!cur_script)
                    continue;

                uint8_t LEDTimeSet = 0;
#if BOI_VERSION == 1
                //just handle the start and end value as a float and a single running time
                if(cur_script->leds[i].Time[0])
                {
                    LEDTimeSet = 1;

                    //calculate how far along we should be
                    TempStartVal = (float)cur_script->leds[i].StartVal / 100.0;
                    TempEndVal = (float)cur_script->leds[i].EndVal / 100.0;
                    Gap = TempEndVal - TempStartVal;
                    PerStepIncrement = Gap / (float)cur_script->leds[i].Time[0];
                    LapsedTime = CurrentTime - cur_script->leds[i].StartTime[0];
                    TempStartVal = (PerStepIncrement * LapsedTime);

                    cur_script->leds[i].CurrentVal = cur_script->leds[i].StartVal + (int)(TempStartVal * 100.0);

                    //if we hit the end then set the final value
                    if(LapsedTime >= cur_script->leds[i].Time[0])
                    {
                        cur_script->leds[i].Time[0] = 0;
                        cur_script->leds[i].CurrentVal = cur_script->leds[i].EndVal;
                    }
                }

#elif BOI_VERSION == 2
                for(uint8_t x = 0; x < 3; x++)
                {
                    if(cur_script->leds[i].Time[x])
                    {
                        LEDTimeSet = 1;

                        //calculate how far along we should be
                        Mask = (16 - (8 * x));
                        if(cur_script->Version == 1)
                        {
                            TempStartVal = (float)cur_script->leds[i].StartVal / 100.0;
                            TempEndVal = (float)cur_script->leds[i].EndVal / 100.0;
                        }
                        else
                        {
                            TempStartVal = (cur_script->leds[i].StartVal >> Mask) & 0xff;
                            TempEndVal = (cur_script->leds[i].EndVal >> Mask) & 0xff;
                        }
                        Gap = TempEndVal - TempStartVal;
                        PerStepIncrement = Gap / (float)cur_script->leds[i].Time[x];
                        LapsedTime = CurrentTime - cur_script->leds[i].StartTime[x];
                        TempStartVal += (PerStepIncrement * LapsedTime);

                        if(cur_script->Version == 1)
                            cur_script->leds[i].CurrentVal = (int)(TempStartVal * 100.0) & 0x00ffffff;
                        else
                        {
                            DestMask = 0xffffff & ~(0xff << Mask);
                            cur_script->leds[i].CurrentVal = (cur_script->leds[i].CurrentVal & DestMask) | (((int)TempStartVal & 0xff) << Mask);
                        }

                        //if we hit the end then set the final value
                        if(LapsedTime >= cur_script->leds[i].Time[x])
                        {
                            cur_script->leds[i].Time[x] = 0;
                            cur_script->leds[i].CurrentVal = cur_script->leds[i].EndVal;
                        }

                        //if version 1 script then don't check the other portions
                        if(cur_script->Version == 1)
                        {
                            cur_script->leds[i].Time[1] = cur_script->leds[i].Time[0];
                            cur_script->leds[i].Time[2] = cur_script->leds[i].Time[0];
                            break;
                        }
                    }
                }
#endif
                if(LEDTimeSet)
                    this->SetLED(cur_script, i);
                else if(!LEDTimeSet && (cur_script->leds[i].CurrentVal != cur_script->leds[i].EndVal))
                {
                    //we just need to set it and go
                    cur_script->leds[i].CurrentVal = cur_script->leds[i].EndVal;
                    this->SetLED(cur_script, i);
                }
            }
        }

#if BOI_VERSION == 2
        //update the LEDs
        SPIHandler->UpdateOutBuffer();
        SPIHandler->Communicate(&SPIData);
#endif
        delay(10);
    };
}

LEDsInternal::ScriptInfoStruct *LEDsInternal::FindScriptForLED(uint16_t LED)
{
    ScriptInfoStruct *cur_script = this->scripts;
    while(cur_script)
    {
        //if active and the mask bit is set then we want to use it
        if(cur_script->active && (cur_script->LEDMask & (1 << LED)))
        {
            //if we are part of a stack then grab the stack entry
            if(cur_script->stackfirst)
                cur_script = cur_script->stackfirst;

            //found a script impacting this led, stop looking
            return cur_script;
        }

        cur_script = cur_script->next;
    }

    //return that we couldn't find anything
    return 0;
}

#if BOI_VERSION == 1
void LEDsInternal::SetLED(ScriptInfoStruct *cur_script, uint8_t entry)
{
    //set the LED based on the current value
    //the value has to be flipped so 100% is MAX_BRIGHTNESS but starting from MAX_RESOLUTION
    //and moving towards 0 while 0% == MAX_RESOLUTION

    float NewBrightness;
    uint32_t FinalBrightness;
    uint32_t TempNewBrightness;

    //CurrentVal is 0 to 10000 decimal, convert the current value into a value between 0.0000 and 1.0000
    //for what percentage we want of MaxBrightness
    if(this->leds[entry].OverrideTime)
        TempNewBrightness = this->leds[entry].OverrideValue;
    else
    {
        if(!cur_script)
            return;

        TempNewBrightness = cur_script->leds[entry].CurrentVal;
    }

    NewBrightness = (float)TempNewBrightness / 10000.0;

    //if the STAT entry then swap the %
    if(entry == LED_STAT)
        NewBrightness = (1.0 - NewBrightness) * 2.0;
    //we need to scale NewBrightness per Stevens' power law, a should be between 0.33 and 0.5
    FinalBrightness = int(pow(pow(this->MaxBrightness, POWER_LAW_A) * NewBrightness, 1/POWER_LAW_A) + 0.5);
    
    //subtract new value from MAX_RESOLUTION to get the actual value to send
    FinalBrightness = MAX_RESOLUTION - FinalBrightness;

    //if enabled or override time is set then set the actual led
    if(this->leds[entry].Enabled || this->leds[entry].OverrideTime ||
        (cur_script && cur_script->TempOverride && (cur_script->LEDMask & (1 << entry))))
    {
        this->leds[entry].CurrentVal = TempNewBrightness;
        ledcWrite(this->leds[entry].Channel, FinalBrightness);
    }
}
#elif BOI_VERSION == 2
void LEDsInternal::SetLED(ScriptInfoStruct *cur_script, uint8_t entry)
{
    //set the LED based on the current value
    //get the current scaled brightness and adjust the final output accordingly

    //if a version 1 then it is an amount of brightness so work off of all white and dim accordingly
    uint8_t r, g, b;
    float NewBrightness;
    uint32_t FinalBrightness;
    uint32_t TempNewBrightness;

    //CurrentVal is 0 to 10000 decimal, convert the current value into a value between 0.0000 and 1.0000
    //for what percentage we want of MaxBrightness
    if(this->leds[entry].OverrideTime)
        TempNewBrightness = this->leds[entry].OverrideValue;
    else
    {
        if(!cur_script)
            return;

        TempNewBrightness = cur_script->leds[entry].CurrentVal;
    }

    if(cur_script && (cur_script->Version == 1))
    {
        
        NewBrightness = (float)TempNewBrightness / 10000.0;

        //we need to scale NewBrightness per Stevens' power law, a should be between 0.33 and 0.5
        FinalBrightness = int(pow(pow((float)SPIData.SliderPos * 2.55, POWER_LAW_A) * NewBrightness, 1/POWER_LAW_A) + 0.5);

        if(FinalBrightness > 255)
            FinalBrightness = 255;

        //we now have a final brightness, set r/g/b to be the same allowing a scaling of white
        r = FinalBrightness;
        g = FinalBrightness;
        b = FinalBrightness;
    }
    else
    {
        //rgb, scale each accordingly
        NewBrightness = (float)((TempNewBrightness >> 16) & 0xff) / 255.0;
        FinalBrightness = int(pow(pow((float)SPIData.SliderPos * 2.55, POWER_LAW_A) * NewBrightness, 1/POWER_LAW_A) + 0.5);
        if(FinalBrightness > 255)
            r = 255;
        else
            r = FinalBrightness;

        NewBrightness = (float)((TempNewBrightness >> 8) & 0xff) / 255.0;
        FinalBrightness = int(pow(pow((float)SPIData.SliderPos * 2.55, POWER_LAW_A) * NewBrightness, 1/POWER_LAW_A) + 0.5);
        if(FinalBrightness > 255)
            g = 255;
        else
            g = FinalBrightness;

        NewBrightness = (float)(TempNewBrightness & 0xff) / 255.0;
        FinalBrightness = int(pow(pow((float)SPIData.SliderPos * 2.55, POWER_LAW_A) * NewBrightness, 1/POWER_LAW_A) + 0.5);
        if(FinalBrightness > 255)
            b = 255;
        else
            b = FinalBrightness;
    }

    //if enabled or override time is set then set the actual led
    if(this->leds[entry].Enabled || this->leds[entry].OverrideTime ||
        (cur_script && cur_script->TempOverride && (cur_script->LEDMask & (1 << entry))))
    {
        this->leds[entry].CurrentVal = TempNewBrightness;

        if(entry >= LED_Count_Battery)
        {
            //scale MAX_RESOLUTION to the value we want
            FinalBrightness = (MAX_RESOLUTION / 255) * FinalBrightness;
            ledcWrite(this->leds[entry].Channel, FinalBrightness);
        }
        else
            SPIHandler->SetRGBLed(entry, r, g, b);
    }
}
#endif

const char *LEDsInternal::LEDScriptIDToStr(uint8_t ID)
{
    if(ID < LED_TOTAL_SCRIPT_COUNT)
        return LEDScriptNames[ID];

    return "UNKNOWN";
}

void LEDsInternal::AddScript(uint8_t ID, const char *name, const uint8_t *data, uint16_t len)
{
    Serial.printf("Adding script %d - %s\n", ID, name);
    LEDScriptNames[ID] = name;

    ScriptInfoStruct *new_script = (ScriptInfoStruct *)malloc(sizeof(ScriptInfoStruct));
    memset(new_script, 0, sizeof(ScriptInfoStruct));

    //add to the list
    new_script->ID = ID;
    new_script->data = data;
    new_script->len = len;
    new_script->Version = data[0];
    //if 0 then force to 1
    if(!new_script->Version)
        new_script->Version = 1;
    new_script->LEDMask = *(uint16_t *)(&data[1]);

    new_script->next = this->scripts;
    this->scripts = new_script;
}

void LEDsInternal::StartScript(uint16_t ID, bool TempOverride)
{
    //swap out the script being ran
    ScriptInfoStruct *cur_script;

    cur_script = this->scripts;
    while(cur_script && (cur_script->ID != ID))
        cur_script = cur_script->next;

    if(!cur_script)
        return;

    Serial.printf("activating script %s\n", this->LEDScriptIDToStr(cur_script->ID));

    //start the script
    pthread_mutex_lock(&led_lock);
    cur_script->StopSet = 0;
    cur_script->DelayEndTime = 0;
    cur_script->active_pos = 3;     //skip version and led bit mask
    cur_script->active = 1;
    cur_script->TempOverride = TempOverride;

    for(int i = 0; i < 16; i++)
    {
        if(!cur_script->Variable[i].Persist)
            cur_script->Variable[i].Value = 0;
    }

    this->AddToStack(cur_script);
    pthread_mutex_unlock(&led_lock);
}

void LEDsInternal::StopScript(uint16_t ID)
{
    //stop a specific script
    ScriptInfoStruct *cur_script;
    ScriptInfoStruct *active_script;
    int i;

    cur_script = this->scripts;
    if(ID == LED_ALL)
    {
        //stop all scripts
        Serial.println("Stopping all LED scripts");
        pthread_mutex_lock(&led_lock);
        while(cur_script)
        {
            cur_script->active = 0;
            cur_script->stackfirst = 0;
            cur_script->stackprev = 0;
            cur_script->stacknext = 0;
            cur_script->TempOverride = 0;
            cur_script = cur_script->next;
        };
        pthread_mutex_unlock(&led_lock);
    } 
    else
    {
        Serial.printf("Stopping LED script %s\n", this->LEDScriptIDToStr(ID));
        while(cur_script && (cur_script->ID != ID))
            cur_script = cur_script->next;

        if(!cur_script)
            return;

        pthread_mutex_lock(&led_lock);
        cur_script->active = 0;
        cur_script->TempOverride = 0;
        this->RemoveFromStack(cur_script);

        for(i = 0; i < LED_Count; i++)
        {
            //if this led was not being modified then skip it otherwise find it's new value
            if(!(cur_script->LEDMask & (1 << i)))
                continue;

            active_script = FindScriptForLED(i);

            if(active_script)
            {
                if(active_script->LEDMask & (1 << i))
                    this->SetLED(active_script, i);
            }
        }

        pthread_mutex_unlock(&led_lock);

#if BOI_VERSION == 2
        //update the LEDs
        SPIHandler->Communicate(&SPIData);
#endif
    }
}

void LEDsInternal::AddToStack(ScriptInfoStruct *script)
{
    //grab our mask and look for any script that has a mask collision that is active
    ScriptInfoStruct *CurScript;
    ScriptInfoStruct *StackScript;
    ScriptInfoStruct *PrevStackScript;
    int i;

    //remove ourselves from any current chains just in-case
    this->RemoveFromStack(script);

    //set our current values based on what the LEDs currently have
    for(i = 0; i < LED_Count; i++)
        script->leds[i].CurrentVal = this->leds[i].CurrentVal;

    //now see if there is a chain to add to
    CurScript = this->scripts;
    while(CurScript)
    {
        //if not our current script, script is active, and it's mask collides then update it's stack
        if((CurScript != script) && CurScript->active && (CurScript->LEDMask & script->LEDMask))
        {
            //determine if we should be the head entry
            //this requires we either have the override flag set or the first entry does not
            //have override set
            if(script->TempOverride || (!CurScript->stackfirst || !CurScript->stackfirst->TempOverride))
            {
                //set ourselves as head of the stack
                script->stackfirst = script;
                script->stackprev = 0;

                //now our new script and we have a LED collision, grab the stack entry and walk
                //it updating the first for everyone in the list
                StackScript = CurScript->stackfirst;

                //if we have a first entry then tell it we are before it
                if(StackScript)
                {
                    StackScript->stackprev = script;
                    script->stacknext = StackScript;

                    //walk the stack and update the first for everyone
                    while(StackScript)
                    {
                        StackScript->stackfirst = script;
                        StackScript = StackScript->stacknext;
                    }
                }
                else
                {
                    //first chain entry
                    CurScript->stackfirst = script;
                    CurScript->stackprev = script;
                    script->stacknext = CurScript;
                }

                //set the script to pull valus from
                CurScript = script->stacknext;
            }
            else
            {
                //we can not the first entry, insert ourselves into the stack
                //we keep track of previous as we might have a stack of overrides
                StackScript = CurScript->stackfirst;
                PrevStackScript = CurScript;
                while(StackScript && StackScript->TempOverride)
                {
                    PrevStackScript = StackScript;
                    StackScript = StackScript->stacknext;
                };

                //update accordingly
                script->stackfirst = PrevStackScript->stackfirst;
                script->stacknext = StackScript;

                PrevStackScript->stacknext = script;
                if(StackScript)
                    StackScript->stackprev = script;

                //set the script to pull valus from
                CurScript = script->stackfirst;
            }

            //copy the current LED status to us, we just want the values
            //not any settings
            for(i = 0; i < LED_Count; i++)
            {
                script->leds[i].StartVal = CurScript->leds[i].CurrentVal;
                script->leds[i].EndVal = CurScript->leds[i].CurrentVal;
                script->leds[i].StartTime[0] = 0;
                script->leds[i].StartTime[1] = 0;
                script->leds[i].StartTime[2] = 0;
                script->leds[i].Time[0] = 0;
                script->leds[i].Time[1] = 0;
                script->leds[i].Time[2] = 0;
            }
            break;
        }

        //next one
        CurScript = CurScript->next;
    }
}

void LEDsInternal::RemoveFromStack(ScriptInfoStruct *script)
{
    //remove ourselves from a stack of scripts
    //if we are first then we need to update first for everyone
    ScriptInfoStruct *NewHead;
    ScriptInfoStruct *StackScript;

    //there is no stack, stop looking
    if(!script->stackfirst)
        return;

    //if we are the first entry then update the whole chain
    if(script->stackfirst == script)
    {
        NewHead = script->stacknext;
        NewHead->stackprev = 0;

        //if the new head does not have any entries then reset it's stackfirst
        if(!NewHead->stacknext)
            NewHead->stackfirst = 0;
        else
        {
            //update the chain for who is first
            StackScript = NewHead;
            while(StackScript)
            {
                StackScript->stackfirst = NewHead;
                StackScript = StackScript->stacknext;
            }
        }
    }
    else
    {
        //just remove from the chain
        if(script->stacknext)
            script->stacknext->stackprev = script->stackprev;
        if(script->stackprev)
            script->stackprev->stacknext = script->stacknext;

        //check the first entry, if the next entry is null due
        //to pointing at us prior then it is the last entry in
        //the chain so unset it's first entry
        if(!script->stackfirst->stacknext)
            script->stackfirst->stackfirst = 0;
    }

    //wipe out our prev/next for stack along with our first
    script->stackfirst = 0;
    script->stackprev = 0;
    script->stacknext = 0;
}

void LEDsInternal::SetGlobalVariable(uint8_t ID, double Value)
{
    this->SetGlobalVariable(ID, (uint32_t)(Value * 100.0));
}

void LEDsInternal::SetGlobalVariable(uint8_t ID, uint32_t Value)
{
    this->GlobalVariables[ID] = Value;
}

void LEDsInternal::SetLocalVariable(uint16_t VarID, double Value)
{
    this->SetLocalVariable(VarID, (uint32_t)(Value * 100.0));
}

void LEDsInternal::SetLocalVariable(uint16_t VarID, uint32_t Value)
{
    ScriptInfoStruct *CurScript;

    //upper 8 bits is the script id, lower 8 bits is the variable id
    uint16_t ScriptID = VarID >> 8;

    VarID = VarID & 0xff;
    CurScript = this->scripts;
    while(CurScript && CurScript->ID != ScriptID)
        CurScript = CurScript->next;

    if(!CurScript)
        return;

    CurScript->Variable[VarID].Value = Value;
}

uint32_t LEDsInternal::GetLEDValue(LEDEnum LED)
{
    return (float)this->leds[LED].CurrentVal;
}

void LEDsInternal::SetLEDValue(LEDEnum LED, double Value, uint32_t LengthMS)
{
    //store the value and immediately update it
#if BOI_VERSION == 1
    this->leds[LED].OverrideValue = Value * 100.0;
#elif BOI_VERSION == 2
    int IntVal = (int)(Value * 2.55) & 0xff;
    this->leds[LED].OverrideValue = (IntVal << 16) | (IntVal << 8) | IntVal; 
#endif
    if(LengthMS == LED_OVERRIDE_INFINITE)
        this->leds[LED].OverrideTime = LED_OVERRIDE_INFINITE;
    else
        this->leds[LED].OverrideTime = (esp_timer_get_time() / 1000) + LengthMS;
    this->SetLED(0, LED);

#if BOI_VERSION == 2
    SPIHandler->Communicate(&SPIData);
#endif
}

void LEDsInternal::SetLEDValue(LEDEnum LED, uint32_t Value, uint32_t LengthMS)
{
    //store the value and immediately update it
    this->leds[LED].OverrideValue = Value;
    if(LengthMS == LED_OVERRIDE_INFINITE)
        this->leds[LED].OverrideTime = LED_OVERRIDE_INFINITE;
    else
        this->leds[LED].OverrideTime = (esp_timer_get_time() / 1000) + LengthMS;
    this->SetLED(0, LED);

#if BOI_VERSION == 2
    SPIHandler->Communicate(&SPIData);
#endif
}

uint16_t LEDsInternal::GetAmbientSensor()
{
    return this->AmbientSensorValue;
}

void LEDsInternal::ReadAmbientSensor()
{
    //store the last time we got the sensor
    this->LastAmbientReading = esp_timer_get_time();
    return;

    //this is all sorts of broke. We have to power down wifi and reset registers to fix this
}

#if BOI_VERSION == 1
void LEDsInternal::SetLEDBrightness(float BrightnessPercent)
{
    int i;

    //set the scale of the LED brightness, note that this needs to follow the same logic that SetLED does
    if(BrightnessPercent < 0.0)
        BrightnessPercent = 0.0;
    else if(BrightnessPercent > 1.0)
        BrightnessPercent = 1.0;

    this->MaxBrightnessPercent = BrightnessPercent;

    //calculate a new value for max
    this->MaxBrightness = int(pow(pow(MAX_RESOLUTION, POWER_LAW_A) * BrightnessPercent, 1/POWER_LAW_A) + 0.5);

    //force all LEDs to set their brightness accordingly
    for(i = 0; i < LEDEnum::LED_Count; i++)
        this->SetLED(this->FindScriptForLED(i), i);

#if BOI_VERSION == 2
    SPIHandler->Communicate(&SPIData);
#endif
}
#endif

float LEDsInternal::GetLEDBrightness()
{
#if BOI_VERSION == 1
    //return the percent set
    return this->MaxBrightnessPercent;
#elif BOI_VERSION == 2
    return (float)SPIData.SliderPos / 100.0;
#endif
}

void LEDsInternal::SetLEDCap(uint8_t Count)
{
    int8_t i;

#if BOI_VERSION == 1
#define MIN_SET_LED LED_LEV20
#define MAX_SET_LED LED_LEV100
#elif BOI_VERSION == 2
#define MIN_SET_LED LED_0
#define MAX_SET_LED LED_8
#endif

    pthread_mutex_lock(&led_lock);

    //enable all LEDs
    for(i = MIN_SET_LED; i <= MAX_SET_LED; i++)
        this->leds[i].Enabled = 1;

    //make sure any LEDs above the count are off
#if BOI_VERSION == 1
    for(i = MIN_SET_LED + Count; i <= MAX_SET_LED; i++)
#elif BOI_VERSION == 2
    //led_0 is the top light so work backwards
    for(i = MAX_SET_LED - Count; i >= MIN_SET_LED; i--)
#endif
    {
        this->leds[i].Enabled = 0;
        this->leds[i].OverrideValue = 0;
        this->leds[i].OverrideTime = 1;
    }

    //force all refreshed
    for(i = MIN_SET_LED; i <= MAX_SET_LED; i++)
        this->SetLED(this->FindScriptForLED(i), i);

#if BOI_VERSION == 2
    //update the LEDs
    SPIHandler->Communicate(&SPIData);
#endif

    pthread_mutex_unlock(&led_lock);
}