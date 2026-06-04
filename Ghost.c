#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <lcd.h>

// SPI
#define SPI_CHANNEL 0
#define SPI_SPEED 1000000
#define CHAN_CONFIG_SINGLE 8
#define HEARTBEAT_CHANNEL 0

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
    video_1 = 1,
    video_2 = 2,
    video_3 = 3,
    video_4 = 4,
    video_5 = 5,
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
void WaitSwitchPush();

// HeartBeat
void PrintHeartBeat(int lcd);

// State
bool ProcessState(enum State* state, int lcd);

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
        if (ProcessState(&state, lcd))
        {
            break;
        }
    }

    close(myFd);

    return 0;
}

int SPISetup()
{
    return wiringPiSPISetup(SPI_CHANNEL, SPI_SPEED, );
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

void WaitSwitchPush()
{
    while (digitalRead(PUSH_PIN) == HIGH)
    {
        delay(10);
    }
}

void PrintHeartBeat(int lcd)
{
    int wait = 0;
    while (++wait <= 100)
    {
        int value = AnalogRead(SPI_CHANNEL, CHAN_CONFIG_SINGLE, HEARTBEAT_CHANNEL);

        lcdClear(lcd);
        lcdPosition(lcd, 0, 0);
        lcdPrintf(lcd, "BPM: %d", value);

        delay(100);
    }
}

bool ProcessState(enum State* state, int lcd)
{
    // 상태 패턴을 활용
    switch (state)
    {
    case start:
    {
        printf("BPM Test Start!");

        WaitSwitchPush();

        ++(*state);

        break;
    }
    case video_1:
    case video_2:
    case video_3:
    case video_4:
    case video_5:
    {
        char cmd[100] = "mpv ";
        char path[] = "Video/video";
        char num = (int)(*state) + '0';
        char file[] = ".mp4 &";

        strcat(cmd, path);
        strcat(cmd, num);
        strcat(cmd, file);

        system(cmd);

        PrintHeartBeat(lcd);

        WaitSwitchPush();

        ++(*state);

        break;
    }
    case calc:
    {
        break;
    }
    case result:
    {
        return true;
    }
    }

    return false;
}