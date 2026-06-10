#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <lcd.h>

// SPI
#define SPI_CHANNEL 0
#define SPI_SPEED 1000000
#define CHAN_CONFIG_SINGLE 8

// LCD
#define LCD_RS 0
#define LCD_E 2
#define LCD_D4 6
#define LCD_D5 5
#define LCD_D6 4
#define LCD_D7 1

// Switch
#define PUSH_PIN 25

// HeartBeat
#define HEARTBEAT_CHANNEL 0
#define THRESHOLD 650
#define BPM_MIN 40
#define BPM_MAX 200

// Video
#define VIDEO_COUNT 3

// State
enum State
{
    STATE_WAIT = 0,
    STATE_VIDEO_1 = 1,
    STATE_VIDEO_2 = 2,
    STATE_VIDEO_3 = 3,
    STATE_CALC = 4,
    STATE_RESULT = 5
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
void ResetBPM();
int MeasureHeartBeatWhileVideo(int lcd, const char *videoPath);

// State
bool ProcessState(enum State *state, int lcd);

int videoAvgBPM[VIDEO_COUNT] = {0};
int totalAvgBPM = 0;
int prevAboveThreshold = 0;
unsigned int lastBeatTime = 0;

int main()
{
    enum State state = STATE_WAIT;
    int myFd = 0;
    int lcd = 0;

    wiringPiSetup();
    myFd = SPISetup();
    lcd = LCDSetup();
    SwitchSetup();

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
    return wiringPiSPISetup(SPI_CHANNEL, SPI_SPEED);
}

int AnalogRead(int spiChannel, int channelConfig, int analogChannel)
{
    if (analogChannel < 0 || analogChannel > 3)
    {
        printf("InValid: Analog Channel\n");
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

    delay(50);

    while (digitalRead(PUSH_PIN) == LOW)
    {
        delay(10);
    }
}

int GetBPM()
{
    int Value = AnalogRead(SPI_CHANNEL, CHAN_CONFIG_SINGLE, HEARTBEAT_CHANNEL);
    int CurrentAboveThreshold = (Value > THRESHOLD);

    if (!prevAboveThreshold && CurrentAboveThreshold)
    {
        unsigned int CurrentTime = millis();

        if (lastBeatTime != 0)
        {
            unsigned int Interval = CurrentTime - lastBeatTime;

            lastBeatTime = CurrentTime;

            if (Interval == 0)
            {
                return -1;
            }

            return 60000 / Interval;
        }

        lastBeatTime = CurrentTime;
    }

    prevAboveThreshold = CurrentAboveThreshold;

    return -1;
}

void ResetBPM()
{
    prevAboveThreshold = 0;
    lastBeatTime = 0;
}

int MeasureHeartBeatWhileVideo(int lcd, const char *videoPath)
{
    int sum = 0;
    int count = 0;
    int lastBPM = 0;
    int status = 0;
    pid_t pid = fork();

    if (pid == 0)
    {
        execlp("mpv", "mpv", videoPath, (char *)NULL);
        _exit(127);
    }

    if (pid < 0)
    {
        return -1;
    }

    ResetBPM();

    while (waitpid(pid, &status, WNOHANG) == 0)
    {
        int value = GetBPM();

        if (value >= BPM_MIN && value <= BPM_MAX)
        {
            lastBPM = value;
            sum += value;
            count++;
        }

        lcdPosition(lcd, 0, 0);
        lcdPrintf(lcd, "BPM: %-3d       ", lastBPM);
        lcdPosition(lcd, 0, 1);
        lcdPrintf(lcd, "Samples: %-3d    ", count);

        delay(100);
    }

    if (count == 0)
    {
        return 0;
    }

    return sum / count;
}

bool ProcessState(enum State *state, int lcd)
{
    switch (*state)
    {
    case STATE_WAIT:
    {
        printf("BPM Test Start!\n");
        lcdClear(lcd);
        lcdPosition(lcd, 0, 0);
        lcdPrintf(lcd, "BPM Test Start");
        lcdPosition(lcd, 0, 1);
        lcdPrintf(lcd, "Press switch");

        WaitSwitchPush();

        ++(*state);

        break;
    }
    case STATE_VIDEO_1:
    case STATE_VIDEO_2:
    case STATE_VIDEO_3:
    {
        char videoPath[100];
        int videoIndex = *state - STATE_VIDEO_1;
        int avgBPM = 0;

        sprintf(videoPath, "Video/video%d.mp4", *state);

        lcdClear(lcd);

        avgBPM = MeasureHeartBeatWhileVideo(lcd, videoPath);

        if (avgBPM < 0)
        {
            lcdClear(lcd);
            lcdPosition(lcd, 0, 0);
            lcdPrintf(lcd, "Video error");
            lcdPosition(lcd, 0, 1);
            lcdPrintf(lcd, "Press switch");

            WaitSwitchPush();

            return true;
        }

        videoAvgBPM[videoIndex] = avgBPM;

        lcdClear(lcd);
        lcdPosition(lcd, 0, 0);
        lcdPrintf(lcd, "Video %d done", videoIndex + 1);
        lcdPosition(lcd, 0, 1);
        lcdPrintf(lcd, "Press switch");

        WaitSwitchPush();

        ++(*state);

        break;
    }
    case STATE_CALC:
    {
        int sum = 0;
        int count = 0;

        for (int i = 0; i < VIDEO_COUNT; i++)
        {
            if (videoAvgBPM[i] > 0)
            {
                sum += videoAvgBPM[i];
                count++;
            }
        }

        if (count > 0)
        {
            totalAvgBPM = sum / count;
        }
        else
        {
            totalAvgBPM = 0;
        }

        ++(*state);

        break;
    }
    case STATE_RESULT:
    {
        printf("Total AVG BPM: %d\n", totalAvgBPM);

        lcdClear(lcd);
        lcdPosition(lcd, 0, 0);
        lcdPrintf(lcd, "Total AVG BPM");
        lcdPosition(lcd, 0, 1);
        lcdPrintf(lcd, "%d", totalAvgBPM);

        WaitSwitchPush();

        return true;
    }
    }

    return false;
}
