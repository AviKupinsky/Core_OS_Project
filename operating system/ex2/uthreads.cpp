#include "uthreads.h"
#include <string>
#include <set>
#include <vector>
#include "iostream"
#include <map>
#include <algorithm>
#include "setjmp.h"
#include "signal.h"
#include "sys/time.h"
#include "queue"


#define THREAD_ERROR "Thread library error: "
#define SYSTEM_ERROR "System error: "
#define THREADS_LIMIT_ERROR "number of threads has exceed the limit."
#define BLOCK_MAIN_ERROR "main thread can't be blocked."
#define SLEEP_MAIN_ERROR "main thread can't enter sleep mode."
#define NO_THREAD_ID_ERROR "the thread dose not exist."
#define MEMORY_ERROR "memory allocation failed."
#define TIME_POSITIVE_ERROR "time must be positive."
#define SIGNALS_BLOCK_ERROR "unable to block / unblock signals."
#define TIMER_ERROR "couldn't set / cancel timer."
#define SIGVTALRM_OVERRIDE_ERROR "couldn't override the 'SIGVTALRM' signal handler."
#define ENVIRONMENT_SETUP_ERROR "could not setup thread environment."
#define ENVIRONMENT_SAVING_ERROR "could not save running thread environment."
#define ENTRY_POINT_ERROR "entry point can't be null."
#define READY_STATE "READY"
#define BLOCKED_STATE "BLOCKED"
#define RUNNING_STATE "RUNNING"
#define FAILURE -1
#define NO_THREAD -1
#define SUCCESS 0
#define MAIN_THREAD_ID 0
#define RETURN_VAL 1
#define SYS_ERROR_EXIT_VAL 1


using namespace std;


typedef unsigned long address_t;
#ifdef __x86_64__
/* code for 64 bit Intel arch */
#define JB_SP 6
#define JB_PC 7


/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5


/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#endif

sigjmp_buf env[MAX_THREAD_NUM];
struct itimerval quantum;
struct itimerval cancel_timer;
static struct sigaction group_of_signals;
int count_total_quantums = 0;


/**
 * Class that represents a thread.
 */
class Thread{
    string state_ = "READY";
    int id_ = 0;
    thread_entry_point entry_point_ = nullptr;
    char *stack_pointer = nullptr;
    address_t sp{}, pc{};
    timeval sleep{};
    int times_used_in_quantums = 0;
public:
    /**
     * Thread object constructor, initializes thread environment and variables.
     * @param id - the thread id.
     * @param entry_point - entry point of thread (function).
     */
    Thread(int id, thread_entry_point entry_point){
        times_used_in_quantums = 0;
        id_ = id;
        entry_point_ = entry_point;
        stack_pointer = new (nothrow) char [STACK_SIZE];
        if (!stack_pointer) {
            cerr << SYSTEM_ERROR << MEMORY_ERROR << endl;
            exit(1);
        }
        sp = (address_t) stack_pointer + STACK_SIZE - sizeof(address_t);
        pc = (address_t) entry_point_;
        if (sigsetjmp(env[id], RETURN_VAL) != SUCCESS){
            cerr << SYSTEM_ERROR << ENVIRONMENT_SETUP_ERROR << endl;
            exit(1);
        }
        (env[id]->__jmpbuf)[JB_SP] = translate_address(sp);
        (env[id]->__jmpbuf)[JB_PC] = translate_address(pc);
        if (sigemptyset(&env[id]->__saved_mask) != SUCCESS){
            cerr << SYSTEM_ERROR << ENVIRONMENT_SETUP_ERROR << endl;
            exit(1);
        }
    }


    /**
     * Thread object constructor of the main thread, initializes thread environment and variables.
     * @param id - the thread id.
     */
    explicit Thread(int id){
        state_ = RUNNING_STATE;
        id_ = id;
        if (sigsetjmp(env[id], RETURN_VAL) != SUCCESS){
            cerr << SYSTEM_ERROR << ENVIRONMENT_SETUP_ERROR << endl;
            exit(1);
        }
        if (sigemptyset(&env[id]->__saved_mask) != SUCCESS){
            cerr << SYSTEM_ERROR << ENVIRONMENT_SETUP_ERROR << endl;
            exit(1);
        }
    }


    /**
     * Increases the number of times the thread had run.
     */
    void quantums_use(){
        times_used_in_quantums++;
    }


    /**
     * Getter of the class Thread.
     * @return the thread id.
     */
    int get_thread_id() const {
        return id_;
    }


