#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include "a2_helper.h"
#include <sys/sem.h>

#define SEM_TH_1 0
#define SEM_TH_4 1
#define SEM_TH_MAX_ALLOWED 2
#define SEM_PROC6_TH3 3
#define SEM_PROC8_TH4 4
#define PROCESS_2 2
#define PROCESS_6 6
#define PROCESS_8 8


int semaphore_id;

pthread_mutex_t lock;
pthread_cond_t cond_var;
pthread_cond_t cond_var2;
int flag=0;
int count=0;

//decrement by 1
void P(int sem_id, int semNumber){
    struct sembuf op = {semNumber,-1,0};
    semop(sem_id,&op,1);
}
//increment by 1
void V(int sem_id, int semNumber){
    struct sembuf op={semNumber,1,0};
    semop(sem_id,&op,1);
}

void *execP6Thread(void *arg){
    //Thread 4 must start before thread 1 and end after thread 1 ends
    int th_id = *((int *)arg);
    if(th_id==1){  //we put thread 1 on hold when we 1st encounter it
        P(semaphore_id,SEM_TH_1);
    }
    if(th_id==3){
        P(semaphore_id,SEM_PROC6_TH3);
    }
    info(BEGIN,PROCESS_6,th_id);
    if(th_id==4){  //after t4 starts, we start thread 1 again and pause thread 4
        V(semaphore_id,SEM_TH_1);
        P(semaphore_id,SEM_TH_4);
    }
    info(END,PROCESS_6,th_id);
    if(th_id==1){  //after thread 1 ends, we start again thread 4
        V(semaphore_id,SEM_TH_4);
    }
    if(th_id==3){
        V(semaphore_id,SEM_PROC8_TH4);
    }
    return NULL;
}

void *execP2Thread(void *arg){
    //Only 4 threads can run at the same time and thread 10 can terminate only when 4 threads are running
    int th_id = *((int *)arg);
    if(th_id==10){
        info(BEGIN,PROCESS_2,th_id);
        pthread_mutex_lock(&lock);
        while(count!=3){
            pthread_cond_wait(&cond_var,&lock);
        }
        flag=1;
        info(END,PROCESS_2,th_id);
        pthread_cond_broadcast(&cond_var2);
        pthread_mutex_unlock(&lock);
        return NULL;
    }
    P(semaphore_id,SEM_TH_MAX_ALLOWED);
    info(BEGIN,PROCESS_2,th_id);
    pthread_mutex_lock(&lock);
    count++;
    pthread_cond_signal(&cond_var);
    while(flag == 0){
        pthread_cond_wait(&cond_var2,&lock);
    }
    pthread_mutex_unlock(&lock);
    info(END,PROCESS_2,th_id);
    V(semaphore_id,SEM_TH_MAX_ALLOWED);
    return NULL;
}

void *execP8Thread(void *arg){
    int th_id = *((int *)arg);
    if(th_id==4){
        P(semaphore_id,SEM_PROC8_TH4);
    }
    info(BEGIN,PROCESS_8,th_id);
    info(END,PROCESS_8,th_id);
    if(th_id==3){
        V(semaphore_id,SEM_PROC6_TH3);
    }
    return NULL;
}

