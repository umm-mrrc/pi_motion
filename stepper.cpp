/*
*************************************
* stepper.c:
*   Provide an interface to drive stepper motors
*************************************
*/

#include <stdio.h>
#include <time.h>
#include <pthread.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>
#include <wiringPi.h>
#include <math.h>
#include <stdlib.h>

#include "stepper.h"

#include "pi_stepper_pins.h"

#define ST_BASE             (0x20003000)
#define TIMER_OFFSET        (4)
#define PULSE_WIDTH_DELAY   50
#define MIN_LOOPS_PER_STEP  15
#define STEPS_PER_MM        441

// Map the 1mHz system timer on the Raspberry Pi into our memory space
// From: http://mindplusplus.wordpress.com/2013/05/21/accessing-the-raspberry-pis-1mhz-timer/
void stepper::initSysTime()
{
    fd = -1;
    timer = NULL;
    void *st_base; // byte ptr to simplify offset math
    // Set up access to the system core memory...
    if (-1 == (fd = open("/dev/mem", O_RDONLY))) {
        fprintf(stderr, "open() failed.\n");
        return;
    }
    //  Map the timer's page into the process' address space...
    if (MAP_FAILED == (st_base = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, ST_BASE))) {
        fprintf(stderr, "mmap() failed.\n");
        fclose((FILE *)fd);
        fd = -1;
        return;
    }
    // Set the pointer to the timer based on the mapped page...
    timer = (long long int *)((char *)st_base + TIMER_OFFSET);
}

// Constructor - initialize everything...
stepper::stepper()
{
    // Put us on the RT scheduler and give us a high priority...
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    if ( sched_setscheduler( 0, SCHED_FIFO, &param ) == -1 ) {
      perror("sched_setscheduler");
      return;
    }
    // Lock memory to ensure no swapping is done...
    if (mlockall(MCL_FUTURE|MCL_CURRENT)) {
      fprintf(stderr,"WARNING: Failed to lock memory\n");
    }
    // Set up access to the 1 mHz system timer...
    initSysTime();
    // Set up to drive the Pi's GPIO pins...
    wiringPiSetup () ;
    // Set up the parameters needed to drive the individual stepper motors...
    for (int n = 0; n < NUM_MOTORS; n++) {
        stepData[n].stepPin = stepPins[n];
        pinMode (stepData[n].stepPin, OUTPUT);
        digitalWrite (stepData[n].stepPin, LOW);
        //
        stepData[n].dirPin = dirPins[n];
        pinMode (stepData[n].dirPin, OUTPUT);
        digitalWrite (stepData[n].dirPin, LOW);
        //
        stepData[n].enablePin = enablePins[n];
        pinMode (stepData[n].enablePin, OUTPUT);
        digitalWrite (stepData[n].enablePin, HIGH);
        stepData[n].enabled = false;
        //
        stepData[n].stepping = false;
        stepData[n].currQueuedCmd = 0;
        pthread_mutex_init(&(stepData[n].lock), NULL);
        //SRR ToDo *****************************************
        // stepsPerMM and minCyclesPerStep SHOULD ideally be set from a configuration file!!!!!!
//        stepData[n].minCyclesPerStep = 20;
        stepData[n].minCyclesPerStep = MIN_LOOPS_PER_STEP;
        stepData[n].stepsPerMM = STEPS_PER_MM;
        //SRR ToDo *****************************************
        stepData[n].stepLogIndex = 0;
        for (int sli = 0; sli < STEP_LOG_SIZE; sli++)
            stepData[n].stepLog[sli] = 0;
    }
    //
    // Set up our timers...
    enableDelay.tv_sec = 0;
    enableDelay.tv_nsec = 15000000;
    cycleDelay.tv_sec = 0;
    cycleDelay.tv_nsec = 23000;
    //
    // Queue a priority command to the thread to check the loop frequency...
    pthread_mutex_init(&pc_lock, NULL);
    stepperCmd *initCmd = new stepperCmd;
    initCmd->cmdType = STEPCMD_CHECK_LOOP_FREQ;
    initCmd->cycleCounter = 10000;
    priorityCmdList.append(initCmd);
    //
    // Start the thread to loop forever or until it's told to stop, whichever comes first...
    pthreadStatus = 0;
    int iReturnValue = pthread_create(&sThread, NULL, &stepperThread1, (void *)this);
    if (iReturnValue) {
        printf("Unable to start stepperThread?\n");
    }
}