    /**
    * Getter of the class Thread.
    * @return the number of times the thread had run.
    */
    int get_quantum() const {
        return times_used_in_quantums;
    }


    /**
     * Sets the amount of time to sleep.
     */
    void set_sleeping(int num_quantums){
        sleep.tv_sec = num_quantums * quantum.it_value.tv_sec;
        sleep.tv_usec = num_quantums * quantum.it_value.tv_usec;
    }


    /**
     * Checks if thread is in sleep state.
     * @return true if thread is sleeping, false otherwise.
     */
    int check_sleeping() const {
        return (sleep.tv_sec == 0 && sleep.tv_usec == 0);
    }


    /**
     * Reduces the time it remains for thread to sleep.
     */
    void reduce_sleeping(){
        sleep.tv_usec -= quantum.it_value.tv_usec;
        sleep.tv_sec -= quantum.it_value.tv_sec;
        if (sleep.tv_sec < 0) {
            sleep.tv_sec = 0;
        }
        if (sleep.tv_usec < 0) {
            sleep.tv_usec = 0;
        }
    }


    /**
     * Getter of the class Thread.
     * @return the thread sate.
     */
    string get_state() const {
        return state_;
    }


    /**
     * Changes thread state to ready.
     */
    void change_state_to_ready(){
        state_ = READY_STATE;
    }


    /**
     * Changes thread state to blocked.
     */
    void change_state_to_blocked(){
        state_ = BLOCKED_STATE;
    }


    /**
     * Changes thread state to running.
     */
    void change_state_to_running(){
        state_ = RUNNING_STATE;
    }


    /**
     * Thread class de-constructor.
     */
    ~Thread(){
        if (id_ != 0){
            delete stack_pointer;
        }
    }
};


int running_thread;
map<int, Thread*> threads;
vector<int> ready_queue;
map<int, Thread*> blocked;
map<int, Thread*> sleeping;
priority_queue<int, vector<int>, greater<int>> ids;
bool set_timer = false;


/**
 * Initializes the group of signals 'group_of_signals' - signals which we will block while preforming important tasks.
 * The same group will be used to unblock the signals after we are done. The signals being blocked are the SIGVTALRM and
 * SIGALRM signals.
 * @return - 0 on success, and -1 otherwise.
 */
int  create_signals_group(){
    if (sigemptyset(&group_of_signals.sa_mask) == FAILURE){
        return FAILURE;
    }
    if (sigaddset(&group_of_signals.sa_mask, SIGALRM) == FAILURE){
        return FAILURE;
    }
    if (sigaction(SIGVTALRM, &group_of_signals, nullptr) == FAILURE) {
        return FAILURE;
    }
    return SUCCESS;
}


/**
 * Frees all allocated recourses.
 */
void erase_allocated_threads(){
    for (auto &it : threads){
        delete it.second;
    }
}


/**
 * The function prints the message for the error.
 * @param first THREAD_ERROR/SYSTEM_ERROR: a string to print or thread error or system error.
 * @param second a string with the error message.
 * @param check if we should exit the program or just return FAILURE.
 */
int error_handler(const string& first, const string& second, bool check){
    cerr << first << second << endl;
    if (check){
        erase_allocated_threads();
        exit(SYS_ERROR_EXIT_VAL);
    } else{
        return FAILURE;
    }
}


/**
 * Blocks the signals in 'group_of_signals' using the function 'sigprocmask'.
 */
void block_signals(){
    if (sigprocmask(SIG_BLOCK, &group_of_signals.sa_mask, nullptr) == FAILURE){
        error_handler(SYSTEM_ERROR, SIGNALS_BLOCK_ERROR, true);
    }
}


/**
 * Unblocks the signals in 'group_of_signals' using the function 'sigprocmask'.
 */
void unblock_signals(){
    if (sigprocmask(SIG_UNBLOCK, &group_of_signals.sa_mask, nullptr) == FAILURE){
        error_handler(SYSTEM_ERROR, SIGNALS_BLOCK_ERROR, true);
    }
}


/**
 * Set timer.
 */
void set_timer_threads(itimerval item){
    if (setitimer(ITIMER_VIRTUAL, &item, nullptr)){
        error_handler(SYSTEM_ERROR, TIMER_ERROR, true);
    }
}


/**
 * Runs a thread. NOTE: the function will not return to the calling function.
 * @param thread - the thread to run.
 */
void run_thread(Thread *thread){
    block_signals();
    thread->quantums_use();
    running_thread = thread->get_thread_id();
    thread->change_state_to_running();
    if (set_timer){
        set_timer_threads(quantum);
        set_timer = false;
    }
    unblock_signals();
    siglongjmp(env[thread->get_thread_id()], RETURN_VAL);
}


