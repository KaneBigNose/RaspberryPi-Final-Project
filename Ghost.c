#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <lcd.h>

// MCP3004와 통신하기 위한 SPI 설정입니다.
#define SPI_CHANNEL 0
#define SPI_SPEED 1000000
#define CHAN_CONFIG_SINGLE 8

// TextLCD에 연결한 wiringPi 핀 번호입니다.
#define LCD_RS 0
#define LCD_E 2
#define LCD_D4 6
#define LCD_D5 5
#define LCD_D6 4
#define LCD_D7 1

// 푸시 스위치 입력 핀입니다.
#define PUSH_PIN 25

// 심박 센서와 BPM 계산에 사용하는 설정입니다.
#define HEARTBEAT_CHANNEL 0
#define HEARTBEAT_DELTA 30
#define BPM_MIN 40
#define BPM_MAX 200
#define MIN_BEAT_INTERVAL_MS 400
#define MAX_BEAT_INTERVAL_MS 1500
#define HEARTBEAT_SAMPLE_DELAY_MS 50
#define BPM_WARMUP_TIMEOUT_MS 5000

// 재생할 영상 개수입니다.
#define VIDEO_COUNT 3

// 프로그램의 전체 흐름을 관리하는 상태입니다.
enum State
{
    // 첫 번째 영상 시작 전 대기 상태입니다.
    STATE_WAIT = 0,

    // 1번 영상을 재생하면서 심박수를 측정하는 상태입니다.
    STATE_VIDEO_1 = 1,

    // 2번 영상을 재생하면서 심박수를 측정하는 상태입니다.
    STATE_VIDEO_2 = 2,

    // 3번 영상을 재생하면서 심박수를 측정하는 상태입니다.
    STATE_VIDEO_3 = 3,

    // 측정된 영상별 평균 심박수로 전체 평균을 계산하는 상태입니다.
    STATE_CALC = 4,

    // 최종 평균 심박수를 출력하는 상태입니다.
    STATE_RESULT = 5
};

// SPI 통신을 초기화합니다.
int SPISetup();

// MCP3004의 아날로그 채널 하나를 읽습니다.
int AnalogRead(int spiChannel, int channelConfig, int analogChannel);

// TextLCD를 초기화합니다.
int LCDSetup();

// 푸시 스위치 입력을 초기화합니다.
void SwitchSetup();

// 스위치가 눌렸다가 떼어질 때까지 기다립니다.
void WaitSwitchPush();

// 심박 센서 신호를 이용해 BPM을 계산합니다.
int GetBPM();

// MCP3004에서 현재 심박 센서 ADC 값을 읽습니다.
int GetHeartBeatRawValue();

// 영상 시작 전 표시할 초기 BPM을 미리 측정합니다.
int WarmUpBPM(int lcd);

// 이전 심박 감지 시간 정보를 초기화합니다.
void ResetBPM();

// 영상을 재생하고, 영상이 재생되는 동안 평균 BPM을 측정합니다.
int MeasureHeartBeatWhileVideo(int lcd, const char *videoPath, int initialBPM);

// 현재 상태에 맞는 동작을 한 단계 실행합니다.
bool ProcessState(enum State *state, int lcd);

// 각 영상에서 측정한 평균 BPM을 저장합니다.
int videoAvgBPM[VIDEO_COUNT] = {0};

// 유효한 영상별 평균 BPM으로 계산한 최종 평균 BPM입니다.
int totalAvgBPM = 0;

// 이전 센서 값이 임계값을 넘었는지 저장합니다.
int prevAboveThreshold = 0;

// 마지막으로 감지한 심박 시간입니다.
unsigned int lastBeatTime = 0;

// KY-039 센서의 현재 기준 ADC 값입니다.
int heartbeatBaseValue = 0;

// 기준 ADC 값이 초기화되었는지 저장합니다.
int heartbeatBaseReady = 0;

int main()
{
    // 현재 프로그램 상태입니다.
    enum State state = STATE_WAIT;

    // wiringPi가 반환하는 SPI 파일 디스크립터입니다.
    int myFd = 0;

    // lcdInit이 반환하는 LCD 핸들입니다.
    int lcd = 0;

    // 라즈베리파이 하드웨어 인터페이스를 초기화합니다.
    wiringPiSetup();
    myFd = SPISetup();
    lcd = LCDSetup();
    SwitchSetup();

    // STATE_RESULT에서 종료 신호가 나올 때까지 상태를 반복 처리합니다.
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
    // 설정한 속도로 SPI 채널 0을 엽니다.
    return wiringPiSPISetup(SPI_CHANNEL, SPI_SPEED);
}

int AnalogRead(int spiChannel, int channelConfig, int analogChannel)
{
    // MCP3004는 0번부터 3번까지의 아날로그 채널을 가집니다.
    if (analogChannel < 0 || analogChannel > 3)
    {
        printf("InValid: Analog Channel\n");
        return -1;
    }

    // MCP3004와 주고받는 3바이트 SPI 버퍼입니다.
    unsigned char buffer[3] = {1};

    // 단일 입력 모드에서 읽을 채널을 명령 바이트에 설정합니다.
    buffer[1] = (channelConfig + analogChannel) << 4;

    // SPI로 명령을 보내고 변환 결과를 받습니다.
    if (wiringPiSPIDataRW(spiChannel, buffer, 3) == -1)
    {
        printf("SPI read error\n");
        return -1;
    }

    // 반환된 바이트를 10비트 아날로그 값으로 변환합니다.
    return ((buffer[1] & 3) << 8) + buffer[2];
}