// Destructor - make sure everything is turned off and cleaned up...
stepper::~stepper()
{
    // Tell the thread to close and wait for it to terminate...
    struct timespec waitTerminate, tim2;
    waitTerminate.tv_sec = 0;
    waitTerminate.tv_nsec = 10000000;
    pthreadStatus = 1;
    while (pthreadStatus == 1) nanosleep(&waitTerminate, &tim2);
    // Turn off the stepper motors...
    for (int n = 0; n < NUM_MOTORS; n++) {
        // Stop everything from stepping and turn off power to the motors...
        digitalWrite (stepData[n].stepPin, LOW);
        digitalWrite (stepData[n].dirPin, LOW);
        digitalWrite (stepData[n].enablePin, HIGH);
        // FWIW - delete the mutex locks...
        pthread_mutex_destroy(&(stepData[n].lock));
        // Clear any queued commands...
        while (!stepData[n].queuedCmdList.isEmpty())
            delete stepData[n].queuedCmdList.takeFirst();
    }
    // Clear the priority queue and delete the lock...
    while (!priorityCmdList.isEmpty())
        delete priorityCmdList.takeFirst();
    pthread_mutex_destroy(&pc_lock);
}

// INLINE(?) code to read the current time from the memory mapped system timer...
inline long long int stepper::getSysTime(void)
{
    if (timer)
        return(*timer);
    else
        return(0);
}

// Static(!?) method used to start the 'real' stepper motor thread...
void * stepper::stepperThread1(void *p_this)
{
    stepper *l_this = (stepper *)p_this;
    l_this->stepperThread();
}

