#include "TaskQueue.h"    
    

void TaskQueue::addTask(Task task){
    pthread_mutex_lock(&m_mutex);
    m_taskQ.push(task);
    pthread_mutex_unlock(&m_mutex);
}

Task TaskQueue::takeTask(){
    Task t;
    pthread_mutex_lock(&m_mutex);
    t.function = m_taskQ.front().function;
    t.arg = m_taskQ.front().arg;
    m_taskQ.pop();
    pthread_mutex_unlock(&m_mutex);
    return t;
}

TaskQueue::TaskQueue()
{
    pthread_mutex_init(&m_mutex, NULL);
}

TaskQueue::~TaskQueue()
{
    pthread_mutex_destroy(&m_mutex);
}
