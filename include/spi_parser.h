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
        void SetRGBLed(uint8_t LEDNum, uint8_t R, uint8_t G, uint8_t B);
        void SetGATPower(bool Enable);
        void UpdateOutBuffer();

    private:
        void GenerateSPIDataStruct(SPIDataStruct *Data);
        uint8_t InSPIBuffer[31];
        uint8_t OutSPIBuffer[31];
        uint8_t RGBData[27];
        int64_t LastSensorDataUpdate;
        uint8_t LastSliderPos;
};

#endif

#endif