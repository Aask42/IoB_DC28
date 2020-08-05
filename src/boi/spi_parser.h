#ifndef __spi_parser_h
#define __spi_parser_h

#if BOI_VERSION == 2

#include <Arduino.h>
#include "SPI.h"

class SPIParser
{
    public:
        typedef struct SPIDataStruct
        {
            bool GATPowerEnabled;
            bool SliderPressed;
            bool BtnPressed[2];
            uint8_t SliderPos;
            float BatteryVoltage;
            float GATVoltage;
            short GATCurrent;
        } SPIDataStruct;

        SPIParser();
        void Communicate();
        void Communicate(SPIDataStruct *Data);
        void SetRGBLed(uint8_t LEDNum, uint32_t RGB);
        void SetGATPower(bool Enable);

    private:
        void GenerateSPIDataStruct(SPIDataStruct *Data);
        uint8_t InSPIBuffer[31];
        uint8_t OutSPIBuffer[31];
        uint64_t LastSensorDataUpdate;
};

#endif

#endif