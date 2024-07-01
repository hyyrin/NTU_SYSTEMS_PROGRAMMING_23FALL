#include "threadtools.h"
#include <sys/signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Print out the signal you received.
 * If SIGALRM is received, reset the alarm here.
 * This function should not return. Instead, call siglongjmp(sched_buf, 1).
 */
void sighandler(int signo) {
    // TODO
    if (signo == SIGALRM){
        alarm(timeslice);
        printf("caught SIGALRM\n");
        siglongjmp(sched_buf, 1);
    }
    else if (signo == SIGTSTP){
        printf("caught SIGTSTP\n");
        siglongjmp(sched_buf, 1);
    }
}



/*
 * Prior to calling this function, both SIGTSTP and SIGALRM should be blocked.
 */
void scheduler() {
    // TODO
    int rtv;
    //step 1
    if ((rtv = setjmp(sched_buf)) == 0){
        rq_current = 0;
        longjmp(RUNNING->environment, 1);
    }
    //step 2
    if (bank.lock_owner == -1 && wq_size > 0){
        ready_queue[rq_size] = waiting_queue[0];
        rq_size++;
        bank.lock_owner = waiting_queue[0]->id;
        for (int i = 0; i < wq_size - 1; i++)
            waiting_queue[i] = waiting_queue[i + 1];
        wq_size--;
    }
    //step 3
    if (rtv == 2){
        if (rq_current < (rq_size - 1)){
            waiting_queue[wq_size] = ready_queue[rq_current];
            wq_size++;
            ready_queue[rq_current] = ready_queue[rq_size - 1];
            rq_size--;
        }
        else if (rq_current == (rq_size - 1)){
            waiting_queue[wq_size] = ready_queue[rq_current];
            wq_size++;
            rq_current = 0;
            rq_size--;
        }
    }
    else if (rtv == 3){
        if (rq_current < (rq_size - 1)){
            struct tcb *tmp = ready_queue[rq_current];
            ready_queue[rq_current] = ready_queue[rq_size - 1];
            rq_size--;
            free(tmp);
        }
        else if (rq_current == (rq_size - 1)){
            struct tcb *tmp = ready_queue[rq_current];
            rq_current = 0;
            rq_size--;
            free(tmp);
        }
    }
    //step 4
    if (rtv == 1){
        rq_current++;
        if (rq_current >= rq_size)
            rq_current = 0;
        longjmp(RUNNING->environment, 1);
    }
    else if (rtv == 2)
        longjmp(RUNNING->environment, 1);
    else if (rtv == 3)
        longjmp(RUNNING->environment, 1);
    if (rq_size == 0 && wq_size == 0)
        return;
}

    
