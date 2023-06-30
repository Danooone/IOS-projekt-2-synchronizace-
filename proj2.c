#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <signal.h>


#define shmem_key 0x6543210 // id key for shared memory
#define WRITE_TO_FILE

#ifndef WRITE_TO_FILE
    #define fp stdout
#endif

/// Help message
const char *helpMessage =
    "Usage:\n"
    "   ./proj2 NZ NU TZ TU F\n"
    "where:\n"
    "   NZ - the number of customers (from 1 to 100)\n"
    "   NU - the number of postal employees (from 1 to 100)\n"
    "   TZ - the maximum time (in milliseconds) for which customers wait to enter the post office (from 0 to 1000)\n"
    "   TU - the maximum break time (in milliseconds) for postal employees (from 0 to 1000)\n"
    "   F  - the maximum time (in milliseconds) after which post office is closed (from 0 to 10000)\n"
    "\n"
    "All arguments have to be whole numbers or wrong format.\n";

/********************************
        SHARED MEMORY                                   
*******************************/ 

typedef struct {
    volatile int action;        // action counter
    volatile int total[3];      // total number of requests in each queue (letter, parcel, money)
    volatile int taken[3];      // number of taken requests in each queue
    volatile int done[3];       // number of completed requests in each queue
    volatile int totalAll;      // total number of requests in all queues
    volatile int takenAll;      // number of taken requests in all queues

    sem_t semPrint;             // semaphore used by all processes to prevent simultaneous writing into the .out file

    pthread_mutexattr_t psharedm;  // attributes for mutexes
    pthread_condattr_t psharedc;   // attributes for condition variables
    pthread_mutex_t mutAdd;        // mutex used for condition variable condAdd
    pthread_cond_t condAdd;        // conditional variable for waiting of request add
    pthread_mutex_t mutComplete[3];   // mutex used for condition variable condComplete
    pthread_cond_t condComplete[3];   // conditional variable for waiting of request completion

    atomic_int error;          // process creating error flag
    atomic_int closing;        // post office is closing
} sharedMem;


/********************************                             
        GLOBAL VARIABLES                                     
*******************************/ 

/// Process type
typedef enum  {
    procMain,
    procCustomer,
    procEmployee
} procTypes;

int NZ = 0;  // input argv[0]: number of customers
int NU = 0;  // input argv[1]: number of postal employees
int TZ = 0;  // input argv[2]: maximum time (in milliseconds) for which customers wait to enter the post office
int TU = 0;  // input argv[3]: maximum break time (in milliseconds) for postal employees
int F = 0;   // input argv[4]: maximum time (in milliseconds) after which post office is closed (from 0 to 10000)
pid_t* Z_ids = NULL;  // customer process identifiers
pid_t* U_ids = NULL;  // employee process identifiers
procTypes procType = procMain;  // process type
int procId = 0;  // process identifier

int shmemId = 0;  // shared memory identifier
sharedMem *shmem = NULL;  // shared memory segment pointed

#ifdef WRITE_TO_FILE
    FILE *fp = NULL;  // file pointer
#endif

/********************************
          FUNCTIONS                                        
*******************************/

/**
 *  argsCheck
 * -----------------------------------------------
 *  @brief: checks if input arguments meet required format
 *  
 *  @param argc: standard input argument count 
 *  @param argv: standard input argument values
 */

void argsCheck(int argc, char* argv[])
{
    char *next;
    if (argc != 6) { // test for number of arguments and floats/non-number characters
        fprintf(stderr,"%s", helpMessage);
        exit(1);
    }
    for (int i = 1; i < argc; ++i) { // process all arguments one-by-one
        strtol(argv[i], &next, 10); //get value of arguments, stopping when NoN encountered
        if ((next == argv[i]) || (*next != '\0')) { // check for empty string and characters left after conversion
            fprintf(stderr,"%s", helpMessage);
            exit(1);
        }
    }
}
/**
 *  argsLoad
 * -----------------------------------------------
 *  @brief: loads and stores arguments from standard input into pre-prepared global variables
 *  
 *  @param argv: standard input argument values
 */

void argsLoad(char* argv[])
{
    NZ = atoi(argv[1]);
    NU = atoi(argv[2]);
    TZ = atoi(argv[3]);
    TU = atoi(argv[4]);
    F = atoi(argv[5]);
    if ((NZ < 1)  || // 1 <= NZ 
        (NU < 1)  || // 1 <= NU 
        (TZ < 0 || TZ > 1000) || // 0 <= TZ <= 1000
        (TU < 0 || TU > 1000) || // 0 <= TU <= 1000
        (F  < 0 || F > 10000)) { // 0 <= F <= 10000
        fprintf(stderr,"%s", helpMessage); /// print help
        exit(1);
    }
}

