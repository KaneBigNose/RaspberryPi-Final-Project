#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <lcd.h>

// SPI
#define SPI_CHANNEL 0
#define SPI_SPEED 1000000
#define CHAN_CONFIG_SINGLE 8

// LCD
#define LCD_RS 11
#define LCD_E 10
#define LCD_D4 6
#define LCD_D5 5
#define LCD_D6 4
#define LCD_D7 1

// Switch
#define PUSH_PIN 25

// HeartBeat
#define HEARTBEAT_CHANNEL 0
#define THRESHOLD 650

// State
enum State
{
    STATE_WAIT = 0,
    STATE_VIDEO_1 = 1,
    STATE_VIDEO_2 = 2,
    STATE_VIDEO_3 = 3,
    STATE_VIDEO_4 = 4,
    STATE_VIDEO_5 = 5,
    STATE_CALC = 6,
    STATE_RESULT = 7
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
int GetBPM();
void PrintHeartBeat(int lcd);

// State
bool ProcessState(enum State *state, int lcd);

int main()
{
    // Declare
    enum State state = STATE_WAIT;
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
    // SPI 세팅
    return wiringPiSPISetup(SPI_CHANNEL, SPI_SPEED);
}

int AnalogRead(int spiChannel, int channelConfig, int analogChannel)
{
    // 아날로그 값을 읽어주는 함수
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
    // LCD 세팅
    return lcdInit(2, 16, 4, LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7, 0, 0, 0, 0);
}

void SwitchSetup()
{
    // 스위치 세팅
    pinMode(PUSH_PIN, INPUT);
    pullUpDnControl(PUSH_PIN, PUD_UP);
}

void WaitSwitchPush()
{
    // 스위치 누르기 대기
    while (digitalRead(PUSH_PIN) == HIGH)
    {
        delay(10);
    }

    delay(50);

    while (digitalRead(PUSH_PIN) == LOW)
    {
        delay(10);
    }
}

int GetBPM()
{
    // 아날로그 값을 BPM으로 변환시키는 함수
    static int PrevAboveThreshold = 0;
    static unsigned int LastBeatTime = 0;

    int Value = AnalogRead(SPI_CHANNEL, CHAN_CONFIG_SINGLE, HEARTBEAT_CHANNEL);

    int CurrentAboveThreshold = (Value > THRESHOLD);

    if (!PrevAboveThreshold && CurrentAboveThreshold)
    {
        unsigned int CurrentTime = millis();

        if (LastBeatTime != 0)
        {
            unsigned int Interval = CurrentTime - LastBeatTime;

            LastBeatTime = CurrentTime;

            return 60000 / Interval;
        }

        LastBeatTime = CurrentTime;
    }

    PrevAboveThreshold = CurrentAboveThreshold;

    return -1;
}

void PrintHeartBeat(int lcd)
{
    // LCD에 BPM을 출력
    int BPM = 0;
    int wait = 0;

    while (++wait <= 100)
    {
        int value = GetBPM();

        if (value > 0)
        {
            BPM = value;
        }

        lcdPosition(lcd, 0, 0);
        lcdPrintf(lcd, "BPM: %d   ", BPM);

        delay(100);
    }
}

bool ProcessState(enum State *state, int lcd)
{
    // 상태 패턴을 활용
    switch (*state)
    {
    case STATE_WAIT:
    {
        printf("BPM Test Start!");

        WaitSwitchPush();

        ++(*state);

        break;
    }
    case STATE_VIDEO_1:
    case STATE_VIDEO_2:
    case STATE_VIDEO_3:
    case STATE_VIDEO_4:
    case STATE_VIDEO_5:
    {
        char cmd[100];
        sprintf(cmd, "mpv Video/video%d.mp4 &", *state);

        system(cmd);

        PrintHeartBeat(lcd);

        WaitSwitchPush();

        ++(*state);

        break;
    }
    case STATE_CALC:
    {
        break;
    }
    case STATE_RESULT:
    {
        return true;
    }
    }

    return false;
}