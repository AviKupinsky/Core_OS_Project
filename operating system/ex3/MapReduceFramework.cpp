#include <atomic>
#include <iostream>
#include <cmath>
#include <deque>
#include <queue>
#include <algorithm>
#include "MapReduceFramework.h"
#include "pthread.h"
#include <semaphore.h>
#include "Barrier.h"
#include "Barrier.cpp"
using std::string;
using std::cerr;
using std::endl;
using std::pair;
using std::deque;
using std::atomic;
using std::vector;
using std::min;
using std::priority_queue;



#define SYSTEM_ERROR "System error: "
#define INIT_MUTEX_ERROR "failed to initialize the mutex."
#define INIT_SEMAPHORE_ERROR "failed to initialize the semaphore."
#define UNLOCK_MUTEX_ERROR "failed to unlock the mutex."
#define LOCK_MUTEX_ERROR "failed to lock the mutex."
#define DESTROY_MUTEX_ERROR "failed to destroy the mutex."
#define INIT_THREAD_ERROR "failed to initialize a thread."
#define JOIN_THREAD_ERROR "failed to join between threads."
#define SEMAPHORE_UP_ERROR "couldn't successfully add to semaphore."
#define SEMAPHORE_DOWN_ERROR "couldn't successfully decrease from semaphore."
#define SEMAPHORE_DESTROY_ERROR "couldn't successfully destroy the semaphore."
#define INITIALIZE 1
#define LOCK 2
#define INCREASE 2
#define UNLOCK 3
#define DECREASE 3
#define DESTROY 4
#define MAIN_T_ID 0
#define BITS_TODO_DONE 31
#define BITS_TODO_AND_DONE 62
#define HANDLE_PERCENTAGES 100
#define MASK_STAGE 0xC000000000000000
#define MASK_JOB_TO_D0 0x3FFFFFFF80000000
#define MASK_JOB_DONE 0x7FFFFFFF


/**
 * Prints the relevant error and exits the program with 1 (EXIT_FAILURE).
 * @param error - the error massage to be printed.
 */
void error_handler(const string& error){
    cerr << SYSTEM_ERROR << error << endl;
    exit(EXIT_FAILURE);
}


/**
 * A function that deals with all the scenarios regarding handling the mutex.
 * @param mutex - the mutex to operate on.
 * @param operation - a number that indicates what operation the handler should preform.
 */
void handling_mutex(pthread_mutex_t &mutex, int operation){
    switch (operation) {
        case INITIALIZE:
            if (pthread_mutex_init(&mutex, nullptr) != 0)
            {
                error_handler(INIT_MUTEX_ERROR);
            }
            break;
        case LOCK:
            if (pthread_mutex_lock(&mutex) != 0)
            {
                error_handler(LOCK_MUTEX_ERROR);
            }
            break;
        case UNLOCK:
            if (pthread_mutex_unlock(&mutex) != 0)
            {
                error_handler(UNLOCK_MUTEX_ERROR);
            }
            break;
        case DESTROY:
            if (pthread_mutex_destroy(&mutex) != 0)
            {
                error_handler(DESTROY_MUTEX_ERROR);
            }
            break;
    }
}

/**
 * A function that deals with all the scenarios regarding handling the semaphore.
 * @param semaphore - the semaphore to operate on.
 * @param operation - a number that indicates what operation the handler should preform.
 */
void handling_semaphore(sem_t &semaphore, int operation){
    switch (operation) {
        case INITIALIZE:
            if (sem_init(&semaphore, 0, 0)) {
                error_handler(INIT_SEMAPHORE_ERROR);
            }
            break;
        case INCREASE:
            if (sem_post(&semaphore)) {
                error_handler(SEMAPHORE_UP_ERROR);
            }
            break;
        case DECREASE:
            if (sem_wait(&semaphore)) {
                error_handler(SEMAPHORE_DOWN_ERROR);
            }
            break;
        case DESTROY:
            if(sem_destroy(&semaphore)){
                error_handler(SEMAPHORE_DESTROY_ERROR);
            }
            break;
    }
}


