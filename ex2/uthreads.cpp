#include "uthreads.h"
#include <iostream>
#include <setjmp.h>
#include <signal.h>
#include <vector>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>

///////////////// black box ///////////////////////
typedef unsigned long address_t;
address_t translate_address(address_t addr);
typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7
#define QUANTUM_ERROR "thread library error: quantum must be a non negative number\n"
#define ID_ERROR "thread library error: invalid id\n"
#define EXCEEDING_NAX_ERROR "thread library error: exceeded max number of threads\n"
#define MUTEX_LOCKED_ERROR "thread library error: mutex already locked by this thread\n"
#define MUTEX_UNLOCKED_ERROR "thread library error: mutex already unlocked or locked by other thread\n"
#define BAD_ALLOC_ERROR "system error: bad allocation\n"
#define SIGACTION_ERROR "system error: sigaction error\n"
#define SET_TIMER_ERROR "system error: setitimer error\n"
#define MILLION 1000000
#define INITIAL_VAL 0
#define OCCUPIED 1
#define MUTEX_IS_FREE -1
#define MAIN_THREAD 0
#define TIME_OUT 1
#define BLOCK 3
#define TERMINATE 2



///////////////// structs ////////////////////

typedef struct thread{
    int id = INITIAL_VAL;
    char stack[STACK_SIZE] = {};
    sigjmp_buf buffer = {};
    int ready = 1;
    int blocked_by_mutex = INITIAL_VAL;
    int waitingForMutex = INITIAL_VAL;
    int blocked = INITIAL_VAL;
    int quantumsCounter = INITIAL_VAL;



}thread;

typedef struct threadLibrary{
    char idArray[MAX_THREAD_NUM] = {};
    thread** threadArr = new thread*[MAX_THREAD_NUM];
    std::vector<int> readyThreads;
    std::vector<int> waitingForMuteX;
    int threadQuantum = INITIAL_VAL;
    int current = INITIAL_VAL;
    int mutex = MUTEX_IS_FREE;
    int threadsCounter = INITIAL_VAL;
    int totalQuantums = 1;
    struct sigaction sa = {};
    sigset_t set = {};
    struct itimerval timer = {};
    struct itimerval stop_timer = {};
    struct itimerval start_timer = {};
}threadLibrary;


threadLibrary *lib;

///////////////////////////////////////////////////


/*
 * setup the new thread fields
 */
int setup(void (*f)(void)= nullptr);

/*
 * find min id for new thread
 */
int findRightId();

/*
 * make the scheduler decision
 */
void roundRobin(int interrupt);

/*
 * override the default func for sigaction
 */
void handler(int sig);

/*
 * set the fields of timer
 */
void createTimer();

/*
 * set the sigsetjmp buffer
 */
void setSigjmpBufPointers(thread *t, void (*f)(void));

/*
 * terminate and relase all library resources
 */
void terminateLibrary();

/*
 * delete thread from ready list
 */
void delFromReady(int tid);

/*
 * rapper for block thread func
 */
int block_wrapper(bool mutex_blocking, int tid);


/*
 * Description: This function initializes the thread library.
 * You may assume that this function is called before any other thread library
 * function, and that it is called exactly once. The input to the function is
 * the length of a quantum in micro-seconds. It is an error to call this
 * function with non-positive quantum_usecs.
 * Return value: On success, return 0. On failure, return -1.
*/

int uthread_init(int quantum_usecs){
    if(quantum_usecs <= INITIAL_VAL){
        std::cerr << QUANTUM_ERROR;
        return -1;
    }
    try {
        lib = new threadLibrary;

        lib->threadQuantum = quantum_usecs;
        lib->current = INITIAL_VAL;
        lib->sa.sa_handler = &handler;
        if (sigaction(SIGVTALRM, &lib->sa, nullptr) < INITIAL_VAL) {
            printf(SIGACTION_ERROR);
        }
        sigemptyset(&lib->set);
        sigaddset(&(lib->set), SIGVTALRM);
        createTimer();
        setitimer(ITIMER_VIRTUAL, &lib->stop_timer, &lib->start_timer);
        int id = setup();
        setitimer(ITIMER_VIRTUAL, &lib->start_timer, nullptr);
        return id;
    } catch (std::bad_alloc&) {
        std::cerr << BAD_ALLOC_ERROR;
        exit(1);
    }
}
/*
 * Description: This function creates a new thread, whose entry point is the
 * function f with the signature void f(void). The thread is added to the end
 * of the READY threads list. The uthread_spawn function should fail if it
 * would cause the number of concurrent threads to exceed the limit
 * (MAX_THREAD_NUM). Each thread should be allocated with a stack of size
 * STACK_SIZE bytes.
 * Return value: On success, return the ID of the created thread.
 * On failure, return -1.
*/

