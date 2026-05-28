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

// State
enum State
{
    start = 0,
    pic_1 = 1,
    pic_2 = 2,
    pic_3 = 3,
    pic_4 = 4,
    pic_5 = 5,
    calc = 6,
    result = 7
};

// SPI
int SPISetup();
int AnalogRead(int spiChannel, int channelConfig, int analogChannel);

// LCD
int LCDSetup();

// Switch
void SwitchSetup();

// State
void ProcessState(enum State state);

int main()
{
    // Declare
    enum State state = start;
    int myFd = 0;
    int lcd = 0;

    // Setup
    wiringPiSetup();
    myFd = SPISetup();
    lcd = LCDSetup();
    SwitchSetup();

    // loop
    while (true)
    {
       ProcessState(state);
    }

    close(myFd);

    return 0;
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

void ProcessState(enum State state)
{
    switch (state)
    {
    case start:
    {
        break;
    }
    case pic_1:
    {
        break;
    }
    case pic_2:
    {
        break;
    }
    case pic_3:
    {
        break;
    }
    case pic_4:
    {
        break;
    }
    case pic_5:
    {
        break;
    }
    case calc:
    {
        break;
    }
    case result:
    {
        break;
    }
    }
}