#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<semaphore.h>
#include<sys/types.h>
#include<unistd.h>
#include<string.h>

/* The maximum number of customers that can visit the post office*/
#define customerThreads 50

/*The number of postal workers in the post office*/
#define postalWorkerThreads 3

#define errexit(code,str)                          \
  fprintf(stderr,"%s: %s\n",(str),strerror(code)); \
  exit(1);


/*Semaphores declaration*/
sem_t max_capacity;
sem_t mutex1,mutex2,mutex3;
sem_t cust_ready,postal_worker_ready;
sem_t finished[customerThreads];
sem_t scales_ready;
sem_t cust_order[customerThreads], processOrder[postalWorkerThreads];

/*Pipes for enqueue and dequeue*/

int taskPipe[2];
int customersPipe[2];
int postalWorkersPipe[2];

/*Function for sleep in milliseconds*/

void sleepMs(int ms){
	usleep(ms*1000);/*usleep is for sleep in microseconds*/
	return;
}

/*Function for printing the customer task*/

char* printTask(int task){
   switch(task){
        case 0: return "buy stamps\n";
                break;
        case 1: return "mail a letter\n";
                break;
        case 2: return "mail a package\n";
                break;
    }
    return "";
}

/*Function for printing that the customer completed his task*/

void printFinishTask(int task,int custID){

switch(task){
        case 0: printf("Customer %d finished buying stamps\n",custID);
                break;
        case 1: printf("Customer %d finished mailing a letter\n",custID);
                break;
        case 2: printf("Customer %d finished mailing a package\n",custID);
                break;
    }

}

/*Function for threads sleep. For every 60 seconds of the task in the task table, the thread sleeps 1 second*/

void threadSleep(int task,int workerId){
   switch(task){
	case 0: sleepMs(1000);/*Sleep for 1 second for 60 seconds as per the task table for buying stamps*/
                break;
        case 1: sleepMs(1500);/*Sleep for 1.5 seconds for 90 seconds as per the task table for mailing a letter*/
                break;
        case 2: sem_wait(&scales_ready);/* Wait for the scales if it is used by another postal worker*/
                   printf("Scales in use by postal worker %d\n",workerId);
                sleepMs(2000);/*Sleep for 2 seconds for 120 seconds as per the task table for mailing a package*/
		   printf("Scales released by postal worker %d\n",workerId);
                sem_post(&scales_ready);/*Release the scales when finished with its use*/
                break;
        }
}

/*Enqueue function for enqueue of tasks, customer ids and postal worker ids*/

void enqueue(int pipe,int data){

    switch(pipe){
    case 0:write(taskPipe[1],&data,sizeof(int));
           break;
    case 1:write(customersPipe[1],&data,sizeof(int));
           break;
    case 2:write(postalWorkersPipe[1],&data,sizeof(int));
           break;
    }
}

/*Dequeue function for dequeue of tasks, customer ids and postal worker ids*/

int dequeue(int pipe){
    int data;
    switch(pipe){
        case 0: read(taskPipe[0],&data,sizeof(int));
                break;
        case 1: read(customersPipe[0],&data,sizeof(int));
                break;
        case 2: read(postalWorkersPipe[0],&data,sizeof(int));
                break;
    }
    return data;
}

/*This is the customer thread code*/

void *customerThread(void *arg){
    int custID=*(int*)arg;/*Dereference the argument sent from the main which is the ID for the customer*/
    int task, workerID;/*Local variables for customer task and the worker who is serving the particular customer*/
    printf("Customer %d created\n",custID);/*Print that the customer thread is created*/
    task=rand()%3;/*Assign a random task*/
    sem_wait(&max_capacity);/*Wait on max_capacity semaphore to enter the office. That is the only 10 customers can be in office at any time*/
    printf("Customer %d enters post office\n",custID);/*If there are less than 10 customers, then enter the office*/
    sem_wait(&postal_worker_ready);/*Wait for the postal worker to be ready*/
    sem_wait(&mutex1);/*Protects the the customer ids queue*/
    enqueue(1,custID);/*Enqueue the customer IDs*/
    sem_post(&cust_ready);
    sem_post(&mutex1);
    sem_wait(&cust_order[custID]);/*Wait for the postal worker to to let the customer to place the order*/
    sem_wait(&mutex2);/*Protects worker IDs queue*/
    workerID=dequeue(2);/*Dequeue the particular postal worker who is servicing this particular customer*/
    sem_post(&mutex2);
    printf("Customer %d asks postal worker %d to %s",custID,workerID,printTask(task));/*Print the customer task*/
    sem_wait(&mutex3);/*Protects the customer tasks */
    enqueue(0,task);/*Enqueue the customer tasks*/
    sem_post(&processOrder[workerID]);/*Signal the postal worker to process the order*/
    sem_post(&mutex3);
    sem_wait(&finished[custID]);/*Wait for the postal worker to finish the task for the customer*/
    printFinishTask(task,custID);/*Print that the customer has finished his task when the postal worker signals it*/
    printf("Customer %d leaves the post office\n",custID);/*Leave the post office*/
    sem_post(&max_capacity);
    return arg;/*return the argument to main to join it*/
}

/*This is postal worker thread code*/