int uthread_spawn(void (*f)(void)){
    setitimer(ITIMER_VIRTUAL, &lib->stop_timer, &lib->start_timer);
    if(lib->threadsCounter == MAX_THREAD_NUM){
        std::cerr << EXCEEDING_NAX_ERROR;
        setitimer(ITIMER_VIRTUAL, &lib->start_timer, nullptr);
        sigprocmask(SIG_UNBLOCK, &(lib->set), nullptr);
        return -1;
    }
    int res = setup(f);
    lib->readyThreads.push_back(lib->threadArr[res]->id);
    setitimer(ITIMER_VIRTUAL, &lib->start_timer, nullptr);
    sigprocmask(SIG_UNBLOCK, &(lib->set), nullptr);
    return res;
}

int findRightId(){
    for (int i = INITIAL_VAL; i < MAX_THREAD_NUM; ++i) {
        if (lib->idArray[i] == MAIN_THREAD){
            lib->idArray[i] = OCCUPIED;
            return i;
        }
    }
    return -1;
}

/*
 * Description: This function terminates the thread with ID tid and deletes
 * it from all relevant control structures. All the resources allocated by
 * the library for this thread should be released. If no thread with ID tid
 * exists it is considered an error. Terminating the main thread
 * (tid == 0) will result in the termination of the entire process using
 * exit(0) [after releasing the assigned library memory].
 * Return value: The function returns 0 if the thread was successfully
 * terminated and -1 otherwise. If a thread terminates itself or the main
 * thread is terminated, the function does not return.
*/
void terminateLibrary(){
    for (int i = INITIAL_VAL; i < MAX_THREAD_NUM; ++i) {
        if (lib->idArray[i] != INITIAL_VAL){
            delete[] lib->threadArr[i];
        }
    }
    delete [] lib->threadArr;
    delete lib;
    exit(0);
}

int uthread_terminate(int tid){
    setitimer(ITIMER_VIRTUAL, &lib->stop_timer, &lib->start_timer);
    if(tid >= MAX_THREAD_NUM || tid < INITIAL_VAL || lib->idArray[tid] == INITIAL_VAL){
        std::cerr << ID_ERROR;
        setitimer(ITIMER_VIRTUAL, &lib->start_timer, nullptr);
        return -1;
    }
    if (tid == MAIN_THREAD) {
        terminateLibrary();
    }
    if(lib->mutex == tid){
        setitimer(ITIMER_VIRTUAL, &lib->start_timer, nullptr);
        uthread_mutex_unlock();
        setitimer(ITIMER_VIRTUAL, &lib->stop_timer, &lib->start_timer);
    }
    delete[] lib->threadArr[tid];
    lib->idArray[tid] = INITIAL_VAL;
    lib->threadsCounter--;
    if(lib->current != tid){
        delFromReady(tid);
        setitimer(ITIMER_VIRTUAL, &lib->start_timer, nullptr);
        return 0;
    }
    lib->current = lib->readyThreads.front();
    lib->readyThreads.erase(lib->readyThreads.begin());
    roundRobin(2);
    return 0;
}

