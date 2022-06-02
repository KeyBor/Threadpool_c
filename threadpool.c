#include "threadpool.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


const int NUMBER =2;

//任务结构体
typedef struct Task
{
    void (*function)(void* arg);
    void* arg;
}Task;

//线程池结构体
typedef struct ThreadPool
{
    //任务队列
    Task* taskQ;
    int queueCapacity;              //容量
    int queueSize;                  //当前任务个数
    int queueFront;                 //队头->取数据
    int queueRear;                  //队尾->放数据

    pthread_t managerID;            //管理者线程
    pthread_t *threadIDs;           //工作线程，消费者线程

    int minNum;                     //最小线程数
    int maxNum;                     //最大线程数量
    int busyNum;                    //忙的线程的个数
    int liveNum;                    //存活的线程的个数
    int exitNuM;                    //要销毁的线程个数
    pthread_mutex_t mutexPool;      //锁整个的线程池
    pthread_mutex_t mutexBusy;      //锁busyNum的线程池
    pthread_cond_t  notFull;        //任务队列是不是满了
    pthread_cond_t  notEmpty;       //任务队列是不是空了
    int shutdown;                   //是否要销毁线程池，销毁为1，不销毁为0
    
};

ThreadPool* threadPoolCreate(int min,int max,int queueSize){
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    do{
        if(pool == NULL){
            printf("malloc threadpool memory failed!\n");
            break;
        }

        pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t)* max);
        if(pool->threadIDs== NULL){
            printf("malloc threadIDs memory failed!\n");
            break;
        }

        memset(pool->threadIDs,0,sizeof(pthread_t)*max);
        pool->minNum = min;
        pool->maxNum = max;
        pool->busyNum = 0;
        pool->liveNum = min;
        pool->exitNuM = 0;

        if(pthread_mutex_init(&pool->mutexPool,NULL) !=0 ||
        pthread_mutex_init(&pool->mutexBusy,NULL) !=0 ||
        pthread_cond_init(&pool->notEmpty,NULL)   !=0 ||
        pthread_cond_init(&pool->notFull,NULL)    !=0){

            printf("mutex or cond");
            break;
        }


        //任务队列
        pool->taskQ = (Task*)(malloc(sizeof(Task)*queueSize));
        pool->queueCapacity = queueSize;
        pool->queueSize = 0;
        pool->queueFront = 0;
        pool->queueRear = 0;

        pool->shutdown = 0;

        //创建线程
        pthread_create(&pool->managerID,NULL,manager,pool);
        for(int i =0;i<min;++i){
            pthread_create(&pool->threadIDs[i],NULL,worker,pool);
        }
        return pool;
    }while(0);

    if(pool&&pool->threadIDs) free(pool->threadIDs);
    if(pool&&pool->taskQ)     free(pool->taskQ);
    if(pool)                  free(pool);
    return NULL;
}