/**
 * A comparator between two pairs.
 * @param temp1 - the first pair of type pair<K2 *, V2 *> in the comparison.
 * @param temp2 - the second pair of type pair<K2 *, V2 *> in the comparison.
 * @return - true if the value of K2 (the key) of the first pair is larger then the value of K2 (the key) of the second
 * pair and false otherwise.
 */
bool compare_by_first_element(pair<K2 *, V2 *> temp1, pair<K2 *, V2 *> temp2){
    return (*temp1.first < *temp2.first);
}


struct pairs_k2_grater {
    bool operator()(pair<K2 *, V2 *> const a, pair<K2 *, V2 *> const b) const {
        return compare_by_first_element(b, a);
    }
};


/**
 * JobContext class declaration.
 */
class JobContext;


/**
 * A class that represents the thread context.
 */
class ThreadContext{
public:
    int id; // The thread's id number.
    JobContext *job; // Keeps all the details about the job.
    vector<IntermediateVec *> intermediate_pair_vec; // The vector that keeps the output of the map stage.


    /**
     * A constructor for the ThreadContext class.
     * @param id - the id of the thread.
     * @param job - the job that the thread executes.
     */
    ThreadContext(int id, JobContext *job)
    {
        this->id = id;
        this->job = job;
    }
};


/**
 * The class JobContest that stores all the details needed in order to run the algorithm using multiple threads.
 */
class JobContext{
public:
    const MapReduceClient * client; // The job client.
    const InputVec * inputVec; // The input vector of the algorithm.
    OutputVec * outputVec; // The output vector where we'll keep the results.
    int num_of_threads; // The number of threads we have for this job.
    unsigned long num_of_pairs; //The number of pairs in stage 2 and 3.
    JobState job_state{}; // An object that represents the job (_state and percentages).
    deque<pthread_t> threads; // All the treads that will preform the job.
    deque<ThreadContext> thread_context; // A deque that keeps the ThreadContext of each thread.
    std::atomic_uint64_t * progress; // An atomic counter for keeping track of the task progress for etch stage.
    Barrier *barrier; // The barrier used to ensure all threads had finished the mapping and sort stage.
    Barrier *barrier_reduce; // The barrier used to ensure the main thread had finished the shuffle stage.
    sem_t sem{}; // A semaphore for the 'shuffle' and 'reduce' stages.
    vector<IntermediateVec> vector_of_vectors; // Keeps all the vectors (results / inputs) of all the threads for each
    // stage.
    pthread_mutex_t Mutex_get_progress{}; // The mutex used for blocking other threads when running critical sections.
    pthread_mutex_t Mutex_set_atomic{}; // The mutex used for blocking other threads when running critical sections.
    pthread_mutex_t Mutex_map{}; // The mutex used for blocking other threads when running critical sections.
    pthread_mutex_t Mutex_sort{}; // The mutex used for blocking other threads when running critical sections.
    pthread_mutex_t Mutex_reduce{}; // The mutex used for blocking other threads when running critical sections.
    bool wait_for_job_bool; // Represents the state of the job (all the stages) - true if the job is finished and false
    // otherwise.
    bool finished_shuffle_stage; // Represents the state of the 'shuffle' stage - true if the shuffle stage is finished
    // and false otherwise.
    bool stage_job_to_do_set_map; // keeps track if atomic counter was initialized for the 'map' stage - true if was
    // initialized and false otherwise.
    bool done_map; // Represents the state of the map stage - true if the 'map' stage is finished and false otherwise.
    bool stage_job_to_do_set_shuffle; // keeps track if atomic counter was initialized for the 'shuffle' stage - true if
    // was initialized and false otherwise.
    bool stage_job_to_do_set_reduce; // keeps track if atomic counter was initialized for the 'reduce' stage - true if
    // was initialized and false otherwise.


