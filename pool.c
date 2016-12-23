#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "pool.h"

pool_manager manager;

work* work_create(void *(*wfunc)(void *),void *arg)
{
    struct timespec tp;

    clock_gettime(CLOCK_REALTIME, &tp);

    work *w = (work *)malloc(sizeof(work));

    memset(w, 0, sizeof(work));

    w->work_func = wfunc;
    w->arg = arg;

    w->workid = tp.tv_nsec%1000000;

    return w;
}

void work_queue(work *w)
{
    pthread_rwlock_wrlock(&manager.worklock);

    w->next = manager.workqueue.next;
    manager.workqueue.next = w;

    pthread_rwlock_unlock(&manager.worklock);
}

void work_dequeue(int workid)
{
    work *tmp, *prev;

    pthread_rwlock_wrlock(&manager.worklock);
    prev = &manager.workqueue;
    for(tmp = manager.workqueue.next; tmp; prev = tmp, tmp = tmp->next)
    {
        if(workid == tmp->workid)
        {
            prev->next = tmp->next;
            free(tmp);
            break;
        }
    }
    pthread_rwlock_unlock(&manager.worklock);
}

void work_list()
{
    work *w = NULL;

    pthread_rwlock_rdlock(&manager.worklock);

    w = manager.workqueue.next;
    printf("Wait to work in the queue list");
    while(w != NULL)
    {
        printf(" %d", w->workid);
        w = w->next;
    }
    printf("\n");

    pthread_rwlock_unlock(&manager.worklock);
}

void *work_thread()
{
    work *w = NULL;
    result *res = NULL;
    pthread_t tid;
    void *value;
    
    tid = pthread_self();

    while(1)
    {
        thread_status_update(tid, IDLE);

        sem_wait(&manager.sem);
        pthread_rwlock_wrlock(&manager.worklock);
        if((w = manager.workqueue.next) == NULL)
        {
            pthread_rwlock_unlock(&manager.worklock);
            continue;
        }

        pthread_rwlock_unlock(&manager.worklock);
        work_dequeue(w->workid);

        thread_status_update(tid, WORKING);
        printf("Thread %d start to doing work id %d request\n", tid, w->workid);

        value = (*(w->work_func))(w->arg);

        res = result_create(value, w->workid);

        result_queue(res);
    }
}

result* result_create(void *value, int workid)
{
    result *res = (result *)malloc(sizeof(result));

    memset(res, 0, sizeof(result));
    res->value = value;
    res->workid = workid;

    return res;
}

void result_queue(result *res)
{
    pthread_rwlock_wrlock(&manager.reslock);
    res->next = manager.resqueue.next;
    manager.resqueue.next = res;
    pthread_rwlock_unlock(&manager.reslock);
}

result* result_dequeue()
{
    result *tmp = NULL, *prev = NULL;

    pthread_rwlock_wrlock(&manager.reslock);
    if(tmp = manager.resqueue.next)
    {
        manager.resqueue.next = tmp->next;
        pthread_rwlock_unlock(&manager.reslock);
        return tmp;
    }
    pthread_rwlock_unlock(&manager.reslock);

    return NULL;
}

void thread_status_update(pthread_t tid, int status)
{
    thread *t = NULL;

    pthread_rwlock_wrlock(&manager.pool.lock);
    for(t = manager.pool.threads.next; t; t = t->next)
    {
        if(t->tid == tid)
        {
            t->status = status;
            break;
        }
    }
    pthread_rwlock_unlock(&manager.pool.lock);
}

thread* thread_create()
{
    pthread_t tid;

    thread *t = (thread *)malloc(sizeof(thread));
    memset(t, 0, sizeof(thread));

    if(pthread_create(&tid, NULL, work_thread, NULL))
    {
        printf("Create pthread error!\n");
        return NULL;
    }
    {
        printf("Thread %d has been created\n", tid);
        t->tid = tid;
        t->status = IDLE;
    }

    return t;
}

void thread_queue(thread *t)
{
    pthread_rwlock_wrlock(&manager.pool.lock);

    t->next = manager.pool.threads.next;
    manager.pool.threads.next = t;

    printf("Thread %d has been added to thread pool\n", t->tid);
    pthread_rwlock_unlock(&manager.pool.lock);
}

void thread_dequeue(int cancel_thread_num)
{
    thread *tmp = NULL, *prev = NULL;
    int num = 0;

    if(cancel_thread_num > manager.pool.size)
    {
        num = manager.pool.size;
    }
    else
    {
        num = cancel_thread_num;
    }
    pthread_rwlock_wrlock(&manager.pool.lock);
    prev = &manager.pool.threads;

    for(tmp = manager.pool.threads.next; tmp; prev = tmp, tmp = tmp->next) 
    {
        if(tmp->status != WORKING)
        {
            if(!pthread_cancel(tmp->tid))
            {
                prev->next = tmp->next;
                free(tmp);
                if(!--num)
                {
                    break;
                }
            }
        }
    }

    pthread_rwlock_unlock(&manager.pool.lock);
}

void pool_create()
{
    int i;
    thread *p = NULL;

    for(i = 0; i< manager.pool.size; i++)
    {
        if((p = thread_create()) != NULL)
        {
            thread_queue(p);
        }
    }
}

int pool_init(int pool_size)
{
    memset(&manager, 0, sizeof(manager));

    pthread_rwlock_init(&manager.pool.lock, NULL);
    pthread_rwlock_init(&manager.worklock, NULL);
    pthread_rwlock_init(&manager.reslock, NULL);

    sem_init(&manager.sem, 0, 0);

    manager.pool.size = pool_size;

    pool_create();
}

void *function(void *a)
{
    int* i = (int *)malloc(4);

    memset(i, 0, 4);
    for(; *i < *(int *)a; (*i)++)
    {
        sleep(1);
    }

    return (void *)i;
}

int main()
{
    int a[10] = {10,11,12,13,14,15,16,17,18,19};
    int i = 0;
    int cnt = 0;
    work *w = NULL;
    result *r = NULL;

    pool_init(5);
    for(; i < 10; i++)
    {
        w = work_create(function, (void *)&a[i]);
        work_queue(w);
        sem_post(&manager.sem);
    }
    while(1)
    {
        cnt++;
        work_list();
        r = result_dequeue();
        if(!r)
        {
            printf("No result dequeue!\n");
        }
        else
        {
            printf("Dequeue a result workid %d result %d !\n", r->workid, *((int*)(r->value)));
            free(r->value);
            free(r);
        }
        if(cnt == 50)
        {
            i = 0;
            thread_dequeue(4);
            for(; i < 12; i++)
            {
                w = work_create(function, (void *)&a[i]);
                work_queue(w);
                sem_post(&manager.sem);
            }
        }
        sleep(1);
    }
}