/*
 * Description: This function blocks the thread with ID tid. The thread may
 * be resumed later using uthread_resume. If no thread with ID tid exists it
 * is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision
 * should be made. Blocking a thread in BLOCKED state has no
 * effect and is not considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_block(int tid){
    setitimer(ITIMER_VIRTUAL, &lib->stop_timer, &lib->start_timer);
    int res = block_wrapper(false, tid);
    setitimer(ITIMER_VIRTUAL, &lib->start_timer, nullptr);
    return res;
}

void delFromReady(int tid){
    for (unsigned long i = INITIAL_VAL; i < lib->readyThreads.size(); ++i) {
        if (lib->readyThreads[i] == tid){
            lib->readyThreads.erase(lib->readyThreads.begin() + i);
            break;
        }
    }
}



/*
 * Description: This function resumes a blocked thread with ID tid and moves
 * it to the READY state if it's not synced. Resuming a thread in a RUNNING or READY state
 * has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid){
    setitimer(ITIMER_VIRTUAL, &lib->stop_timer, &lib->start_timer);
    if (tid < INITIAL_VAL || tid >= MAX_THREAD_NUM || lib->idArray[tid] == INITIAL_VAL){
        std::cerr << ID_ERROR;
        setitimer(ITIMER_VIRTUAL, &lib->start_timer, nullptr);
        return -1;
    }
    if (lib->threadArr[tid]->blocked == INITIAL_VAL){
        setitimer(ITIMER_VIRTUAL, &lib->start_timer, nullptr);
        return 0;
    }
    lib->threadArr[tid]->blocked = INITIAL_VAL;
    if (lib->threadArr[tid]->blocked_by_mutex == INITIAL_VAL){
        lib->readyThreads.push_back(tid);
    }
    setitimer(ITIMER_VIRTUAL, &lib->start_timer, nullptr);
    return 0;
}


/*
 * Description: This function tries to acquire a mutex.
 * If the mutex is unlocked, it locks it and returns.
 * If the mutex is already locked by different thread, the thread moves to BLOCK state.
 * In the future when this thread will be back to RUNNING state,
 * it will try again to acquire the mutex.
 * If the mutex is already locked by this thread, it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_mutex_lock(){
    setitimer(ITIMER_VIRTUAL, &lib->stop_timer, &lib->start_timer);
    if (lib->current == lib->mutex){
        std::cerr << MUTEX_LOCKED_ERROR;
        setitimer(ITIMER_VIRTUAL, &lib->start_timer, nullptr);
        return -1;
    }
    if (lib->mutex == MUTEX_IS_FREE) {
        lib->mutex = lib->current;
        setitimer(ITIMER_VIRTUAL, &lib->start_timer, nullptr);
        return 0;
    }
    lib->threadArr[lib->current]->waitingForMutex = OCCUPIED;
    lib->threadArr[lib->current]->blocked_by_mutex = OCCUPIED;
    lib->waitingForMuteX.push_back(lib->current);
    block_wrapper(true ,lib->current);
    uthread_mutex_lock();
    return 0;
}

/*
 * Description: This function releases a mutex.
 * If there are blocked threads waiting for this mutex,
 * one of them (no matter which one) moves to READY state.
 * If the mutex is already unlocked, it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_mutex_unlock(){
    setitimer(ITIMER_VIRTUAL, &lib->stop_timer, &lib->start_timer);
    if (lib->mutex == MUTEX_IS_FREE || lib->mutex != lib->current){
        std::cerr << MUTEX_UNLOCKED_ERROR;
        setitimer(ITIMER_VIRTUAL, &lib->start_timer, nullptr);
        return -1;
    }
    lib->mutex = MUTEX_IS_FREE;
    if(!lib->waitingForMuteX.empty()){
        lib->threadArr[*(lib->waitingForMuteX.begin())]->blocked_by_mutex = INITIAL_VAL;
        if (lib->threadArr[*(lib->waitingForMuteX.begin())]->blocked == INITIAL_VAL) {
            lib->readyThreads.push_back(*(lib->waitingForMuteX.begin()));
            lib->waitingForMuteX.erase(lib->waitingForMuteX.begin());
        }
    }
    setitimer(ITIMER_VIRTUAL, &lib->start_timer, nullptr);
    return 0;
}

/*
 * Description: This function returns the thread ID of the calling thread.
 * Return value: The ID of the calling thread.
*/
int uthread_get_tid(){
    return lib->current;
}


/*
 * Description: This function returns the total number of quantumsCounter since
 * the library was initialized, including the current quantum.
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number
 * should be increased by 1.
 * Return value: The total number of quantumsCounter.
*/

int uthread_get_total_quantums(){
    return lib->totalQuantums;
}