int LCDSetup()
{
    // 16x2 LCD를 4비트 모드로 초기화합니다.
    return lcdInit(2, 16, 4, LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7, 0, 0, 0, 0);
}

void SwitchSetup()
{
    // 내부 풀업 저항을 사용하므로 스위치를 누르면 LOW가 됩니다.
    pinMode(PUSH_PIN, INPUT);
    pullUpDnControl(PUSH_PIN, PUD_UP);
}

void WaitSwitchPush()
{
    // 스위치가 눌릴 때까지 기다립니다.
    while (digitalRead(PUSH_PIN) == HIGH)
    {
        delay(10);
    }

    // 스위치 채터링을 줄이기 위한 간단한 지연입니다.
    delay(50);

    // 스위치가 떼어질 때까지 기다립니다.
    while (digitalRead(PUSH_PIN) == LOW)
    {
        delay(10);
    }
}

int GetBPM()
{
    // KY-039의 아날로그 값을 MCP3004를 통해 읽습니다.
    int Value = GetHeartBeatRawValue();

    if (Value < 0)
    {
        return -1;
    }

    // 첫 샘플을 기준 ADC 값으로 사용합니다.
    if (!heartbeatBaseReady)
    {
        heartbeatBaseValue = Value;
        heartbeatBaseReady = 1;
    }

    // 센서의 천천히 변하는 기준값을 따라가도록 보정합니다.
    heartbeatBaseValue = ((heartbeatBaseValue * 9) + Value) / 10;

    // 기준값보다 충분히 높게 튀는 순간을 심박 신호로 판단합니다.
    int CurrentAboveThreshold = ((Value - heartbeatBaseValue) > HEARTBEAT_DELTA);

    if (!prevAboveThreshold && CurrentAboveThreshold)
    {
        // wiringPi 기준 현재 시간을 밀리초 단위로 가져옵니다.
        unsigned int CurrentTime = millis();

        if (lastBeatTime != 0)
        {
            // 이전 심박과 현재 심박 사이의 시간 간격입니다.
            unsigned int Interval = CurrentTime - lastBeatTime;

            if (Interval < MIN_BEAT_INTERVAL_MS)
            {
                return -1;
            }

            lastBeatTime = CurrentTime;

            if (Interval > MAX_BEAT_INTERVAL_MS)
            {
                return -1;
            }

            // 밀리초 단위 심박 간격을 BPM으로 변환합니다.
            return 60000 / Interval;
        }

        // 첫 번째로 감지된 심박 시간을 저장합니다.
        lastBeatTime = CurrentTime;
    }

    // 다음 상승 에지를 감지하기 위해 현재 임계값 상태를 저장합니다.
    prevAboveThreshold = CurrentAboveThreshold;

    // 새로 계산된 BPM이 없으면 -1을 반환합니다.
    return -1;
}

int GetHeartBeatRawValue()
{
    // KY-039가 연결된 MCP3004 채널의 원시 ADC 값을 반환합니다.
    return AnalogRead(SPI_CHANNEL, CHAN_CONFIG_SINGLE, HEARTBEAT_CHANNEL);
}

int WarmUpBPM(int lcd)
{
    // 영상 시작 전 손가락을 안정시키고 첫 BPM을 확보합니다.
    int elapsed = 0;
    int lastBPM = 0;

    ResetBPM();

    while (elapsed < BPM_WARMUP_TIMEOUT_MS)
    {
        int value = GetBPM();
        int rawValue = GetHeartBeatRawValue();

        if (value >= BPM_MIN && value <= BPM_MAX)
        {
            lastBPM = value;
            break;
        }

        lcdPosition(lcd, 0, 0);
        lcdPrintf(lcd, "Measuring BPM   ");
        lcdPosition(lcd, 0, 1);
        lcdPrintf(lcd, "ADC: %-4d      ", rawValue);

        delay(HEARTBEAT_SAMPLE_DELAY_MS);
        elapsed += HEARTBEAT_SAMPLE_DELAY_MS;
    }

    return lastBPM;
}

void ResetBPM()
{
    // 새 영상 측정을 시작하기 전에 심박 감지 상태를 초기화합니다.
    prevAboveThreshold = 0;
    lastBeatTime = 0;
    heartbeatBaseValue = 0;
    heartbeatBaseReady = 0;
}

