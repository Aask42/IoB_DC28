#include "spi_parser.h"

#if BOI_VERSION == 2

SPIParser *SPIHandler;
SPIParser::SPIDataStruct SPIData;

SPIParser::SPIParser()
{
    memset(this->OutSPIBuffer, 0, sizeof(this->OutSPIBuffer));

    //initialize and setup the SPI interface
    pinMode(SS, OUTPUT);
    SPI.begin();
}

void SPIParser::Communicate(SPIDataStruct *Data)
{
    //send the current data for SPI
    uint8_t rgb[9][3];
    int i;
    int buffIndex = 0;
    uint8_t InSPIBuffer[31];

    for(i = 0; i < 9; i++)
    {
      rgb[i][i % 3] = 0x88;
      rgb[i][(i+1) % 3] = 0x00;
      rgb[i][(i+2) % 3] = 0x00;
    }

    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    digitalWrite(SS, LOW);

    for (i = 0; i < 4; i++) {
        InSPIBuffer[buffIndex++] = SPI.transfer(0);
    }
      for (i = 0; i < 27; i++) {
        InSPIBuffer[buffIndex++] = SPI.transfer(rgb[i / 3][i % 3]);  
    }

    digitalWrite(SS, HIGH);
    SPI.endTransaction();

    if(Data)
        this->GenerateSPIDataStruct(Data, InSPIBuffer);
}

void SPIParser::SetRGBLed(uint8_t LEDNum, uint32_t RGB)
{
    this->OutSPIBuffer[4 + (LEDNum * 3)] = (RGB >> 16) & 0xff;
    this->OutSPIBuffer[4 + (LEDNum * 3) + 1] = (RGB >> 8) & 0xff;
    this->OutSPIBuffer[4 + (LEDNum * 3) + 2] = RGB & 0xff;
}

void SPIParser::SetGATPower(bool Enable)
{
    this->OutSPIBuffer[3] = 0;
    if(Enable)
        this->OutSPIBuffer[3] |= 0x80;
}

void SPIParser::GenerateSPIDataStruct(SPIDataStruct *Data, uint8_t *InData)
{
    //fill everything in
    Data->Btn0Pressed = (InData[1] & 0x01);
    Data->Btn1Pressed = (InData[1] & 0x02);
    Data->SliderPressed = (InData[1] & 0x04);
    Data->GATPowerEnabled = (InData[1] & 0x80);
    Data->SliderPos = InData[2];
    Data->BatteryVoltage = (float)InData[4] + ((float)InData[5] / 100.0);
    Data->GATVoltage = (float)InData[6] + ((float)InData[7] / 100.0);
    Data->GATCurrent = (((short)InData[8]) << 8) | InData[9];
}

#endif