    /**
     * A constructor for the JobContext.
     * @param client - the job client.
     * @param inputVec - the input vector of the algorithm.
     * @param outputVec - the output vector where we'll keep the results.
     * @param multiThreadLevel - the number of threads that will preform this job.
     */
    JobContext(const MapReduceClient* client,
               const InputVec& inputVec, OutputVec& outputVec,
               int multiThreadLevel){
        this->client= client;
        this->inputVec = &inputVec;
        this->outputVec = &outputVec;
        this->num_of_threads = min(multiThreadLevel, (int) inputVec.size());
        this->job_state.stage = UNDEFINED_STAGE;
        this->job_state.percentage = 0;
        this->num_of_pairs = 0;
        this->barrier = new Barrier(this->num_of_threads);
        this->barrier_reduce = new Barrier(this->num_of_threads);
        for (int i = 0; i < num_of_threads; ++i) {
            thread_context.emplace_back(i,this);
            threads.emplace_back();
        }
        this->progress = new std::atomic_uint64_t(0);
        handling_mutex(this->Mutex_get_progress, INITIALIZE);
        handling_mutex(this->Mutex_set_atomic, INITIALIZE);
        handling_mutex(this->Mutex_map, INITIALIZE);
        handling_mutex(this->Mutex_sort, INITIALIZE);
        handling_mutex(this->Mutex_reduce, INITIALIZE);
        this->wait_for_job_bool = false;
        this->finished_shuffle_stage = false;
        this->stage_job_to_do_set_map = false;
        this->stage_job_to_do_set_shuffle = false;
        this->stage_job_to_do_set_reduce = false;
        this->done_map = false;
        handling_semaphore(this->sem, INITIALIZE);
    }
};


/**
 * Initialize atomic counter for a given stage.
 * @param stage - the stage that sets the atomic counter.
 * @param context - job context / thread context.
 * @param value - value to set as the amount of job to be done (number of pairs to processes).
 */
void set_atomic(stage_t stage, void * context, unsigned long long value){
    JobContext * job_context;
    bool did_set;
    switch (stage) {
        case MAP_STAGE:
            job_context = ((ThreadContext *) context)->job;
            handling_mutex(job_context->Mutex_set_atomic, LOCK);
            did_set = job_context->stage_job_to_do_set_map;
            break;
        case REDUCE_STAGE:
            job_context = ((JobContext *) context);
            handling_mutex(job_context->Mutex_set_atomic, LOCK);
            did_set = job_context->stage_job_to_do_set_reduce;
            break;
        case SHUFFLE_STAGE:
            job_context = ((JobContext *) context);
            handling_mutex(job_context->Mutex_set_atomic, LOCK);
            did_set = job_context->stage_job_to_do_set_shuffle;
            break;
    }
    if (!did_set){
        unsigned long long temp = (((unsigned long long) stage) << BITS_TODO_AND_DONE) & MASK_STAGE;
        temp |= ((value << BITS_TODO_DONE) & MASK_JOB_TO_D0);
        (job_context->progress)->store(temp);
        switch (stage) {
            case MAP_STAGE:
                job_context->stage_job_to_do_set_map = true;
                break;
            case REDUCE_STAGE:
                job_context->stage_job_to_do_set_reduce = true;
                break;
            case SHUFFLE_STAGE:
                job_context->stage_job_to_do_set_shuffle = true;
                break;
        }
    }
    handling_mutex(job_context->Mutex_set_atomic, UNLOCK);
}


/**
 * Runs the map phase.
 * @param thread_context - the ThreadContext object representing the context of the running thread.
 */
void running_map_phase(void *thread_context){
    auto *tc = (ThreadContext *) thread_context;
    set_atomic(MAP_STAGE, tc, tc->job->inputVec->size());
    tc->intermediate_pair_vec.clear();
    // Using atomic counter in order not to have deadlock.
    while (!tc->job->done_map){
        // Mapping each pair in the input_vector:
        auto *temp_vec = new IntermediateVec;
        handling_mutex(tc->job->Mutex_map, LOCK);
        if (((*(tc->job->progress)) & MASK_JOB_DONE) + 1 > tc->job->inputVec->size()){
            tc->job->done_map = true;
            handling_mutex(tc->job->Mutex_map, UNLOCK);
            break;
        }
        unsigned long long index = (*(tc->job->progress))++ & MASK_JOB_DONE;
        handling_mutex(tc->job->Mutex_map, UNLOCK);
        tc->job->client->map(tc->job->inputVec->at(index).first, tc->job->inputVec->at(index).second, temp_vec);
        tc->intermediate_pair_vec.push_back(temp_vec);
    }
}