// 'Real' stepper motor thread that loops forever and drives the stepper motors...
void stepper::stepperThread()
{
    struct timespec tim2;
    long long int llSysTime_prev, llSysTime_curr;
    long long int t1, t2;
    llSysTime_prev = getSysTime();
    int motorNum;
    long int sum;
    stepperCmd *currCmd;
    int currQueuedCmd, numQueuedCmds;
    int stepPins[NUM_MOTORS], dirPins[NUM_MOTORS], dirs[NUM_MOTORS];
    bool motorEnable[NUM_MOTORS];
    int num2step;
    while (!pthreadStatus) {
        //
        // Process any priority commands...
        while (!priorityCmdList.isEmpty()) {
            pthread_mutex_lock(&pc_lock);
            currCmd = priorityCmdList.takeFirst();
            pthread_mutex_unlock(&pc_lock);
            if (currCmd->cmdType == STEPCMD_CHECK_LOOP_FREQ) {
                t1 = getSysTime();
                for (int ncs = 0; ncs < currCmd->cycleCounter; ncs++) {
                    nanosleep(&cycleDelay, &tim2);
                }
                t2 = getSysTime();
                cycleFreq = 1000000.0 / (((double)t2 - (double)t1) / (double)currCmd->cycleCounter);
                printf("cycleFreq (Hz) %f\n", cycleFreq);
            }
            delete currCmd;
        }
        //
        // Step through the motor's command queues to see if we need to do anything...
        num2step = 0;
        for (motorNum = 0; motorNum < NUM_MOTORS; motorNum++) {
            motorEnable[motorNum] = false;
            if (!stepData[motorNum].stepping) continue;
            numQueuedCmds = stepData[motorNum].queuedCmdList.size();
            currQueuedCmd = stepData[motorNum].currQueuedCmd;
            if (numQueuedCmds && currQueuedCmd < numQueuedCmds) {
                motorEnable[motorNum] = true;
                // Ignore all loop start commands...
                currCmd = stepData[motorNum].queuedCmdList[currQueuedCmd];
                while (currCmd->cmdType == STEPCMD_LOOP_START) {
                    currQueuedCmd = ++stepData[motorNum].currQueuedCmd;
                    currCmd = stepData[motorNum].queuedCmdList[currQueuedCmd];
                }
                // If this is the first time we're seeing the 'pause' command
                // Then disable the stepper...
                if (currCmd->cmdType == STEPCMD_PAUSE && currCmd->dir == 0) {
                    setStepperEnable(motorNum, false);
                    currCmd->dir = 1;
                }
                // If the command being processed for the current motor doesn't trigger this cycle
                // Then loop back to check the next motor's command queue...
                if (--currCmd->cycleCounter) {
                    continue;
                }
                --currCmd->triggerCounter;
                //
                // Process a "move" command trigger event...
                if (currCmd->cmdType == STEPCMD_MOVE){
                    // Set up to step the curent motor...
                    stepPins[num2step] = stepData[motorNum].stepPin;
                    dirPins[num2step] = stepData[motorNum].dirPin;
                    dirs[num2step] = (currCmd->dir < 0)?LOW:HIGH;
                    num2step++;
                    // If there are more triggers in the "move" command
                    // Then set things up for the next iteration
                    // Else move on to the next command in the queue...
                    if (currCmd->triggerCounter) {
                        float  cim1 = (float)(currCmd->numCycles);
                        float ni = (float)(currCmd->numTriggers - currCmd->triggerCounter) + 1.0;
                        long int ci = (int)(cim1 - 2.0 * cim1 / (4.0 * ni));
                        currCmd->numCycles = (ci < currCmd->endNumCycles)?currCmd->endNumCycles:ci;
                        currCmd->cycleCounter = currCmd->numCycles;
                    }
                    else {
                        currCmd->numCycles = currCmd->initNumCycles;
                        currCmd->cycleCounter = 1;
                        currCmd->triggerCounter = currCmd->numTriggers;
                        stepData[motorNum].currQueuedCmd++;
                    }
                }
                //
                // Process a "pause" command trigger event...
                else if (currCmd->cmdType == STEPCMD_PAUSE) {
                    currCmd->cycleCounter = currCmd->numCycles;
                    if (currCmd->triggerCounter == 0) {
                        currCmd->dir = 0;
                        setStepperEnable(motorNum, true);
                        currCmd->triggerCounter = currCmd->numTriggers;
                        stepData[motorNum].currQueuedCmd++;
                    }
                }
                //
                // Process a "loop end" command trigger event...
                else if (currCmd->cmdType == STEPCMD_LOOP_STOP) {
                    currCmd->cycleCounter = currCmd->numCycles;
                    // If we're not done looping
                    // Then go back to the beginning of the loop
                    // Else move to the next command in the queue
                    if (currCmd->triggerCounter) {
                        // Keep infinite loops infinite...
                        if (currCmd->triggerCounter < 0) {
                            currCmd->triggerCounter = 0;
                        }
                        stepData[motorNum].currQueuedCmd = currCmd->dir;
                    }
                    else {
                        currCmd->triggerCounter = currCmd->numTriggers;
                        stepData[motorNum].currQueuedCmd++;
                    }
                }
            }
        }
        //
        // Drive the motors that needed to be driven...
        if (num2step > 0) {
            for (int cs = 0; cs < num2step; cs++) {
                digitalWrite (dirPins[cs], dirs[cs]);
                digitalWrite (stepPins[cs], HIGH);
            }
            for (int dd = 0; dd < PULSE_WIDTH_DELAY; dd++) sum++;
            for (int cs = 0; cs < num2step; cs++) {
                digitalWrite (stepPins[cs], LOW);
            }
        }
        // Turn off any motors that we're done with...
        for (int n = 0; n < NUM_MOTORS; n++) {
            if (!motorEnable[n]) {
                setStepperEnable(n, motorEnable[n]);
            }
        }
        // Wait a bit, then loop back to do it all over again...
        nanosleep(&cycleDelay, &tim2);
    }
pthreadStatus = 2;
}

