#ifndef STEPPER_H
#define STEPPER_H

#define NUM_MOTORS 2
#define STEP_LOG_SIZE     100000

// Valid stepperCmd command types...
#define STEPCMD_CHECK_LOOP_FREQ 1
#define STEPCMD_MOVE            2
#define STEPCMD_LOOP_START      3
#define STEPCMD_LOOP_STOP       4
#define STEPCMD_PAUSE           5

#include <pthread.h>
#include <QList>

// Queued step command...
struct stepperCmd {
    int cmdType;                // Command type
    long int triggerCounter;    // Number of times triggered so far
    long int numTriggers;       // Number of times to trigger
    long int cycleCounter;      // Number of times cycled so far
    long int numCycles;         // Number of times to cycle before triggering
    long int initNumCycles;     // Initial number of times to cycle before triggering
    long int endNumCycles;      // Ending number of times to cycle before triggering
    int dir;                    // Which way to move (+1/-1)
};

// StepperThread data, one per motor...
struct stepperData {
    int stepsPerMM;
    int stepPin;
    int dirPin;
    int enablePin;
    int minCyclesPerStep;
    bool stepping;
    bool enabled;
    pthread_mutex_t lock;
    QList<stepperCmd *> queuedCmdList;
    int currQueuedCmd;
    long long int stepLog[STEP_LOG_SIZE];
    int stepLogIndex;
};

class stepper {
private:
    int fd;
    pthread_t sThread;
    double cycleFreq;
    struct timespec cycleDelay;
    struct timespec enableDelay;
    stepperData stepData[NUM_MOTORS];
    pthread_mutex_t pc_lock;
    QList<stepperCmd *> priorityCmdList;
    long long int *timer; // Pointer to 64 bit 1mHz timer
    int pthreadStatus;
    //
    void initSysTime();
    inline long long int getSysTime(void);
    static void *stepperThread1 (void *);
    void stepperThread();
    void setStepperEnable(int, bool);
    void dumpCmd(const char *, stepperCmd *);

public:
    stepper();
    ~stepper();
    // Stepper system control...
    void startAll();
    void stopAll();
    void resetAll();
    void clearAll();
    // Queued command control...
    void startMotor(int motorNum);
    void stopMotor(int motorNum);
    void resetMotor(int motorNum);
    void clearMotor(int motorNum);
    // Queue commands...
    int queueMoveCmd(int motorNum, double distance, double duration, double accel);
    int queuePauseCmd(int motorNum, double duration);
    void queueLoopStartCmd(int motorNum);
    void queueLoopEndCmd(int motorNum, int startLoopIndex, int cycleCounter);
    // Stepper log control and access...
    void stepperLogStart(int motorNum);
    void stepperLogStop(int motorNum);
    void stepperLogReset(int motorNum);
    long long int *getStepperLog(int motorNum);
};

#endif
