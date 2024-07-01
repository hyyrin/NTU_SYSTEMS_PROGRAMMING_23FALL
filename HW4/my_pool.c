#include "my_pool.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
void tpool_add(tpool *pool, void *(*func)(void *), void *arg) {
  // TODO
    tpool_task *new_task = (tpool_task *)malloc(sizeof(tpool_task));
    new_task->func = func;
    new_task->arg = arg;
    new_task->next = NULL;
    pthread_mutex_lock(&pool->queue_lock);
    if (pool->task_queue == NULL) {
        pool->task_queue = new_task;
        //pthread_cond_signal(&pool->queue_not_empty); //告知有新的job
        pthread_cond_broadcast(&pool->queue_not_empty); 
    }
    else { // 把新工作放進queue後面
        tpool_task *curr = pool->task_queue;
        while (curr->next != NULL)
            curr = curr->next;
        curr->next = new_task;
    }
    pthread_mutex_unlock(&pool->queue_lock);
}
void *tpool_worker(void *arg) {
    tpool *pool = (tpool *)arg;
    while (1) {
        pthread_mutex_lock(&pool->queue_lock);
        while (pool->task_queue == NULL && pool->stop == 0) //等job
            pthread_cond_wait(&pool->queue_not_empty, &pool->queue_lock);
        if (pool->stop) { //結束工作
            pthread_mutex_unlock(&pool->queue_lock);
            pthread_exit(NULL);
        }
        tpool_task *task = pool->task_queue; //取新工作
        pool->task_queue = task->next;
        pthread_mutex_unlock(&pool->queue_lock);
        task->func(task->arg); //執行任務
        free(task);
        pthread_mutex_lock(&pool->queue_lock);
        if (pool->task_queue == NULL)
            pthread_cond_signal(&pool->queue_empty);
        pthread_mutex_unlock(&pool->queue_lock);
    }
    return NULL;
}
void tpool_wait(tpool *pool) {
  // TODO
    pthread_mutex_lock(&pool->queue_lock);
    while (pool->task_queue != NULL)  //還有job
        pthread_cond_wait(&pool->queue_empty, &pool->queue_lock);
    pthread_mutex_unlock(&pool->queue_lock);
}

void tpool_destroy(tpool *pool) {
  // TODO
    pthread_mutex_lock(&pool->queue_lock);
    pool->stop = 1; //main thread要terminate
    pthread_cond_broadcast(&pool->queue_not_empty); // 確保叫醒然後terminate
    pthread_mutex_unlock(&pool->queue_lock);
    for (int i = 0; i < pool->n_threads; i++)
        pthread_join(pool->threads[i], NULL); // wait for all thread exit
    free(pool->threads);
    free(pool);
    pthread_mutex_destroy(&pool->queue_lock);
    pthread_cond_destroy(&pool->queue_not_empty);
    pthread_cond_destroy(&pool->queue_empty);
}

tpool *tpool_init(int n_threads) {
  // TODO
    tpool *pool = (tpool *)malloc(sizeof(tpool));
    pool->n_threads = n_threads;
    pool->threads = (pthread_t *)malloc(n_threads * sizeof(pthread_t));
    pool->task_queue = NULL;
    pool->stop = 0; // check main thread 是否要結束
    pthread_mutex_init(&pool->queue_lock, NULL);
    pthread_cond_init(&pool->queue_not_empty, NULL); // there still has jobs
    pthread_cond_init(&pool->queue_empty, NULL);  // there is no job
    for (int i = 0; i < n_threads; i++)
        pthread_create(&(pool->threads[i]), NULL, tpool_worker, (void *)pool);
    return pool;
}

