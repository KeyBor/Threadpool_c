#pragma once

#include <queue>
#include <pthread.h>

struct Task{
    Task(){
    }

    Task(void (*function)(void* arg),void* arg){
        this->function = function;
        this->arg = arg;
    }

    void (*function)(void* arg);
    void* arg;
};

class TaskQueue{
public:
    TaskQueue();
    ~TaskQueue();
    void addTask(Task task);
    Task takeTask();
    inline int getTaskNum() {return m_taskQ.size();}

private:
    std::queue<Task> m_taskQ;
    pthread_mutex_t m_mutex;

};