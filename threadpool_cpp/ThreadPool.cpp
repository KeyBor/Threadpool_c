#include "ThreadPool.h"
#include "TaskQueue.h"
#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <string>

void ThreadPool::threadExit(){
    /**
     * @brief 该函数需要将线程退出，并且将threadIDs中的对应id置0
     * 
     */
    pthread_t tid = pthread_self();
    for(int i =0;i<maxNum;++i){
        if(threadIDs[i] == tid){
            threadIDs[i] = 0;
            printf("threadExit() function: thread %ld exiting...\n",pthread_self());
            break;
        }
    }
    pthread_exit(NULL);
}


void ThreadPool::addTask(Task task){
    if (shutdown)
    {
        return;
    }
    // 添加任务，不需要加锁，任务队列中有锁
    taskQ->addTask(task);
    // 唤醒工作的线程
    pthread_cond_signal(&not_empty);
}


ThreadPool::ThreadPool(int min,int max){
    do{
        //初始化最小线程数和最大线程数
        minNum = min;
        maxNum = max;
        liveNum = min;
        busyNum = 0;
        exitNum = 0;

        //初始化锁和条件变量
        if(pthread_mutex_init(&mutexpool,NULL) !=0 ||
            pthread_cond_init(&not_empty,NULL) !=0){

            printf("条件变量或互斥锁初始化失败!");
            break;
        }

        //初始化任务队列
        taskQ = new TaskQueue;
        if(taskQ == nullptr){
            std::cout<<"队列内存分配失败!\n";
            break;
        }

        //初始化管理者线程
        pthread_create(&managerID,NULL,manager,this);

        //初始化工作线程（消费者线程）
        threadIDs = new pthread_t[maxNum];
        if(threadIDs == nullptr){
            std::cout<<"工作线程内存分配失败!\n";
            break;
        }
        memset(threadIDs, 0, sizeof(pthread_t) * maxNum);
        for(int i =0;i<minNum;++i){
            pthread_create(&threadIDs[i],NULL,worker,this);
            std::cout << "创建子线程, ID: " << std::to_string(threadIDs[i]) << std::endl;
        }
        return;
    }while(0);
    
    if(taskQ == nullptr) delete taskQ;
    if(threadIDs == nullptr) delete[] threadIDs;
}

ThreadPool::~ThreadPool()
{
    shutdown = 1;
    // 销毁管理者线程
    pthread_join(managerID, NULL);
    // 唤醒所有消费者线程
    for (int i = 0; i < liveNum; ++i)
    {
        pthread_cond_signal(&not_empty);
    }

    if (taskQ) delete taskQ;
    if (threadIDs) delete[]threadIDs;
    pthread_mutex_destroy(&mutexpool);
    pthread_cond_destroy(&not_empty);
}


void* ThreadPool::worker(void* arg){
    //将传入的this指针，强制类型转换为ThreadPool类型
    ThreadPool* pool = static_cast<ThreadPool*>(arg);

    while(true){
        //判断任务队列是否为空，若为空，工作线程需要阻塞等待
        pthread_mutex_lock(&pool->mutexpool);
        while(pool->taskQ->getTaskNum() == 0 && !pool->shutdown){
            pthread_cond_wait(&pool->not_empty,&pool->mutexpool);

            if(pool->exitNum > 0 && !pool->shutdown){
                pool->exitNum--;
                pthread_mutex_unlock(&pool->mutexpool);
                pool->threadExit();
            }
        }

        //如果线程池被关闭，线程退出
        if(pool->shutdown){
            pthread_mutex_unlock(&pool->mutexpool);
            pool->threadExit();
        }

        Task task=pool->taskQ->takeTask();
        pool->busyNum++;
        printf("thread %ld start working....\n",pthread_self());
        pthread_mutex_unlock(&pool->mutexpool);

        task.function(task.arg);
        delete task.arg;
        task.arg = nullptr;

        pthread_mutex_lock(&pool->mutexpool);
        pool->busyNum--;
        printf("thread %ld end working....\n",pthread_self());
        pthread_mutex_unlock(&pool->mutexpool);
    }
   

    return nullptr;
  
}


void* ThreadPool::manager(void* arg){

    //将传入的this指针，强制类型转换为ThreadPool类型
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    while(!pool->shutdown){
        sleep(3);
         //创建线程
        /**
         * @brief 当任务数大于活着的线程数时并且活着的线程数小于最大线程数时，创建线程
         * 
         */
        pthread_mutex_lock(&pool->mutexpool);
        int livenum = pool->liveNum;
        if(pool->taskQ->getTaskNum()>livenum && livenum < pool->maxNum){
            int counter = 0;
            for(int i =0;i<pool->maxNum && pool->liveNum < pool->maxNum && counter<pool->NUMBER;++i){
                if(pool->threadIDs[i] == 0){
                    counter++;
                    pool->liveNum++;
                    pthread_create(&pool->threadIDs[i],NULL,worker,pool);
                    
                }
            }
        }
        pthread_mutex_unlock(&pool->mutexpool);
        //销毁线程
        /**
         * @brief 如果当忙的线程数*2 < 活着的线程数同时活着的线程数 > 最小线程数，销毁线程
         * 
         */
        pthread_mutex_lock(&pool->mutexpool);
        int busynum = pool->busyNum;
        if(busynum*2 < livenum && livenum >pool->minNum){
            pool->exitNum = NUMBER;
            for (int i = 0; i < NUMBER; ++i)
            {
                pthread_cond_signal(&pool->not_empty);
            }
        }
        pthread_mutex_unlock(&pool->mutexpool);
    }
    return nullptr;

}