void *postalWorkerThread(void *arg){
    int workerId=*(int*)arg;/*Dereference the argument sent form main which is postal worker ID*/
    printf("Postal worker %d created\n",workerId);/*Print that the postal worker has been created*/
    while(1){
        int custId,task;/*Local variables for task number and customer ID*/
        sem_post(&postal_worker_ready);/*Signal the customer that the postal worker is ready for service*/
        sem_wait(&cust_ready);/*Wait for the customer */
        sem_wait(&mutex1);
        custId=dequeue(1);/*Dequeue the customer ID*/
        sem_post(&mutex1);
        printf("Postal worker %d serving customer %d\n",workerId,custId);/*Print that the postal worker has started the service for that customer*/
        sem_wait(&mutex2);
        enqueue(2,workerId);/*Enqueue the postal worker ID to let the customer know the ID of the postal worker for his reference*/
        sem_post(&cust_order[custId]);/*Ask for the customer for the his task*/
        sem_post(&mutex2);
        sem_wait(&processOrder[workerId]);/*Wait for the customer to make the postal worker proceed with the task*/
        sem_wait(&mutex3);
        task=dequeue(0);/*Dequeue the task number to service that particular task*/
        sem_post(&mutex3);
        threadSleep(task,workerId);/*Processing the task. That is sleeping as per the task table*/
        printf("Postal worker %d finished serving customer %d\n",workerId,custId);/*Print that the service is completed*/
        sem_post(&finished[custId]);/*Signal the particular customer that his task has been completed and he can leave the post office*/

    }
}

/*Main function code*/

int main(){

     int sem_count;
    //Initializing all the semaphores. The initial value is the third parameter in the sem_init function
    sem_init(&max_capacity,0,10);/*10 customers can be inside the post office*/
    sem_init(&mutex1,0,1);/*Protect customer IDs queue*/
    sem_init(&mutex2,0,1);/*Protect worker IDs queue*/
    sem_init(&mutex3,0,1);/*Protect tasks queue*/
    sem_init(&cust_ready,0,0);/*Postal worker needs to wait for the customer in the queue*/
    sem_init(&postal_worker_ready,0,0);/*Customer needs to wait for the next available postal worker*/
    for(sem_count=0;sem_count<customerThreads;sem_count++)/*Let the customer that his task was completed*/
        sem_init(&finished[sem_count],0,0);
    for(sem_count=0;sem_count<customerThreads;sem_count++)/*The postal worker waits to ask the customer for his task*/
        sem_init(&cust_order[sem_count],0,0);
    for(sem_count=0;sem_count<postalWorkerThreads;sem_count++)/*The customer tells the postal worker to process the order*/
        sem_init(&processOrder[sem_count],0,0);
    sem_init(&scales_ready,0,1);/*Resource for the scales. If it is in use by one postal worker, other should wait, if they need it*/

    /*If any of the pipe is not created then throw an error and exit*/
    if(pipe(taskPipe)==-1){
        printf("Unable to create pipe. Exiting\n");
        exit(1);
    }
    if(pipe(customersPipe)==-1){
        printf("Unable to create pipe. Exiting\n");
        exit(1);
    }
    if(pipe(postalWorkersPipe)==-1){
        printf("Unable to create pipe. Exiting\n");
        exit(1);
    }

    int cust_thread_count,worker_thread_count;/*Loop variables*/
    pthread_t custThreads[customerThreads]; /* Holds thread info for customer threads*/
    pthread_t workerThreads[postalWorkerThreads]; /* Holds thread info for postal worker threads*/
    int custIds[customerThreads]; /*Holds customer threads arguments*/
    int workerIds[postalWorkerThreads]; /*Holds postal worker threads arguments*/
    int errcode;         /*holds pthread error code*/
    int *status;        /*holds return code*/

    printf("\nSimulating Post Office with %d customers and %d postal workers\n\n",customerThreads,postalWorkerThreads);

/* create the postal worker threads */
   for (worker_thread_count=0; worker_thread_count<postalWorkerThreads; worker_thread_count++)
   {
      /* save thread number for this thread in array */
      workerIds[worker_thread_count]=worker_thread_count;

      /* create postal worker threads */
      if (errcode=pthread_create(&workerThreads[worker_thread_count],/* thread struct*/
			 NULL,/* default thread attributes */
			 postalWorkerThread,/* start routine*/
			 &workerIds[worker_thread_count])) {/* arg to routine*/
	errexit(errcode,"pthread_create");
      }
   }


/* create the customer threads */
   for (cust_thread_count=0; cust_thread_count<customerThreads; cust_thread_count++)
   {
      /* save thread number for this thread in array */
      custIds[cust_thread_count]=cust_thread_count;

      /* create customer threads */
      if (errcode=pthread_create(&custThreads[cust_thread_count],/*thread struct */
            NULL,/*default thread attributes*/
            customerThread,/*start routine*/
            &custIds[cust_thread_count])) {/*arg to routine*/
	        errexit(errcode,"pthread_create");
      }
   }


   /* join the customer threads as they exit */
   for (cust_thread_count=0; cust_thread_count<customerThreads; cust_thread_count++) {
      if (errcode=pthread_join(custThreads[cust_thread_count],(void **) &status)) {
         errexit(errcode,"pthread_join");
      }
      /* check thread's exit status, should be the same as the
	 thread number */



      if (*status != cust_thread_count) {
         fprintf(stderr,"Customer thread %d terminated abnormally\n",cust_thread_count);
         exit(1);
      }
      else
        printf("Joined customer %d\n",cust_thread_count); /*Print that the thread is joined*/

  }


       return 0;
}