/**
 *  waitAll
 * -----------------------------------------------
 *  @brief: wait for finish of all child processes
 */

void waitAll()
{
    int status;
    // Wait for customer processes
    if (Z_ids) {
        for (int i = 0; i < NZ; ++i) {
            if (Z_ids[i]) { waitpid(Z_ids[i], &status, 0); }
        }
    }
    // Wait for employee processes
    if (U_ids) {
        for (int i = 0; i < NU; ++i) {
            if (U_ids[i]) { waitpid(U_ids[i], &status, 0); }
        }
    }
}

/**
 *  freeAll
 * -----------------------------------------------
 *  @brief: frees semaphores, mutexes, condition variables, shared memory and close file
 */

void freeAll()
{
    // Free process identifiers
    if (Z_ids) { free(Z_ids); }
    if (U_ids) { free(U_ids); }
    
    // Free semaphore
    sem_destroy(&shmem->semPrint);

    // Free mutexes and condition variables
    pthread_mutex_destroy(&shmem->mutAdd);
//    pthread_cond_destroy(&shmem->condAdd);
    for (int i = 0; i < 3; ++i) {
        pthread_mutex_destroy(&shmem->mutComplete[i]);
        pthread_cond_destroy(&shmem->condComplete[i]);
    }
    pthread_mutexattr_destroy(&shmem->psharedm);
    pthread_condattr_destroy(&shmem->psharedc);

    // Free shared memory
    shmdt(shmem); // detaches shared memory
    shmctl(shmemId, IPC_RMID, NULL); // sets it to be deleted

#ifdef WRITE_TO_FILE
    fclose(fp);
#endif
}


/**
 *  init
 * -----------------------------------------------
 *  @brief: initializes semaphores, condition variables and counters in shared memory to wanted values
 */

void init()
{
    bool err = false;

    memset(shmem, 0, sizeof(sharedMem));

    if (sem_init(&shmem->semPrint, 1, 1) < 0) { err = true; }

    pthread_mutexattr_init(&shmem->psharedm);
    pthread_mutexattr_setpshared(&shmem->psharedm, PTHREAD_PROCESS_SHARED);  // make mutexes shareable between processes
    pthread_condattr_init(&shmem->psharedc);
    pthread_condattr_setpshared(&shmem->psharedc, PTHREAD_PROCESS_SHARED);  // make condition variables shareable between processes

    if (pthread_mutex_init(&shmem->mutAdd, &shmem->psharedm)) { err = true; }
//    if (pthread_cond_init(&shmem->condAdd, &shmem->psharedc)) { err = true; }
    for (int i = 0; i < 3; ++i) {
        if (pthread_mutex_init(&shmem->mutComplete[i], &shmem->psharedm)) { err = true; }
        if (pthread_cond_init(&shmem->condComplete[i], &shmem->psharedc)) { err = true; }
    }

    if (err == true){
        fprintf(stderr,"Error initializing semaphores, mutexes and/or condition variables\n");
        freeAll();  // free semaphores, mutexes, condition variables, shared memory and close file
        exit(1);
    }
}

/**
 *  mainFunc
 * -----------------------------------------------
 *  @brief: function for main processe
 */

void mainFunc()
{
    // Use current time as seed for random number generator
    srand(time(0));

    // Post office workday
    usleep((F/2 + rand() % (F-F/2+1)) * 1000);  // wait for closing
    sem_wait(&shmem->semPrint);
        fprintf(fp,"%d: closing\n", ++shmem->action);  // ++ is locked by semPrint semaphore
        fflush(fp);  // forces immediate writing
    sem_post(&shmem->semPrint);
    atomic_store_explicit(&shmem->closing, 1, memory_order_release);

    // Finish
    waitAll();  // wait for finish of all child processes
    freeAll();  // free semaphores, mutexes, condition variables, shared memory and close file
}

