#pragma once

#include "TaskQueue.h"

class ThreadPool
{
private:
    int minNum;
    int maxNum;
    int liveNum;
    int busyNum;
    int exitNum;
    bool shutdown = false;
    static const int NUMBER =2;
    pthread_t managerID;
    pthread_t* threadIDs;
    TaskQueue* taskQ;

    pthread_cond_t not_empty;
    pthread_mutex_t mutexpool;

private:
    static void* manager(void* arg);
    static void* worker(void* arg);
    void threadExit();
    

public:
    ThreadPool(int min,int max);
    ~ThreadPool();
    void addTask(Task task);
    inline int getBusyNum(){
        pthread_mutex_lock(&mutexpool);
        int busynum = busyNum;
        pthread_mutex_unlock(&mutexpool);
        return busynum;
    }

    inline int getLiveNum(){
        pthread_mutex_lock(&mutexpool);
        int livenum = liveNum;
        pthread_mutex_unlock(&mutexpool);
        return livenum;
    }

};
