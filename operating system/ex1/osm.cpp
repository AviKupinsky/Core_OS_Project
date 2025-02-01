#include <iostream>
#include <sys/time.h>
#include "osm.h"
#include "cmath"


#define NUM_OF_LOOPS 5
#define ERROR -1
#define VALIDATE(e, ret) if(!(e)) return ret;
#define VALIDATE_RETURN(e, ret) if(e == -1) return ret;


/**
 * An empty function.
 */
void empty_func() {}


/**
 * General function for measuring time od different operations execution.
 * @param iterations the number of times to preform the operation.
 * @param check determines the operation to measure the execution time of.
 * @return time taken to preform the operation in nano-sec.
 */
double osm_measure_time(unsigned int iterations, int check) {
    VALIDATE(iterations,ERROR)
    int flag;
    long time, end;
    struct timeval currentTime{};
    int numOfIteration = ceil(double (iterations)/NUM_OF_LOOPS);
    flag = gettimeofday(&currentTime, nullptr);
    time = currentTime.tv_usec * 1000 + currentTime.tv_sec * long(1e9);
    VALIDATE_RETURN(flag,ERROR);
    switch (check) {
        case 1:
            int x, y, z, n, m;
            for (int i = 0; i < numOfIteration; i++) {
                x++;
                y++;
                z++;
                n++;
                m++;
            }
            break;
        case 2:
            for (int i = 0; i < numOfIteration; i++) {
                empty_func();
                empty_func();
                empty_func();
                empty_func();
                empty_func();
            }
            break;
        case 3:
            for (int i = 0; i < numOfIteration; i++) {
                OSM_NULLSYSCALL;
                OSM_NULLSYSCALL;
                OSM_NULLSYSCALL;
                OSM_NULLSYSCALL;
                OSM_NULLSYSCALL;
            }
            break;
    }
    flag = gettimeofday(&currentTime, nullptr);
    VALIDATE_RETURN(flag,ERROR)
    end = currentTime.tv_usec * 1000 + currentTime.tv_sec * long(1e9);
    return (double (end-time) / (numOfIteration*NUM_OF_LOOPS));
}


/**
 * time measurement of a simple arithmetic operation.
 * @param iterations number of time simple arithmetic execution.
 * @return average time of a simple arithmetic operation in nano-seconds upon success, -1 upon failure.
 */
double osm_operation_time(unsigned int iterations) {
    return osm_measure_time(iterations, 1);
}


/**
 * time measurement of a function call.
 * @param iterations number of times to call an empty function.
 * @return average time of a function call operation in nano-seconds upon success, -1 upon failure.
 */
double osm_function_time(unsigned int iterations) {
    return osm_measure_time(iterations, 2);
}


/**
 * time measurement of a trap function call.
 * @param iterations number of times to call a trap function.
 * @return average time of a trap function call operation in nano-seconds upon success, -1 upon failure.
 */
double osm_syscall_time(unsigned int iterations) {
    return osm_measure_time(iterations, 3);
}



