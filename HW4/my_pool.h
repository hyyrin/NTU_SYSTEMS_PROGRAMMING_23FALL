#include <pthread.h>
#ifndef __MY_THREAD_POOL_H
#define __MY_THREAD_POOL_H

typedef struct tpool_task {
    void *(*func)(void *);
    void *arg;
    struct tpool_task *next;
}tpool_task;

typedef struct tpool {
  // TODO: define your structure
    int n_threads;
    pthread_t *threads;
    tpool_task *task_queue;
    pthread_mutex_t queue_lock;
    pthread_cond_t queue_not_empty;
    pthread_cond_t queue_empty;
    int stop;
} tpool;

tpool *tpool_init(int n_threads);
void tpool_add(tpool *, void *(*func)(void *), void *);
void tpool_wait(tpool *);
void tpool_destroy(tpool *);
static void *tpool_worker(void *arg);
#endif

