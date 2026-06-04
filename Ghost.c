#include <stdio.h>
#include <stdlib.h>
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

// State
bool ProcessState(enum State &state);

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
        if (ProcessState(state))
        {
            break;
        }
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

void WaitSwitchPush()
{
    while (digitalRead(PUSH_PIN) == HIGH)
    {
        delay(10);
    }
}

bool ProcessState(enum State &state)
{
    // 상태 패턴을 활용
    switch (state)
    {
    case start:
    {
        char* stateString = "Start";
        printf("State: %s", stateString);

        WaitSwitchPush();

        ++state;

        break;
    }
    case video_1:
    {
        char* stateString = "Level 1";
        printf("State: %s", stateString);

        system("mpv Video/video1.mp4");

        WaitSwitchPush();

        ++state;

        break;
    }
    case video_2:
    {
        char* stateString = "Level 2";
        printf("State: %s", stateString);

        system("mpv Video/video2.mp4");

        WaitSwitchPush();

        ++state;

        break;
    }
    case video_3:
    {
        char* stateString = "Level 3";
        printf("State: %s", stateString);

        system("mpv Video/video3.mp4");

        WaitSwitchPush();

        ++state;

        break;
    }
    case video_4:
    {
        char* stateString = "Level 4";
        printf("State: %s", stateString);

        system("mpv Video/video4.mp4");

        WaitSwitchPush();

        ++state;

        break;
    }
    case video_5:
    {
        char* stateString = "Level 5";
        printf("State: %s", stateString);

        system("mpv Video/video5.mp4");

        WaitSwitchPush();

        ++state;

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