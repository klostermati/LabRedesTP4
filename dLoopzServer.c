#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h> 
#include <assert.h>
#include "dloopzServer.h"

WorkUnitId jobId = 1;

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

void QueuePut(Queue_t *pQ, WorkUnit_t *job){
    pthread_mutex_lock(&pQ->lock);
    while(pQ->putter - pQ->getter == MAX_ELEMENTS)
        pthread_cond_wait(&pQ->spaceCond, &pQ->lock);
    pQ->elements[pQ->putter++ % MAX_ELEMENTS] = job;
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


// ThreadFunctions ---------------------------------
void* fthreadGenerator(void *GeneratorObject){
    /* Function to be executed by a generator thread. Represents the life of a particular generator. */

    FakeWorkUnitGen_t *thisGenerator = (FakeWorkUnitGen_t *)GeneratorObject;

    for (int i=0;i<thisGenerator->genParams.life_time;i++){
        // Always check if this generator can run (if server didnt stop it) 
        _generatorTryRun(&thisGenerator->sem);

        // Create a new job request (with some logic select which task)
        // WorkUnit_t *JobRequest = workUnitCreate(some_task)

        // Complete job fields
    
        // Submit to server (updates must be done)


        // Simulate generation difficulty. Also gives time to stop generator if its needed
        _generatorUnlock(&thisGenerator->sem);
        sleep(thisGenerator->genParams.interval);        
    }

    // Use iterator to check errors
    return (void *)(i == thisGenerator->genParams.life_time);
}

void* fthreadWorker(void *WorkerObject){

    WorkerThread_t *thisWorker = (WorkerThread_t *)WorkerObject;


    return NULL;
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
    // Identify each worker ID (large and rare number) with an index from 0 to n_queues,
    // so then each worker can get and put jobs in their queues.
    
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
    else if (flag == 'p'){
        server->stats.jobs_inProgress++;
        server->stats.dead_time = job->stats.startProcTime - job->stats.submitTime + server->stats.dead_time;
    }

    // Job done
    else if (flag == 'd'){
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
    assert(newJob);

    newJob->id ++;
    newJob->fun = taskToDo;
    newJob->context = NULL;
    newJob->stats.submitTime = 0;
    newJob->stats.startProcTime = 0;
    newJob->stats.endProcTime = 0;

    return newJob;
}

void workUnitDestroy(WorkUnit_t *jobToDestroy){

    free(jobToDestroy);
}

void workUnitSubmit(WorkUnit_t *jobToSubmit, WorkServer_t *server){

    // Server type: 0 (many queues). Submit in a random one
    if (server->params.queue_type == 0){

        QueuePut(server->queue[ rand() % (server->params.n_workers + 1) ],jobToSubmit);
        jobToSubmit->stats.submitTime = time(NULL);
    }

    // Server type: 1 (unic queue)
    if (server->params.queue_type == 1){
        
        QueuePut(server->queue, jobToSubmit);
        jobToSubmit->stats.submitTime = time(NULL);
    }
    
    serverUpdateStats(server, jobToSubmit, 's');
}

void workUnitBegins(WorkUnit_t *jobBegins, WorkServer_t *server, pthread_t workerID){
    


}


// Generator object -------------------------------------------------------------

void _generatorTryRun(pthread_mutex_t *mutex){

    // Used by generator when attempts to produce a WorkUnit
    pthread_mutex_lock(mutex);
}

void _generatorUnlock(pthread_mutex_t *mutex){

    // Used by generator after producing one succesful WorkUnit
    pthread_mutex_unlock(mutex);
}