void customerFunc()
{
    // Use current time as seed for random number generator
    srand(time(0)+1000000+procId*1000);

    // Starting
    if (atomic_load_explicit(&shmem->error, memory_order_acquire)) { return; }
    sem_wait(&shmem->semPrint);
        fprintf(fp,"%d: Z %d: started\n", ++shmem->action, procId);  // ++ is locked by semPrint semaphore
        fflush(fp);  // forces immediate writing
    sem_post(&shmem->semPrint);

    // Wait before start
    if (atomic_load_explicit(&shmem->error, memory_order_acquire)) { return; }
    usleep(rand() % (TZ+1) * 1000);

    // Check if the post office is closed
    if (atomic_load_explicit(&shmem->error, memory_order_acquire)) { return; }
    if (atomic_load_explicit(&shmem->closing, memory_order_acquire)) {
        sem_wait(&shmem->semPrint);
            fprintf(fp,"%d: Z %d: going home\n", ++shmem->action, procId);  // ++ is locked by semPrint semaphore
            fflush(fp);  // forces immediate writing
        sem_post(&shmem->semPrint);
        return;
    }

    // Service request
    int service = rand() % 3;
    sem_wait(&shmem->semPrint);
        fprintf(fp,"%d: Z %d: entering office for a service %d\n", ++shmem->action, procId, service+1);  // ++ is locked by semPrint semaphore
        fflush(fp);  // forces immediate writing
    sem_post(&shmem->semPrint);
    pthread_mutex_lock(&shmem->mutAdd);
        int requestIndex = ++shmem->total[service];  // index of request in queue
        ++shmem->totalAll;
//        pthread_cond_signal(&shmem->condAdd);
    pthread_mutex_unlock(&shmem->mutAdd);

    // Wait for the service to finish processing
    if (atomic_load_explicit(&shmem->error, memory_order_acquire)) { return; }
    pthread_mutex_lock(&shmem->mutComplete[service]);
        while (!atomic_load_explicit(&shmem->error, memory_order_acquire) && shmem->done[service] < requestIndex) {
            pthread_cond_wait(&shmem->condComplete[service], &shmem->mutComplete[service]);  // waiting
        }
    pthread_mutex_unlock(&shmem->mutComplete[service]);

    if (atomic_load_explicit(&shmem->error, memory_order_acquire)) { return; }
    sem_wait(&shmem->semPrint);
        fprintf(fp,"%d: Z %d: called by office worker\n", ++shmem->action, procId);  // ++ is locked by semPrint semaphore
        fflush(fp);  // forces immediate writing
    sem_post(&shmem->semPrint);

    // Wait before going home
    if (atomic_load_explicit(&shmem->error, memory_order_acquire)) { return; }
    usleep(rand() % 10 * 1000);

    sem_wait(&shmem->semPrint);
        fprintf(fp,"%d: Z %d: going home\n", ++shmem->action, procId);  // ++ is locked by semPrint semaphore
        fflush(fp);  // forces immediate writing
    sem_post(&shmem->semPrint);
}


/**
 *  employeeFunc
 * -----------------------------------------------
 *  @brief: function for employee child processes
 */

void employeeFunc()
{
    // Use current time as seed for random number generator
    srand(time(0)+2000000+procId*1000);

    // Starting
    if (atomic_load_explicit(&shmem->error, memory_order_acquire)) { return; }
    sem_wait(&shmem->semPrint);
        fprintf(fp,"%d: U %d: started\n", ++shmem->action, procId);  // ++ is locked by semPrint semaphore
        fflush(fp);  // forces immediate writing
    sem_post(&shmem->semPrint);

    // Post office workday loop
    while (1) {
        if (atomic_load_explicit(&shmem->error, memory_order_acquire)) { return; }
        // Waiting for request
        pthread_mutex_lock(&shmem->mutAdd);
            int empty = (shmem->totalAll <= shmem->takenAll);
            int service;
            if (!empty) {
        //    while (!atomic_load_explicit(&shmem->error, memory_order_acquire) && shmem->totalAll <= shmem->takenAll) {
        //        pthread_cond_wait(&shmem->condAdd, &shmem->mutAdd);  // waiting
        //    }
                // Selecting for queue
                if (atomic_load_explicit(&shmem->error, memory_order_acquire)) { return; }
                do {
                    service = rand() % 3;
                } while (shmem->total[service] <= shmem->taken[service]);
                ++shmem->taken[service];
                ++shmem->takenAll;
            }
        pthread_mutex_unlock(&shmem->mutAdd);

        // Process the request
        if (atomic_load_explicit(&shmem->error, memory_order_acquire)) { return; }
        if (!empty) {
            sem_wait(&shmem->semPrint);
                fprintf(fp,"%d: U %d: serving a service of type %d\n", ++shmem->action, procId, service+1);  // ++ is locked by semPrint semaphore
                fflush(fp);  // forces immediate writing
            sem_post(&shmem->semPrint);
    
            // Wait before finish
            usleep(rand() % 10 * 1000);
    
            // Finish service
            if (atomic_load_explicit(&shmem->error, memory_order_acquire)) { return; }
            sem_wait(&shmem->semPrint);
                fprintf(fp,"%d: U %d: service finished\n", ++shmem->action, procId);  // ++ is locked by semPrint semaphore
                fflush(fp);  // forces immediate writing
            sem_post(&shmem->semPrint);
            pthread_mutex_lock(&shmem->mutComplete[service]);
                ++shmem->done[service];
                pthread_cond_signal(&shmem->condComplete[service]);
            pthread_mutex_unlock(&shmem->mutComplete[service]);
        } else {
            // Empty queue
            if (shmem->closing) { break; }  // post office is closed

            sem_wait(&shmem->semPrint);
                fprintf(fp,"%d: U %d: taking break\n", ++shmem->action, procId);  // ++ is locked by semPrint semaphore
                fflush(fp);  // forces immediate writing
            sem_post(&shmem->semPrint);
            usleep(rand() % (TU+1) * 1000);  // taking break
    
            if (atomic_load_explicit(&shmem->error, memory_order_acquire)) { return; }
            sem_wait(&shmem->semPrint);
                fprintf(fp,"%d: U %d: break finished\n", ++shmem->action, procId);  // ++ is locked by semPrint semaphore
                fflush(fp);  // forces immediate writing
            sem_post(&shmem->semPrint);
        }
    }

    // Going home
    sem_wait(&shmem->semPrint);
        fprintf(fp,"%d: U %d: going home\n", ++shmem->action, procId);  // ++ is locked by semPrint semaphore
        fflush(fp);  // forces immediate writing
    sem_post(&shmem->semPrint);
}