// Start processing the commands queued for a stepper motor...
void stepper::startMotor(int motorNum)
{
    if (motorNum < 0 || motorNum >= NUM_MOTORS)
      return;
    setStepperEnable(motorNum, true);
    stepData[motorNum].stepping = true;
}

// Stop processing the commands queued for a stepper motor...
void stepper::stopMotor(int motorNum)
{
    if (motorNum < 0 || motorNum >= NUM_MOTORS)
      return;
    stepData[motorNum].stepping = false;
    setStepperEnable(motorNum, false);
}

// Stop processing the commands queued for a stepper motor and reset the queue...
void stepper::resetMotor(int motorNum)
{
    if (motorNum < 0 || motorNum >= NUM_MOTORS)
      return;
    stepData[motorNum].stepping = false;
    stepData[motorNum].currQueuedCmd = 0;
    setStepperEnable(motorNum, false);
}

// Stop processing the commands queued for a stepper motor and delete all commands...
void stepper::clearMotor(int motorNum)
{
    if (motorNum < 0 || motorNum >= NUM_MOTORS)
      return;
    stepData[motorNum].stepping = false;
    // Clear all commands queued for the motor...
    while (!stepData[motorNum].queuedCmdList.isEmpty())
        delete stepData[motorNum].queuedCmdList.takeFirst();
    stepData[motorNum].currQueuedCmd = 0;
    setStepperEnable(motorNum, false);
}


void stepper::stepperLogStart(int motorNum)
{
    if (motorNum < 0 || motorNum >= NUM_MOTORS)
        return;
}

void stepper::stepperLogStop(int motorNum)
{
    if (motorNum < 0 || motorNum >= NUM_MOTORS)
        return;
}

void stepper::stepperLogReset(int motorNum)
{
    if (motorNum < 0 || motorNum >= NUM_MOTORS)
        return;
}

long long int *stepper::getStepperLog(int motorNum)
{
    if (motorNum < 0 || motorNum >= NUM_MOTORS)
        return(NULL);
    return(NULL);
}

// Start everything...
void stepper::startAll()
{
    for (int motorNum = 0; motorNum < NUM_MOTORS; motorNum++)
        startMotor(motorNum);
}

// Stop everything......
void stepper::stopAll()
{
    for (int motorNum = 0; motorNum < NUM_MOTORS; motorNum++)
        stopMotor(motorNum);
}

// Stop and reset all motion...
void stepper::resetAll()
{
    for (int motorNum = 0; motorNum < NUM_MOTORS; motorNum++)
        resetMotor(motorNum);
}

// Clear everything...
void stepper::clearAll()
{
    for (int motorNum = 0; motorNum < NUM_MOTORS; motorNum++)
        clearMotor(motorNum);
}

// Queue a move command for a motor...
int stepper::queueMoveCmd(int motorNum, double distance, double duration, double accel)
{
    if (motorNum < 0 || motorNum >= NUM_MOTORS)
        return(-1);
    // Create a move command...
    stepperCmd *newMove = new stepperCmd;
    newMove->cmdType = STEPCMD_MOVE;
    double adistance = fabs(distance);
    newMove->numTriggers = (long int)(adistance * stepData[motorNum].stepsPerMM);
    newMove->triggerCounter = newMove->numTriggers;
    double triggersPerSec = (adistance / duration) * (double)(stepData[motorNum].stepsPerMM);
    long int endNumCycles = (long int)(cycleFreq / triggersPerSec);
    endNumCycles = (endNumCycles < stepData[motorNum].minCyclesPerStep)?stepData[motorNum].minCyclesPerStep:endNumCycles;
    long int initNumCycles = (long int)(200.0 / accel);
    if (initNumCycles < endNumCycles) initNumCycles = endNumCycles;
    newMove->numCycles = initNumCycles;
    newMove->cycleCounter = 1;
    newMove->initNumCycles = initNumCycles;
    newMove->endNumCycles = endNumCycles;
    newMove->dir = (distance < 0)?-1:1;
    dumpCmd("ADDING Move", newMove);
    // Add the move command to the thread's list...
    pthread_mutex_lock(&stepData[motorNum].lock);
    stepData[motorNum].queuedCmdList.append(newMove);
    pthread_mutex_unlock(&stepData[motorNum].lock);
    return(0);
}