/**
 * Keeps track of the total number of quantum that had passed, updates sleeping threads quantum (and the sleeping data
 * structure). Decides witch thread should run next and runs it.
 */
void make_scheduling_decision(){
    count_total_quantums++;
    if (!sleeping.empty()){
        auto  it = sleeping.begin();
        while (it != sleeping.end()){
            it->second->reduce_sleeping();
            if (it->second->check_sleeping()){
                if (it->second->get_state() != BLOCKED_STATE){
                    ready_queue.push_back(it->second->get_thread_id());
                }
                it = sleeping.erase(it);
                continue;
            }
            it++;
        }
    }
    int next_thread_to_run_id = MAIN_THREAD_ID;
    if (!ready_queue.empty()) {
        next_thread_to_run_id = ready_queue.front();
    }
    ready_queue.erase(ready_queue.begin());
    run_thread(threads.at(next_thread_to_run_id));
}


/**
 * Manges the flaw of threads whenever quantum time has passed. The scheduler will update the sleeping time remaining to
 * the sleeping threads, save the old state of the currently running thread and run the next ready thread.
 */
void scheduler(int b){
    int ret_val = sigsetjmp(env[running_thread], RETURN_VAL);
    block_signals();
    if (ret_val == FAILURE){
        error_handler(SYSTEM_ERROR, ENVIRONMENT_SAVING_ERROR, true);
    } else if (ret_val == RETURN_VAL) {
        return;
    }
    auto thread = threads.find(running_thread);
    ready_queue.push_back(running_thread);
    thread->second->change_state_to_ready();
    make_scheduling_decision();
}


