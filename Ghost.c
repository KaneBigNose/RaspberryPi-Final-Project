#include <stdio.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <lcd.h>

// SPI
#define SPI_CHANNEL 0
#define SPI_SPEED 1000000

// LCD
#define LCD_RS 11
#define LCD_E 10
#define LCD_D4 6
#define LCD_D5 5
#define LCD_D6 4
#define LCD_D7 1

// Switch
#define PUSH_PIN 25

// SPI
int SPISetup();
int AnalogRead(int spiChannel, int channelConfig, int analogChannel);

// LCD
int LCDSetup();

// Switch
void SwtichSetup();

int main()
{
    // Declare
    int myFd = 0;
    int lcd = 0;

    // Setup
    wiringPiSetup();
    myFd = SPISetup();
    lcd = LCDSetup();
    SwitchSetup();

    

    close(myFd);
}

int SPISetup()
{
    return wiringPiSPISetup(SPI_CHANNEL, SPI_SPEED);
}

int AnalogRead(int spiChannel, int channelConfig, int analogChannel)
{
    // MCP 3004 모듈은 0 ~ 3번 채널만 있음
    if (analogChannel < 0 || analogChannel > 3)
    {
        printf("InValid: Analog Channel");
        return -1;
    }

    unsigned char buffer[3] = {1};
    buffer[1] = (channelConfig + analogChannel) << 4;

    wiringPiSPIDataRW(spiChannel, buffer, 3);

    return ((buffer[1] & 3) << 8) + buffer[2];
}

int LCDSetup()
{
    return lcdInit(2, 16, 4, LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7, 0, 0, 0, 0);
}

void SwitchSetup()
{
    pinMode(PUSH_PIN, INPUT);
    pullUpDnControl(PUSH_PIN, PUD_UP);
}