/**
 * The sort phase.
 * @param thread_context - the ThreadContext object representing the context of the running thread.
 */
void sort_phase(void *thread_context){
    auto* tc = (ThreadContext*) thread_context;
    // Sorting all the elements we got from the map phase:
    for (auto it: tc->intermediate_pair_vec){
        // Adding all the vectors into the vector of vector:
        handling_mutex(tc->job->Mutex_sort, LOCK);
        tc->job->vector_of_vectors.push_back(*it);
        handling_mutex(tc->job->Mutex_sort, UNLOCK);
        delete it;
    }
}


/**
 * Saves all the pairs with the same key to vector OF VECTORS.
 * @param this_pair - the current pair that is in the top of the min heap of pairs.
 * @param last_pair - the current pair that represents the pair with the smallest key seen this far.
 * @param saving_all_pairs - - a vector with all pairs with the same key.
 * @param job_context - the job context.
 */
void update_saving_all_pairs(pair<K2 *, V2 *> * this_pair, pair<K2 *, V2 *> * last_pair,
                             IntermediateVec * saving_all_pairs, JobContext * job_context){
    *last_pair = *this_pair;
    job_context->vector_of_vectors.push_back(*saving_all_pairs);
    saving_all_pairs->clear();
    handling_semaphore(job_context->sem, INCREASE);
}


/**
 * The shuffle phase.
 * @param job_context - A jobContext object representing the job context.
 */
void shuffle_phase(JobContext *job_context){
    vector<IntermediateVec> new_sequences;
    priority_queue<pair<K2 *, V2 *>, vector<pair<K2 *, V2 *>>, pairs_k2_grater> all_pairs;
    for (const auto &it: job_context->vector_of_vectors){
        if (it.empty()){
            continue;
        }
        job_context->num_of_pairs += it.size();
        for (auto pair : it){
            all_pairs.push(pair);
        }
    }
    set_atomic(SHUFFLE_STAGE, job_context, job_context->num_of_pairs);
    auto temp = job_context->vector_of_vectors;
    job_context->vector_of_vectors.clear();
    pair<K2 *, V2 *> last_pair = all_pairs.top();
    IntermediateVec saving_all_pairs;
    while (!all_pairs.empty()) {
        pair<K2 *, V2 *> this_pair = all_pairs.top();
        if (!compare_by_first_element(last_pair, this_pair) && !compare_by_first_element(this_pair, last_pair)){
            saving_all_pairs.push_back(this_pair);
            all_pairs.pop();
            (*(job_context->progress))++;
            if (all_pairs.empty()) {
                update_saving_all_pairs(&this_pair, &last_pair, &saving_all_pairs, job_context);
            }
        }
        else {
            update_saving_all_pairs(&this_pair, &last_pair, &saving_all_pairs, job_context);
        }
    }
    job_context->finished_shuffle_stage = true;
}


/**
 * The reduce phase.
 * @param job_context - A jobContext object representing the job context.
 */
void reduce_phase(JobContext *job_context){
    if (job_context->finished_shuffle_stage) {
        set_atomic(REDUCE_STAGE, job_context, job_context->num_of_pairs);
        while (!job_context->vector_of_vectors.empty()) {
            handling_semaphore(job_context->sem, DECREASE);
            if (job_context->vector_of_vectors.empty()){
                break;
            }
            handling_mutex(job_context->Mutex_reduce, LOCK);
            const IntermediateVec element_reduce = job_context->vector_of_vectors.front();
            job_context->vector_of_vectors.erase(job_context->vector_of_vectors.begin());
            job_context->client->reduce(&element_reduce, job_context);
            (*(job_context->progress))+= element_reduce.size();
            handling_mutex(job_context->Mutex_reduce, UNLOCK);
        }
        handling_semaphore(job_context->sem, INCREASE);
    }
}


/**
 * The function (entry point) that all the threads running the job execute.
 * @param thread_context - the ThreadContext object representing the context of the running thread.
 */
