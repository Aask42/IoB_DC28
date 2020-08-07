#include "spi_parser.h"

#if BOI_VERSION == 2

SPIParser *SPIHandler;
SPIParser::SPIDataStruct SPIData;
pthread_mutex_t spi_lock;
pthread_mutex_t spi_data_lock;

SPIParser::SPIParser()
{
    this->LastSensorDataUpdate = 0;
    memset(this->InSPIBuffer, 0, sizeof(this->InSPIBuffer));
    memset(this->OutSPIBuffer, 0, sizeof(this->OutSPIBuffer));

    //initialize and setup the SPI interface
    pinMode(SS, OUTPUT);
    SPI.begin();

    pthread_mutex_init(&spi_lock, NULL);
    pthread_mutex_init(&spi_data_lock, NULL);
}

void SPIParser::Communicate()
{
    this->Communicate(0);
}

void SPIParser::Communicate(SPIDataStruct *Data)
{
    uint8_t Buffer[sizeof(SPIParser::OutSPIBuffer)];

    //if we talked recently then just return the original data avoid excessive hitting
    if((this->LastSensorDataUpdate + 5000ULL) > esp_timer_get_time())
    {
        if(Data)
            this->GenerateSPIDataStruct(Data);
        return;
    }

    //attempt to lock the mutex
    if(pthread_mutex_trylock(&spi_lock))
    {
        //we failed to lock, someone else snatched it, just return the original data
        if(Data)
            this->GenerateSPIDataStruct(Data);
        return;
    }

    pthread_mutex_lock(&spi_data_lock);
    memcpy(Buffer, this->OutSPIBuffer, sizeof(Buffer));
    pthread_mutex_unlock(&spi_data_lock);

    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    digitalWrite(SS, LOW);

    for (int i = 0; i < sizeof(this->OutSPIBuffer); i++)
        this->InSPIBuffer[i] = SPI.transfer(this->OutSPIBuffer[i]);

    digitalWrite(SS, HIGH);
    SPI.endTransaction();

    //force update so we can get new data
    this->LastSensorDataUpdate = esp_timer_get_time();

    if(Data)
        this->GenerateSPIDataStruct(Data);

    pthread_mutex_unlock(&spi_lock);
}

void SPIParser::SetRGBLed(uint8_t LEDNum, uint32_t RGB)
{
    if(LEDNum > 8)
        return;

    pthread_mutex_lock(&spi_data_lock);

    //printf("Setting LED %d: %x\n", LEDNum, RGB);
    this->OutSPIBuffer[4 + (LEDNum * 3)] = (RGB >> 16) & 0xff;
    this->OutSPIBuffer[4 + (LEDNum * 3) + 1] = (RGB >> 8) & 0xff;
    this->OutSPIBuffer[4 + (LEDNum * 3) + 2] = RGB & 0xff;

    pthread_mutex_unlock(&spi_data_lock);
}

void SPIParser::SetRGBLed(uint8_t LEDNum, uint8_t R, uint8_t G, uint8_t B)
{
    if(LEDNum > 8)
        return;

    pthread_mutex_lock(&spi_data_lock);

    this->OutSPIBuffer[4 + (LEDNum * 3)] = R;
    this->OutSPIBuffer[4 + (LEDNum * 3) + 1] = G;
    this->OutSPIBuffer[4 + (LEDNum * 3) + 2] = B;
    
    pthread_mutex_unlock(&spi_data_lock);
}

void SPIParser::SetGATPower(bool Enable)
{
    pthread_mutex_lock(&spi_data_lock);

    this->OutSPIBuffer[3] = 0;
    if(Enable)
        this->OutSPIBuffer[3] |= 0x80;

    pthread_mutex_unlock(&spi_data_lock);
}

void SPIParser::GenerateSPIDataStruct(SPIDataStruct *Data)
{
    //fill everything in
    Data->BtnPressed[0] = (this->InSPIBuffer[1] & 0x01);
    Data->BtnPressed[1] = (this->InSPIBuffer[1] & 0x02);
    Data->SliderPressed = (this->InSPIBuffer[1] & 0x04);
    Data->GATPowerEnabled = (this->InSPIBuffer[1] & 0x80);
    Data->SliderPos = this->InSPIBuffer[2];
    if(Data->SliderPos == 0)
        Data->SliderPos = 0x80;
    Data->BatteryVoltage = (float)this->InSPIBuffer[4] + ((float)this->InSPIBuffer[5] / 100.0);
    Data->GATVoltage = (float)this->InSPIBuffer[6] + ((float)this->InSPIBuffer[7] / 100.0);
    Data->GATCurrent = (((short)this->InSPIBuffer[8]) << 8) | this->InSPIBuffer[9];
}

#endif