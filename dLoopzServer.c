#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h> 
#include "dloopzServer.h"

// Queue functions -------------------------------------------------------------
void QueueInit(Queue_t *pQ){
    pQ->putter = pQ->getter = 0;
    pthread_mutex_init(&pQ->lock, NULL);
    pthread_cond_init(&pQ->spaceCond, NULL);
    pthread_cond_init(&pQ->valuesCond, NULL);
}

unsigned int QueueNumElements(Queue_t *pQ){
    int ret;
    pthread_mutex_lock(&pQ->lock);
    ret = pQ->putter - pQ->getter;
    pthread_mutex_unlock(&pQ->lock);
    return ret;
}

void QueuePut(Queue_t *pQ, int e){
    pthread_mutex_lock(&pQ->lock);
    while(pQ->putter - pQ->getter == MAX_ELEMENTS)
        pthread_cond_wait(&pQ->spaceCond, &pQ->lock);
    pQ->elements[pQ->putter++ % MAX_ELEMENTS] = e;
    pthread_cond_signal(&pQ->valuesCond);
    pthread_mutex_unlock(&pQ->lock);
}

int QueueGet(Queue_t *pQ, int e){
    int ret;
    pthread_mutex_lock(&pQ->lock);
    while(pQ->putter == pQ->getter)
        pthread_cond_wait(&pQ->valuesCond, &pQ->lock);
    ret = pQ->elements[pQ->getter++ % MAX_ELEMENTS];
    pthread_cond_signal(&pQ->spaceCond);
    pthread_mutex_unlock(&pQ->lock);
    return ret;
}

// Server functions -------------------------------------------------------------
WorkServer_t* serverInit(){
    /* Creates a new server with one queue by default */

    // Alloc memory
    WorkServer_t *newServer = malloc(sizeof(WorkServer_t));
    Queue_t *unicQueue = malloc(sizeof(Queue_t));

    // Type of queue management 
    newServer->queue = unicQueue;

    // Set parameters
    newServer->params.queue_type = 1;
    newServer->params.n_workers = 0;

    // Initiate stats
    newServer->stats.job_requests = 0;
    newServer->stats.jobs_done = 0;
    newServer->stats.jobs_inProgress = 0;
    newServer->stats.jobs_mean_time = 0;
    newServer->stats.largest_time = -1;
    newServer->stats.shortest_time = 3600;
    newServer->stats.total_time = 0;

    return newServer;
}

WorkServer_t* serverInit(unsigned int n_queues){
    /* Creates a new server with multiple queues (one for each worker to use) */
    
    // Alloc memory
    WorkServer_t *newServer = malloc(sizeof(WorkServer_t));
    Queue_t *queuesArray = malloc(n_queues*sizeof(Queue_t));

    // Type of queue management 
    newServer->queue = queuesArray;
    
    // Set parameters
    newServer->params.queue_type = 0;
    newServer->params.n_workers = n_queues;

    // Initiate stats
    newServer->stats.job_requests = 0;
    newServer->stats.jobs_done = 0;
    newServer->stats.jobs_inProgress = 0;
    newServer->stats.jobs_mean_time = 0;
    newServer->stats.largest_time = -1;
    newServer->stats.shortest_time = 3600;
    newServer->stats.total_time = 0;

    return newServer;
}

void serverDestroy(WorkServer_t *server){
    free(server->queue);
    free(server);
}

void serverUpdateParams(WorkServer_t *server, int n_workers){
    server->params.n_workers = n_workers;
}

void serverPrintParams(WorkServer_t *server){

    printf("******* Server information *******\n");
    if (server->params.queue_type == 1)
        printf("  Server type: Unic queue\n");
    if (server->params.queue_type == 0)
        printf("  Server type: Multiple queues\n");
    
    printf("  Number of worker threads: %d\n",server->params.n_workers);
    printf("**********************************")
}

void serverPrintStats(WorkServer_t *server){

    printf("******* Server stats *******\n");
    printf("  Number of job requests: %d",server->stats.job_requests);
    printf("  Number of jobs in done: %d",server->stats.jobs_done);
    printf("  Number of jobs in progress: %d",server->stats.jobs_inProgress);
    printf("  Jobs mean time: %d [s]",server->stats.jobs_mean_time);
    printf("  Jobs largest time: %d [s]",server->stats.largest_time);
    printf("  Jobs shortest time: %d [s]",server->stats.shortest_time);
    printf("  Jobs total time: %d [s]",server->stats.total_time);
    printf("****************************\n");
}

void serverUpdateStats(WorkServer_t *server, WorkUnit_t *job, char flag){

    // Job submitted 
    if (flag == 's'){
        server->stats.job_requests++;
    }
    
    // Job in progress
    if (flag == 'p'){
        server->stats.jobs_inProgress++;
        server->stats.dead_time = job->stats.startProcTime - job->stats.submitTime + server->stats.dead_time;
    }

    // Job done
    if (flag == 'd'){
        server->stats.jobs_done++;
        server->stats.jobs_inProgress--;
        server->stats.total_time = job->stats.endProcTime - job->stats.submitTime + server->stats.total_time;
        server->stats.jobs_mean_time = server->stats.total_time/server->stats.jobs_done;
        server->stats.mean_dead_time = server->stats.dead_time/server->stats.jobs_done;
        
        // Check if jobTime is the shortest or largest time ever
        if ((job->stats.endProcTime - job->stats.submitTime) < server->stats.shortest_time)
            server->stats.shortest_time = job->stats.endProcTime - job->stats.submitTime;
        
        if ((job->stats.endProcTime - job->stats.submitTime) > server->stats.largest_time)
            server->stats.largest_time = job->stats.endProcTime - job->stats.submitTime;
    }

    else {
        printf("Error in flag for serverUpdateStats. Must be s, p or d and get: %c\n",flag);
        exit(1);
    }
}

// WorkUnit functions -------------------------------------------------------------
WorkUnit_t* workUnitCreate(ProcFunc_t taskToDo){

    WorkUnit_t *newJob = malloc(sizeof(WorkUnit_t));
    


}