void *main_map_reduce_framework(void *thread_context){
    auto* tc = (ThreadContext*) thread_context;
    running_map_phase(tc);
    sort_phase(tc);
    tc->job->barrier->barrier();
    if (tc->id == MAIN_T_ID){
        shuffle_phase(tc->job);
    }
    tc->job->barrier_reduce->barrier();
    reduce_phase(tc->job);
    return nullptr;
}


/**
 * Produces Intermediate pair (K2*, V2*).
 * @param key It's type K2.
 * @param value It's type V2.
 * @param context the tread context.
 */
void emit2(K2 *key, V2 *value, void *context){
    IntermediatePair temp = IntermediatePair(key, value);
    auto *tc = (IntermediateVec *) context;
    tc->push_back(temp);
}

/**
 * Produces Intermediate pair (K3*, V3*).
 * @param key It's type K3.
 * @param value It's type V3.
 * @param context the tread context.
 */
void emit3(K3 *key, V3 *value, void *context)
{
    auto *job = (JobContext *) context;
    OutputPair temp = OutputPair(key, value);
    job->outputVec->push_back(temp);
}


/**
 *In this function we start the MapReduce algorithm.
 * @param client - the job client.
 * @param inputVec - the input vector of the algorithm.
 * @param outputVec - the output vector where we'll keep the results.
 * @param multiThreadLevel - the number of threads that will preform this job.
 * @return - a JobHandle object representing the job to preform.
 */
JobHandle startMapReduceJob(const MapReduceClient& client,
                            const InputVec& inputVec, OutputVec& outputVec,
                            int multiThreadLevel){
    auto *job = new JobContext(&client, inputVec, outputVec, multiThreadLevel);
    for (int i = 0; i < job->num_of_threads; ++i) {
        if (pthread_create(&job->threads.at(i), nullptr, main_map_reduce_framework, &job->thread_context[i])) {
            error_handler(INIT_THREAD_ERROR);
        }
    }
    return job;
}


/**
 * Ensures the if a thread had finished it's job it will wait for all the other threads to finish as well.
 * @param job - the JobHandle object representing the job that the algorithm is preforming.
 */
void waitForJob(JobHandle job){
    auto *new_job = (JobContext *) job;
    if (!new_job->wait_for_job_bool) {
        for (int i = 0; i < new_job->num_of_threads; ++i) {
            if (pthread_join(new_job->threads.at(i), nullptr)) {
                error_handler(JOIN_THREAD_ERROR);
            }
        }
    }
    new_job->wait_for_job_bool = true;
}


/**
 * Sets the jobState object.
 * @param job - the JobHandle object representing the job that the algorithm is preforming.
 * @param state - the state to load into the stage and percentage.
 */
void getJobState(JobHandle job, JobState *state){
    auto *new_job = (JobContext *) job;
    unsigned long long temp = (*(new_job->progress));
    unsigned long long done = temp & MASK_JOB_DONE;
    unsigned long long to_do = (temp & MASK_JOB_TO_D0) >> BITS_TODO_DONE;
    handling_mutex(new_job->Mutex_get_progress, LOCK);
    state->percentage = ((float) done / (float) to_do) * HANDLE_PERCENTAGES;
    state->stage = stage_t((temp & MASK_STAGE) >> BITS_TODO_AND_DONE);
    handling_mutex(new_job->Mutex_get_progress, UNLOCK);

}


/**
 * Closes the job and releases all Resources.
 * @param job - the JobHandle object representing the job that the algorithm is preforming.
 */
void closeJobHandle(JobHandle job){
    waitForJob(job);
    auto *new_job = (JobContext *) job;
    handling_semaphore(new_job->sem, DESTROY);
    handling_mutex(new_job->Mutex_get_progress, DESTROY);
    handling_mutex(new_job->Mutex_set_atomic, DESTROY);
    handling_mutex(new_job->Mutex_map, DESTROY);
    handling_mutex(new_job->Mutex_sort, DESTROY);
    handling_mutex(new_job->Mutex_reduce, DESTROY);
    delete (new_job->barrier);
    delete (new_job->barrier_reduce);
    delete new_job->progress;
    delete new_job;
}