int main(){
    init();
    info(BEGIN, 1, 0);
    pid_t p2,p3,p4,p5,p6,p7,p8;
    int status;
    semaphore_id = semget(IPC_PRIVATE, 5, IPC_CREAT | 0600);
    semctl(semaphore_id,SEM_TH_1,SETVAL,0);
    semctl(semaphore_id,SEM_TH_4,SETVAL,0);
    semctl(semaphore_id,SEM_TH_MAX_ALLOWED,SETVAL,3);
    semctl(semaphore_id,SEM_PROC6_TH3,SETVAL,0);
    semctl(semaphore_id,SEM_PROC8_TH4,SETVAL,0);

    p2=fork();
    if(p2<0){
        perror("Error creating p2");
        exit(-1);
    }
    if(p2==0){
        info(BEGIN,2,0);
        //Start code for task 3
        int no_of_threads_task3=43;
        pthread_t th_task3[no_of_threads_task3];
        int th_args_task3[no_of_threads_task3];
        for(int i=1;i<=no_of_threads_task3;i++){
            th_args_task3[i]=i;
            if(pthread_create(&th_task3[i],NULL,execP2Thread,&th_args_task3[i])!=0){
                perror("Error creating threads in process 6");
                exit(-1);
            }
        }
        for(int i=1;i<=no_of_threads_task3;i++){
            pthread_join(th_task3[i],NULL);
        }
        //End code for task 3
        p4=fork();  //Create p4
        if(p4<0){
            perror("Error creating p4");
            exit(-1);
        }
        if(p4==0){
            info(BEGIN,4,0);
            info(END,4,0);
        }
        if(p4>0){
            waitpid(p2,&status,0);
            p6=fork();
            if(p6<0){
                perror("Error creating p6");
                exit(-1);
            }
            if(p6==0){
                info(BEGIN,6,0);
                //Start code for task 2
                int no_of_threads_task2=5;
                pthread_t th_task2[no_of_threads_task2];
                int th_args_task2[no_of_threads_task2];
                p8=fork();
                //important to have fork here so that the threads from p6 and p8 are intertwined and we can work with their flow (task 4)
                //this was the main problem I had and I spent a lot of time figuring this out
                //after this, the synchronisation is easy to do, I just used 2 semaphores
                if(p8!=0){
                     for(int i=1;i<=no_of_threads_task2;i++){
                         th_args_task2[i]=i;
                         if(pthread_create(&th_task2[i],NULL,execP6Thread,&th_args_task2[i])!=0){
                              perror("Error creating threads in process 6");
                              exit(-1);
                         }
                    }
                    for(int i=1;i<=no_of_threads_task2;i++){
                         pthread_join(th_task2[i],NULL);
                    }
                }
                //End code for task 2
                if(p8<0){
                    perror("Error creating p8");
                    exit(-1);
                }
                if(p8==0){
                    info(BEGIN,8,0);
                    //Start code for task 4
                    int no_of_threads_task4=5;
                    pthread_t th_task4[no_of_threads_task4];
                    int th_args_task4[no_of_threads_task4];
                    for(int i=1;i<=no_of_threads_task4;i++){
                        th_args_task4[i]=i;
                        if(pthread_create(&th_task4[i],NULL,execP8Thread,&th_args_task4[i])!=0){
                            perror("Error creating threads in process 8");
                            exit(-1);
                        }
                    }
                    for(int i=1;i<=no_of_threads_task4;i++){
                        pthread_join(th_task4[i],NULL);
                    }
                    //End code for task 4
                    info(END,8,0);
                }
                if(p8>0){
                    waitpid(p6,&status,0);
                    info(END,6,0);
                }

            }
            if(p6>0){
                waitpid(p4,&status,0);
                p7=fork();
                if(p7<0){
                    perror("Error creating p7");
                    exit(-1);
                }
                if(p7==0){
                    info(BEGIN,7,0);
                    info(END,7,0);
                }
                if(p7>0){
                    waitpid(p6,&status,0);
                    waitpid(p8,&status,0);
                    info(END,2,0);
                }
            }
        }
    }
    if(p2>0){
        p3=fork();
        if(p3<0){
            perror("Error creating p3");
            exit(-1);
        }
        if(p3==0){
            info(BEGIN,3,0);
            info(END,3,0);
        }
        if(p3>0){
            waitpid(p2,&status,0);
            p5=fork();
            if(p5<0){
                perror("Error creating p5");
                exit(-1);
            }
            if(p5==0){
                info(BEGIN,5,0);
                info(END,5,0);
            }
            if(p5>0){
                waitpid(p5,&status,0);
                waitpid(p3,&status,0);
                info(END, 1, 0);
            }
        }
    }
    return 0;
}