int MeasureHeartBeatWhileVideo(int lcd, const char *videoPath, int initialBPM)
{
    // sum과 count는 해당 영상의 평균 BPM을 계산하는 데 사용합니다.
    int sum = 0;
    int count = 0;

    // LCD에 표시할 마지막 유효 BPM 값입니다.
    int lastBPM = initialBPM;

    // mpv 자식 프로세스의 종료 상태입니다.
    int status = 0;

    // waitpid의 반환값을 저장합니다.
    pid_t waitResult = 0;

    // 영상 파일을 읽을 수 있는지 먼저 확인합니다.
    if (access(videoPath, R_OK) != 0)
    {
        printf("Video file open error: %s\n", videoPath);
        return -1;
    }

    // mpv 실행을 위한 자식 프로세스 ID입니다.
    pid_t pid = fork();

    if (pid == 0)
    {
        // 자식 프로세스는 자기 자신을 mpv 실행으로 교체합니다.
        execlp("mpv", "mpv", "--really-quiet", videoPath, (char *)NULL);
        _exit(127);
    }

    if (pid < 0)
    {
        // 영상 프로세스를 만들 수 없으면 에러를 반환합니다.
        return -1;
    }

    // 영상 프로세스가 종료될 때까지 BPM을 측정합니다.
    while (true)
    {
        waitResult = waitpid(pid, &status, WNOHANG);

        if (waitResult == pid)
        {
            break;
        }

        if (waitResult < 0)
        {
            printf("Video wait error: %s\n", strerror(errno));
            return -1;
        }

        // 심박 계산 로직에서 반환한 최신 BPM 값입니다.
        int value = GetBPM();

        // 센서 입력 확인을 위해 원시 ADC 값도 함께 읽습니다.
        int rawValue = GetHeartBeatRawValue();

        // 센서 노이즈로 생기는 비현실적인 BPM 값은 제외합니다.
        if (value >= BPM_MIN && value <= BPM_MAX)
        {
            lastBPM = value;
            sum += value;
            count++;
        }

        // 영상 재생 중에는 현재 측정된 BPM을 LCD에 표시합니다.
        lcdPosition(lcd, 0, 0);
        lcdPrintf(lcd, "BPM: %-3d       ", lastBPM);
        lcdPosition(lcd, 0, 1);
        lcdPrintf(lcd, "ADC: %-4d      ", rawValue);

        delay(HEARTBEAT_SAMPLE_DELAY_MS);
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 127)
    {
        printf("mpv 실행 실패: %d\n", WEXITSTATUS(status));
        return -1;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
    {
        printf("mpv 종료 경고: %d\n", WEXITSTATUS(status));
    }

    if (WIFSIGNALED(status))
    {
        printf("mpv signal error: %d\n", WTERMSIG(status));
        return -1;
    }

    if (count == 0)
    {
        // 영상이 너무 짧아 새 샘플이 없으면 초기 BPM을 사용합니다.
        return initialBPM;
    }

    // 해당 영상에서 측정한 평균 BPM을 반환합니다.
    return sum / count;
}

bool ProcessState(enum State *state, int lcd)
{
    // 현재 상태에 따라 필요한 동작을 실행합니다.
    switch (*state)
    {
    case STATE_WAIT:
    {
        // 테스트 시작 전 화면을 표시하고 스위치 입력을 기다립니다.
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
        // 현재 상태 번호를 이용해 재생할 영상 경로를 만듭니다.
        char videoPath[100];

        // 현재 영상 평균을 저장할 배열 인덱스입니다.
        int videoIndex = *state - STATE_VIDEO_1;

        // 현재 영상에서 측정한 평균 BPM 임시 저장값입니다.
        int avgBPM = 0;

        // 영상 시작 전에 확보한 초기 BPM 값입니다.
        int initialBPM = 0;

        sprintf(videoPath, "Video/video%d.mp4", *state);

        lcdClear(lcd);

        // 짧은 영상에서도 0만 보이지 않도록 시작 전 BPM을 먼저 잡습니다.
        initialBPM = WarmUpBPM(lcd);

        lcdClear(lcd);

        // 현재 영상을 재생하고, 재생 중일 때만 BPM을 측정합니다.
        avgBPM = MeasureHeartBeatWhileVideo(lcd, videoPath, initialBPM);

        if (avgBPM < 0)
        {
            // 영상 프로세스를 시작하지 못하면 에러를 표시하고 종료합니다.
            lcdClear(lcd);
            lcdPosition(lcd, 0, 0);
            lcdPrintf(lcd, "Video error");
            lcdPosition(lcd, 0, 1);
            lcdPrintf(lcd, "Press switch");

            WaitSwitchPush();

            return true;
        }

        // 마지막 전체 평균 계산에 사용할 영상별 평균 BPM을 저장합니다.
        videoAvgBPM[videoIndex] = avgBPM;

        // 다음 영상으로 넘어가기 전 스위치 입력을 기다립니다.
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
        // 유효한 영상별 평균 BPM만 합산합니다.
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
            // 최종 결과는 유효한 영상별 평균 BPM들의 평균입니다.
            totalAvgBPM = sum / count;
        }
        else
        {
            // 유효한 BPM이 없으면 결과를 0으로 유지합니다.
            totalAvgBPM = 0;
        }

        ++(*state);

        break;
    }
    case STATE_RESULT:
    {
        // 최종 전체 평균 BPM만 출력합니다.
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