/**
 * @brief initializes the thread library.
 *
 * Once this function returns, the main thread (tid == 0) will be set as RUNNING. There is no need to
 * provide an entry_point or to create a stack for the main thread - it will be using the "regular" stack and PC.
 * You may assume that this function is called before any other thread library function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs) {
    if (quantum_usecs <= 0){
        return error_handler(THREAD_ERROR, TIME_POSITIVE_ERROR, false);
    }
    count_total_quantums++;
    if (create_signals_group() != SUCCESS){
        return error_handler(SYSTEM_ERROR, SIGNALS_BLOCK_ERROR, true);
    }
    cancel_timer.it_value.tv_usec = 0;
    cancel_timer.it_value.tv_sec = 0;
    cancel_timer.it_interval.tv_usec = 0;
    cancel_timer.it_interval.tv_sec = 0;

    quantum.it_value.tv_usec = quantum_usecs;
    quantum.it_value.tv_sec = 0;
    quantum.it_interval.tv_usec = quantum_usecs;
    quantum.it_interval.tv_sec = 0;
    for(int i = 0; i < MAX_THREAD_NUM; i++){
        ids.push(i);
    }
    struct sigaction sa{};
    sa.sa_handler = &scheduler;
    if (sigaction(SIGVTALRM, &sa, nullptr) == FAILURE)
    {
        return error_handler(SYSTEM_ERROR, SIGVTALRM_OVERRIDE_ERROR, true);
    }
    int id = ids.top();
    ids.pop();
    auto *main_thread = new (nothrow) Thread(id);
    if (!main_thread){
        return error_handler(SYSTEM_ERROR, MEMORY_ERROR, true);
    }
    running_thread = main_thread->get_thread_id();
    threads.insert({id, main_thread});
    threads.at(running_thread)->quantums_use();
    set_timer_threads(quantum);
    return SUCCESS;
}


/**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 * It is an error to call this function with a null entry_point.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/
int uthread_spawn(thread_entry_point entry_point) {
    block_signals();
    if (ids.empty()){
        return error_handler(THREAD_ERROR, THREADS_LIMIT_ERROR, false);
    }
    if (!entry_point){
        return error_handler(THREAD_ERROR, ENTRY_POINT_ERROR, false);
    }
    int id = ids.top();
    ids.pop();
    auto *thread = new (nothrow) Thread(id, entry_point);
    if (!thread){
        return error_handler(SYSTEM_ERROR, MEMORY_ERROR, true);
    }
    threads.insert({id, thread});
    ready_queue.push_back(id);
    unblock_signals();
    return id;
}

/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/
int uthread_terminate(int tid) {
    block_signals();
    if (tid == MAIN_THREAD_ID){
        erase_allocated_threads();
        exit(0);
    }
    auto thread = threads.find(tid);
    if (thread == threads.end()){
        return error_handler(THREAD_ERROR, NO_THREAD_ID_ERROR, false);
    }
    else if (thread->second->get_state() == READY_STATE){
        ready_queue.erase(find(ready_queue.begin(), ready_queue.end(), tid));
        if (!thread->second->check_sleeping()){
            sleeping.erase(sleeping.find(tid));
        }
    }
    else if (thread->second->get_state() == RUNNING_STATE){
        set_timer_threads(cancel_timer);
        set_timer = true;
        running_thread = NO_THREAD;
        threads.erase(thread);
        ids.push(tid);
        delete thread->second;
        make_scheduling_decision();
    }
    else if (thread->second->get_state() == BLOCKED_STATE){
        blocked.erase(blocked.find(tid));
        if (!thread->second->check_sleeping()){
            sleeping.erase(sleeping.find(tid));
        }
    }
    threads.erase(thread);
    ids.push(tid);
    delete thread->second;
    unblock_signals();
    return 0;
}


/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
 * BLOCKED state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block(int tid) {
    int ret_val = sigsetjmp(env[running_thread], RETURN_VAL);
    if (ret_val == FAILURE){
        return error_handler(SYSTEM_ERROR, ENVIRONMENT_SAVING_ERROR, true);
    } else if (ret_val == RETURN_VAL){
        return 0;
    }
    block_signals();
    if (tid == MAIN_THREAD_ID){
        return error_handler(THREAD_ERROR, BLOCK_MAIN_ERROR, false);
    }
    auto thread = threads.find(tid);
    if (thread == threads.end()){
        return error_handler(THREAD_ERROR, NO_THREAD_ID_ERROR, false);
    }
    if (thread->second->get_state() == BLOCKED_STATE){
        return SUCCESS;
    }
    else if (thread->second->get_state() == READY_STATE){
        thread->second->change_state_to_blocked();
        if(thread->second->check_sleeping()){
            ready_queue.erase(find(ready_queue.begin(), ready_queue.end(), tid));
        }
        thread->second->change_state_to_blocked();
        blocked.insert({tid, thread->second});
    }
    else if (thread->second->get_state() == RUNNING_STATE){
        thread->second->change_state_to_blocked();
        blocked.insert({tid, thread->second});
        set_timer_threads(cancel_timer);
        set_timer = true;
        running_thread = NO_THREAD;
        make_scheduling_decision();
    }
    unblock_signals();
    return SUCCESS;
}


/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid) {
    block_signals();
    if (threads.find(tid) == threads.end()){
        unblock_signals();
        return error_handler(THREAD_ERROR, NO_THREAD_ID_ERROR, false);
    }
    Thread *thread = threads.at(tid);
    if(thread->get_state() == BLOCKED_STATE){
        blocked.erase(blocked.find(tid));
        thread->change_state_to_ready();
        if (thread->check_sleeping()){
            ready_queue.push_back(thread->get_thread_id());
        }
    }
    unblock_signals();
    return SUCCESS;
}


/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY queue.
 * If the thread which was just RUNNING should also be added to the READY queue, or if multiple threads wake up
 * at the same time, the order in which they're added to the end of the READY queue doesn't matter.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid == 0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep(int num_quantums) {
    int ret_val = sigsetjmp(env[running_thread], RETURN_VAL);
    block_signals();
    if (ret_val == FAILURE){
        return error_handler(SYSTEM_ERROR, ENVIRONMENT_SAVING_ERROR, true);
    } else if (ret_val == RETURN_VAL){
        return 0;
    }
    if (running_thread == MAIN_THREAD_ID){
        unblock_signals();
        return error_handler(THREAD_ERROR, SLEEP_MAIN_ERROR, false);
    }
    threads.at(running_thread)->set_sleeping(num_quantums);
    sleeping.insert({running_thread, threads.at(running_thread)});
    threads.at(running_thread)->change_state_to_ready();
    running_thread = NO_THREAD;
    set_timer_threads(cancel_timer);
    set_timer = true;
    make_scheduling_decision();
    return 0;
}


/**
* @brief Returns the thread ID of the calling thread.
* @return The ID of the calling thread.
*/
int uthread_get_tid() {
    return running_thread;
}


/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums() {
    return count_total_quantums;
}


/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid) {
    block_signals();
    if (threads.find(tid) == threads.end()){
        unblock_signals();
        return error_handler(THREAD_ERROR, NO_THREAD_ID_ERROR, false);
    }
    Thread *thread = threads.at(tid);
    unblock_signals();
    return thread->get_quantum();
}