#pragma once
#include <stdlib.h>
typedef struct ThreadPool ThreadPool;

//创建线程池并初始化
ThreadPool* threadPoolCreate(int min,int max,int queueSize);

//销毁线程池
int threadPoolDestroy(ThreadPool* pool);

//给线程池添加任务
void threadPoolAdd(ThreadPool* pool,void (*func)(void*),void* arg);

//当前线程池的工作线程数量
int threadPoolBusyNum(ThreadPool* pool);


//获取线程池中活着的线程个数
int threadPoolAliveNum(ThreadPool* pool);

//任务队列的消费者
void* worker(void* args);
void* manager(void* args);