void* worker(void* args){
    ThreadPool* pool = (ThreadPool*)args;

    while(1){
        pthread_mutex_lock(&pool->mutexPool);
        //当前任务队列是否为空
        while(pool->queueSize ==0 && !pool->shutdown){
            //阻塞工作线程
            pthread_cond_wait(&pool->notEmpty,&pool->mutexPool);

            if(pool->exitNuM >0){
                pool->exitNuM--;
                if(pool->liveNum > pool->minNum){
                    pool->liveNum--;
                    pthread_mutex_unlock(&pool->mutexPool);
                    threadExit(pool);
                }
            }
        }
        if(pool->shutdown){
            pthread_mutex_unlock(&pool->mutexPool);
            threadExit(pool);
        }

        //取队首元素
        Task task;
        task.function = pool->taskQ[pool->queueFront].function;
        task.arg = pool->taskQ[pool->queueFront].arg;
        pool->queueSize--;
        pool->queueFront = (pool->queueFront+1) % pool->queueCapacity;
        pthread_cond_signal(&pool->notFull);
        pthread_mutex_unlock(&pool->mutexPool);
        //执行任务

        printf("thread %ld start working....\n",pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum++;
        pthread_mutex_unlock(&pool->mutexBusy);

        task.function(task.arg);
        free(task.arg);
        task.arg= NULL;
        
        printf("thread %ld end working....\n",pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum--;
        pthread_mutex_unlock(&pool->mutexBusy);
    }
    return NULL;
}


void* manager(void* args){
    ThreadPool* pool = (ThreadPool*)args;
    while(!pool->shutdown){
        sleep(3);

        //获取当前线程池中的busynum和livenum
        pthread_mutex_lock(&pool->mutexPool);
        int queueSize = pool->queueSize;
        int liveNum = pool->liveNum;
        pthread_mutex_unlock(&pool->mutexPool);

        pthread_mutex_lock(&pool->mutexBusy);
        int busyNum = pool->busyNum;
        pthread_mutex_unlock(&pool->mutexBusy);


        //创建线程
        //任务个数 > 存活的线程数  && 存活的线程数<最大线程数
        if(queueSize > liveNum && liveNum< pool->maxNum){
            pthread_mutex_lock(&pool->mutexPool);
            int counter =0;
            for(int i =0; i<pool->maxNum && counter< NUMBER && pool->liveNum < pool->maxNum;++i){
                if(pool->threadIDs[i] == 0){
                    pthread_create(&pool->threadIDs[i],NULL,worker,pool);
                    pool->liveNum++;
                    counter++;
                }

            }
            pthread_mutex_unlock(&pool->mutexPool);
        }

        //销毁线程
        //当工作的线程数*2 <存活的线程数 && 存活的线程数>最小线程数
        if(busyNum*2 < liveNum && liveNum > pool->minNum){
            pthread_mutex_lock(&pool->mutexPool);
            pool->exitNuM = NUMBER;
            pthread_mutex_unlock(&pool->mutexPool);
            //让工作的线程自杀
            for(int i=0;i<NUMBER;++i){
                pthread_cond_signal(&pool->notEmpty);
            }
        }
    }
    return NULL;
}


void threadExit(ThreadPool* pool){
    pthread_t tid = pthread_self();
    for(int i =0 ; i< pool->maxNum;++i){
        if(pool->threadIDs[i] == tid){
            pool->threadIDs[i] = 0;
            printf("threadExit() called,%ld exiting...\n",tid);
            break;
        }
    }
    pthread_exit(NULL);
}


void threadPoolAdd(ThreadPool* pool,void (*func)(void*),void* arg){
    pthread_mutex_lock(&pool->mutexPool);
    while(pool->queueSize == pool->queueCapacity && !pool->shutdown ){
        //阻塞生产者线程
        pthread_cond_wait(&pool->notFull,&pool->mutexPool);

    }

    if(pool->shutdown){
        pthread_mutex_unlock(&pool->mutexPool);
        threadExit(NULL);
    }
    //添加任务
    pool->taskQ[pool->queueRear].function = func;
    pool->taskQ[pool->queueRear].arg = arg;
    pool->queueRear = (pool->queueRear+1)%pool->queueCapacity;
    pool->queueSize++;

    pthread_cond_signal(&pool->notEmpty);

    pthread_mutex_unlock(&pool->mutexPool);

}


//当前线程池的工作线程数量
int threadPoolBusyNum(ThreadPool* pool){
    pthread_mutex_lock(&pool->mutexBusy);
    int busyNum = pool->busyNum;
    pthread_mutex_unlock(&pool->mutexBusy);
    return busyNum;
}


//获取线程池中活着的线程个数
int threadPoolAliveNum(ThreadPool* pool){
    pthread_mutex_lock(&pool->mutexPool);
    int liveNum = pool->liveNum;
    pthread_mutex_unlock(&pool->mutexPool);
    return liveNum;
}   

int threadPoolDestroy(ThreadPool* pool){
    if(pool == NULL){
        return -1;
    }
    //关闭线程池
    pool->shutdown = 1;
    //阻塞回收管理者线程
    pthread_join(pool->managerID,NULL);
    //唤醒阻塞的消费者线程
    for(int i =0;i<pool->liveNum;++i){
        pthread_cond_signal(&pool->notEmpty);
    }
    //释放堆内存
    if(pool->taskQ){
        free(pool->taskQ);
    }

    if(pool->threadIDs){
        free(pool->threadIDs);
    }
    pthread_mutex_destroy(&pool->mutexPool);
    pthread_mutex_destroy(&pool->mutexBusy);
    pthread_cond_destroy(&pool->notEmpty);
    pthread_cond_destroy(&pool->notFull);

    free(pool);
    pool=NULL;



}