/**
 *  errorExit
 * -----------------------------------------------
 *  @brief: exit with error
 */

void errorExit()
{
    atomic_store_explicit(&shmem->error, 1, memory_order_release);
    for (int j = 0; j < 3; ++j) { pthread_cond_broadcast(&shmem->condComplete[j]); }
//    pthread_cond_broadcast(&shmem->condAdd);
    waitAll();  // wait for finish of all child processes
    freeAll();  // free semaphores, mutexes, condition variables, shared memory and close file
    exit(1);
}


/********************************
         MAIN                                       
*******************************/

int main(int argc, char* argv[])
{
    // Storing values from standard input. All parametres need to be given as whole numbers in their 
    // respective ranges, else the program prints the help message to standard error output and exits.
    argsCheck(argc, argv);
    // Values okay, load and save them
    argsLoad(argv);
    // Values read successfully, open file 'proj2.out' for writing
#ifdef WRITE_TO_FILE
    fp = fopen("proj2.out", "w");
#endif
    // Segment which initializes shared memory
    shmemId = shmget(shmem_key, sizeof(sharedMem), IPC_CREAT | 0644);
    if (shmemId < 0) {
        fprintf(stderr,"Shared memory error\n");
        exit(1);
    }
    shmem = (sharedMem*)shmat(shmemId, NULL, 0);
    // Initialize shared memory structure and create semaphores, mutexes, condition variables
    init();

    // Create child processes
    pid_t* Z_ids = (pid_t*)malloc(sizeof(pid_t)*NZ);
    pid_t* U_ids = (pid_t*)malloc(sizeof(pid_t)*NU);
    if (!Z_ids || !U_ids) {
        fprintf(stderr,"Memory allocation error\n");
        freeAll();  // free semaphores, mutexes, condition variables, shared memory and close file
        exit(1);
    }
    memset(Z_ids, 0, sizeof(pid_t)*NZ);
    memset(U_ids, 0, sizeof(pid_t)*NU);

    // Creating customer processes
    pid_t id;
    procType = procCustomer;
    procId = 0;
    for (int i = 0; i < NZ; ++i) {
        ++procId;
        id = fork();
        if (id == -1) {  // forking error occurred
            fprintf(stderr,"Fork error for customer process\n");
            errorExit();
        }
        if (id == 0) { break; }  // child process
        Z_ids[i] = id;
    }
    // Creating employee processes
    if (id > 0) {
        procType = procEmployee;
        procId = 0;
        for (int i = 0; i < NU; ++i) {
            ++procId;
            id = fork();
            if (id == -1) {  // forking error occurred
                fprintf(stderr,"Fork error for employee process\n");
                errorExit();
            }
            if (id == 0) { break; }  // child process
            U_ids[i] = id;
        }
    }
    if (id > 0) {
        procType = procMain;
    }

    // Process functions
    switch (procType) {
        case procMain:
            mainFunc();
            break;
        case procCustomer:
            customerFunc();
            break;
        case procEmployee:
            employeeFunc();
            break;
    }

    return 0;
}