/*
 * Description: This function returns the number of quantumsCounter the thread with
 * ID tid was in RUNNING state. On the first time a thread runs, the function
 * should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state
 * when this function is called, include also the current quantum). If no
 * thread with ID tid exists it is considered an error.
 * Return value: On success, return the number of quantumsCounter of the thread with ID tid.
 * 			     On failure, return -1.
*/
int uthread_get_quantums(int tid){
    sigprocmask(SIG_SETMASK, &(lib->set), nullptr);
    if (tid < INITIAL_VAL || tid >= MAX_THREAD_NUM || lib->idArray[tid] == INITIAL_VAL){
        std::cerr << ID_ERROR;
        sigprocmask(SIG_UNBLOCK, &(lib->set), nullptr);
        return -1;
    }
    sigprocmask(SIG_UNBLOCK, &(lib->set), nullptr);
     return lib->threadArr[tid]->quantumsCounter;
}

int setup(void (*f)(void)){
    try{
        auto *t = new thread;
        if(f == nullptr){
            t->quantumsCounter++;
        }
        t->id = findRightId();
        lib->threadArr[t->id] = t;
        lib->threadsCounter++;
        setSigjmpBufPointers(t, f);
        return t->id;
    }
    catch (std::bad_alloc&) {
        std::cerr << BAD_ALLOC_ERROR;
        exit(1);
    }
}

void setSigjmpBufPointers(thread *t, void (*f)(void)){
    address_t sp, pc;
    sp = (address_t)(t->stack) + STACK_SIZE - sizeof(address_t);
    int res = sigsetjmp(t->buffer, OCCUPIED);
    if(res != INITIAL_VAL) {
        return;
    }
    (t->buffer->__jmpbuf)[JB_SP] = translate_address(sp);
    if(f != nullptr){
        pc = (address_t)f;
        (t->buffer->__jmpbuf)[JB_PC] = translate_address(pc);
    }
}

void createTimer(){
    lib->timer.it_value.tv_sec = lib->threadQuantum/MILLION;
    lib->timer.it_value.tv_usec = lib->threadQuantum - (lib->timer.it_value.tv_sec*MILLION);
    lib->timer.it_interval.tv_sec= lib->threadQuantum/MILLION;
    lib->timer.it_interval.tv_usec = lib->threadQuantum - (lib->timer.it_value.tv_sec*MILLION);
    if (setitimer (ITIMER_VIRTUAL, &lib->timer, nullptr)) {
        printf(SET_TIMER_ERROR);
    }
}
void handler(int sig){
    if(sig == SIGVTALRM) {
        roundRobin(TIME_OUT);
    }
}

int block_wrapper(bool mutex_blocking, int tid){
    if (tid < INITIAL_VAL || tid >= MAX_THREAD_NUM || lib->idArray[tid] == INITIAL_VAL ||
    (tid == MAIN_THREAD && !mutex_blocking)){
        std::cerr << ID_ERROR;
        return -1;
    }
    if (lib->threadArr[tid]->blocked == OCCUPIED){
        return 0;
    }
    if (!mutex_blocking){
        lib->threadArr[tid]->blocked = OCCUPIED;
    }
    if (lib->current != tid){
        delFromReady(tid);
        return 0;
    }
    roundRobin(BLOCK);
    return 0;
}

void roundRobin(int interrupt) {
    if (interrupt == TIME_OUT || interrupt == BLOCK) {
        int res = sigsetjmp(lib->threadArr[lib->current]->buffer, OCCUPIED);
        if (res == OCCUPIED) {
            return;
        }
        if (interrupt == TIME_OUT) {
            lib->readyThreads.push_back(lib->current);
        }
        lib->current = lib->readyThreads.front();
        lib->readyThreads.erase(lib->readyThreads.begin());
    }
    if(lib->threadArr[lib->current]->waitingForMutex == OCCUPIED){
        lib->threadArr[lib->current]->waitingForMutex = INITIAL_VAL;
        lib->mutex = lib->current;
    }
    lib->threadArr[lib->current]->quantumsCounter++;
    lib->totalQuantums++;
    if(interrupt == TERMINATE || interrupt == BLOCK){
        setitimer(ITIMER_VIRTUAL, &lib->timer, nullptr);
    }
    siglongjmp(lib->threadArr[lib->current]->buffer, OCCUPIED);
}

////////////////////////////////////// blackbox functions /////////////////////////////////////////

address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}



