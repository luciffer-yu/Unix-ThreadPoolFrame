#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

typedef enum thread_status
{
    IDLE,
    WORKING
} state;

struct single_thread
{
    pthread_t tid;
    state     status;
    struct single_thread *next;
};

typedef struct single_thread thread;

struct thread_pool
{
    int              size;
    thread           threads;
    pthread_rwlock_t lock;
} ;

typedef struct thread_pool pool_t;

struct work_request
{
    int              priority;
    int              workid;
    void             *arg;
    struct work_request   *next;
    void *(*work_func)(void *arg);
} ;

typedef struct work_request work;

struct work_result
{
    int              workid;
    void             *value;
    struct work_result    *next;
};

typedef struct work_result result;

typedef struct thread_pool_manager
{
    work             workqueue;
    result           resqueue;
    pool_t           pool;
    pthread_rwlock_t worklock;
    pthread_rwlock_t reslock;
    sem_t           sem;
} pool_manager;

work* work_create(void *(*wfunc)(void *),void *arg);
void work_queue(work *w);
void work_dequeue(int workid);
void work_list();
void *work_thread();
result* result_create(void *value, int workid);
void result_queue(result *res);
result* result_dequeue();
void thread_status_update(pthread_t tid, int status);
thread* thread_create();
void thread_queue(thread *t);
void thread_dequeue(int cancel_thread_num);
void pool_create();