// Queue a pause command for a motor...
int stepper::queuePauseCmd(int motorNum, double duration)
{
    if (motorNum < 0 || motorNum >= NUM_MOTORS)
        return(-1);
    // Create a pause command...
    stepperCmd *newPause = new stepperCmd;
    newPause->cmdType = STEPCMD_PAUSE;
    newPause->numTriggers = 1;
    newPause->triggerCounter = 1;
    long int numCycles = (long int)(cycleFreq * duration) - 1;
    newPause->numCycles = numCycles;
    newPause->cycleCounter = numCycles;
    newPause->initNumCycles = numCycles;
    newPause->endNumCycles = 0;
    newPause->dir = 0;
    dumpCmd("ADDING Pause", newPause);
    // Add the pause command to the thread's list...
    pthread_mutex_lock(&stepData[motorNum].lock);
    stepData[motorNum].queuedCmdList.append(newPause);
    pthread_mutex_unlock(&stepData[motorNum].lock);
    return(0);
}

void stepper::setStepperEnable(int motorNum, bool enabled)
{
    struct timespec tim2;
    if (motorNum < 0 || motorNum >= NUM_MOTORS)
        return;
    if (enabled == stepData[motorNum].enabled)
        return;
    if (enabled) {
        digitalWrite (stepData[motorNum].enablePin, LOW);
        nanosleep(&enableDelay, &tim2);
    }
    else {
        digitalWrite (stepData[motorNum].enablePin, HIGH);
    }
    stepData[motorNum].enabled = enabled;
}

void stepper::queueLoopStartCmd(int motorNum)
{
    stepperCmd *newCmd = new stepperCmd;
    newCmd->cmdType = STEPCMD_LOOP_START;
    newCmd->numTriggers = 0;
    newCmd->triggerCounter = 0;
    newCmd->numCycles = 0;
    newCmd->cycleCounter = 0;
    newCmd->initNumCycles = 0;
    newCmd->endNumCycles = 0;
    newCmd->dir = 0;
    // Add the command to the thread's list...
    pthread_mutex_lock(&stepData[motorNum].lock);
    stepData[motorNum].queuedCmdList.append(newCmd);
    pthread_mutex_unlock(&stepData[motorNum].lock);
    dumpCmd("ADDING loop start", newCmd);
}

void stepper::queueLoopEndCmd(int motorNum, int startLoopIndex, int cycleCounter)
{
    stepperCmd *newCmd = new stepperCmd;
    newCmd->cmdType = STEPCMD_LOOP_STOP;
    newCmd->numTriggers = cycleCounter;
    newCmd->triggerCounter = cycleCounter;
    newCmd->numCycles = 1;
    newCmd->cycleCounter = 1;
    newCmd->initNumCycles = 1;
    newCmd->endNumCycles = 1;
    newCmd->dir = startLoopIndex;
    // Add the command to the thread's list...
    pthread_mutex_lock(&stepData[motorNum].lock);
    stepData[motorNum].queuedCmdList.append(newCmd);
    pthread_mutex_unlock(&stepData[motorNum].lock);
    dumpCmd("ADDING loop end", newCmd);
}

void stepper::dumpCmd(const char *text, stepperCmd *cmd)
{
    printf("\n%s\n", text);
    printf("  cmd: %d\n", cmd->cmdType);
    printf("  triggerCounter: %ld\n", cmd->triggerCounter);
    printf("  numTriggers: %ld\n", cmd->numTriggers);
    printf("  cycleCounter: %ld\n", cmd->cycleCounter);
    printf("  numCycles: %ld\n", cmd->numCycles);
    printf("  initNumCycles: %ld\n", cmd->initNumCycles);
    printf("  endNumCycles: %ld\n", cmd->endNumCycles);
    printf("  dir: %d\n", cmd->dir);
}
