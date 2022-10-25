#include "threadpool.h"
#include "pthread.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
const int NUMBER = 2;
typedef struct Task
{
    void (*function)(void* arg);
    void* arg;
}Task;
typedef struct ThreadPool
{
    // 任务队列
    Task* taskQ;
    int queueCapacity;  // 容量
    int queueSize;      // 当前任务个数
    int queueFront;     // 队头 -> 取数据
    int queueRear;      // 队尾 -> 放数据
    pthread_t managerID;    // 管理者线程ID
    pthread_t *threadIDs;   // 工作的线程ID
    int minNum;             // 最小线程数量
    int maxNum;             // 最大线程数量
    int busyNum;            // 忙的线程的个数 由于busynum变化较快，对busy单独加锁
    int liveNum;            // 存活的线程的个数 between minNum,maxNum
    int exitNum;            // 要销毁的线程个数 to kill
    pthread_mutex_t mutexPool;  // 锁整个的线程池
    pthread_mutex_t mutexBusy;  // 锁busyNum变量 
    pthread_cond_t notFull;     // 任务队列是不是满了 生产者消费者模型，需要定义条件变量
    pthread_cond_t notEmpty;    // 任务队列是不是空了
    int shutdown;           // 是不是要销毁线程池, 销毁为1, 不销毁为0
}ThreadPool;
ThreadPool* threadPoolCreate(int min, int max, int queueSize){
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    do{
    if(pool == NULL)
    {
        printf("malloc threadpoll failed!");
        return NULL;
    }
    pool->threadIDs = (pthread_t*) malloc(sizeof(pthread_t) * max);
    if(pool->threadIDs == NULL){
        printf("malloc threadpoll failed!");
        free(pool);
        return NULL;
    }
    memset(pool->threadIDs, 0, sizeof(pool->threadIDs));
    pool->minNum = min;
    pool->maxNum = max;
    pool->busyNum = 0;
    pool->liveNum = min;
    pool->exitNum = 0;
    if(pthread_mutex_init(&pool->mutexPool,NULL) != 0 ||
        pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
        pthread_cond_init(&pool->notEmpty, NULL) != 0 ||
        pthread_cond_init(&pool->notFull, NULL) != 0)
        {
            printf("mutex or condition init fail...\n");
            break;
        }
        // 任务队列info
        pool->taskQ = (Task*)malloc(sizeof(Task) * queueSize);
        pool->queueCapacity = queueSize;
        pool->queueSize = 0;
        pool->queueFront = 0;
        pool->queueRear = 0;
        pool->shutdown = 0;
        // 创建manager线程
        pthread_create(&pool->managerID, NULL, manager, pool);
        // 创建worker线程
        for (int i = 0; i < min; ++i)
        {
            pthread_create(&pool->threadIDs[i], NULL, worker, pool);
        }
        return pool;
    } while (0); //do while(0):
    // 释放资源
    if (pool && pool->threadIDs) free(pool->threadIDs);
    if (pool && pool->taskQ) free(pool->taskQ);
    if (pool) free(pool);
    return NULL;
}

int threadPoolDestroy(ThreadPool* pool)
{
    if (pool == NULL)
    {
        return -1;
    }

    // 关闭线程池
    pool->shutdown = 1;
    // 阻塞回收管理者线程
    pthread_join(pool->managerID, NULL);
    // 唤醒阻塞的消费者线程
    for (int i = 0; i < pool->liveNum; ++i)
    {
        pthread_cond_signal(&pool->notEmpty);
    }
    // 释放堆内存
    if (pool->taskQ)
    {
        free(pool->taskQ);
    }
    if (pool->threadIDs)
    {
        free(pool->threadIDs);
    }

    pthread_mutex_destroy(&pool->mutexPool);
    pthread_mutex_destroy(&pool->mutexBusy);
    pthread_cond_destroy(&pool->notEmpty);
    pthread_cond_destroy(&pool->notFull);

    free(pool);
    pool = NULL;

    return 0;
}

int threadPoolBusyNum(ThreadPool* pool)
{
    pthread_mutex_lock(&pool->mutexBusy);
    int busyNum = pool->busyNum;
    pthread_mutex_unlock(&pool->mutexBusy);
    return busyNum;
}

int threadPoolAliveNum(ThreadPool* pool)
{
    pthread_mutex_lock(&pool->mutexPool);
    int aliveNum = pool->liveNum;
    pthread_mutex_unlock(&pool->mutexPool);
    return aliveNum;
}

void* worker(void* arg){
    ThreadPool* pool = (ThreadPool*) arg;
    while(1){
        pthread_mutex_lock(&pool->mutexPool); //访问线程池之前，给pool加锁，访问完了之后进行解锁
        while (pool->queueSize == 0 && !pool->shutdown)//说明已经没有任务需要处理了
        {
            pthread_cond_wait(&pool->notEmpty,&pool->mutexPool);//pthread_cond_wait函数有三个功能：
                                //1.阻塞线程 2.把附带的互斥锁打开 3.在此线程解除阻塞后，会自动把这把锁锁上 
            // 在worker里面判断当前是否要销毁该线程
            if(pool->exitNum > 0){
                pool->exitNum--;
                pool->liveNum--;
                //在此线程解除阻塞后，会自动把这把锁锁上
                pthread_mutex_unlock(&pool->mutexPool);
                threadExit(pool);
            }
        }
        if(pool->shutdown){
            pthread_mutex_unlock(&pool->mutexPool);
            threadExit(pool);
        }
        //任务队列也不为空，那么正式开始消费
        Task task;
        task.function = pool->taskQ[pool->queueFront].function;//维护一个环形队列，取出任务
        task.arg = pool->taskQ[pool->queueFront].function;
        pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
        -- pool->queueSize;
        pthread_mutex_unlock(&pool->mutexPool);//取任务完毕，对线程池进行解锁
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum++;
        pthread_mutex_lock(&pool->mutexBusy);
        task.function(task.arg);
        free(task.arg);
        task.arg=NULL;
        printf("The task is already processed!\n");
        pthread_mutex_lock(&pool->mutexBusy);
        --pool->busyNum;
        pthread_mutex_lock(&pool->mutexBusy);
    }
}
void* manager(void* arg){
    ThreadPool* pool = (ThreadPool*) arg;
    while (!pool->shutdown)
    {
        sleep(3);
        //取此线程池中任务的数量以及当前线程的数量
        pthread_mutex_lock(&pool->mutexPool);
        int queueSize = pool->queueSize;
        int liveNum = pool->liveNum;
        pthread_mutex_unlock(&pool->mutexPool);
        //然后取出当前在忙的线程的数量
        pthread_mutex_lock(&pool->mutexBusy);
        int busyNum = pool->busyNum;
        pthread_mutex_unlock(&pool->mutexBusy);
        //添加线程：任务个数>存活的线程数； 存活的线程数<最大线程数  
        if(queueSize > liveNum && pool->maxNum > liveNum){
            pthread_mutex_lock(&pool->mutexBusy);
            int counter = 0;                   
            for(int i=0; i<pool->maxNum && counter<NUMBER && liveNum < pool->maxNum; ++i){
                if(pool->threadIDs[i] == 0)
                {
                    pthread_create(&pool->threadIDs[i],NULL, worker, pool);
                    counter++;
                    pool->liveNum++;
                }
            }
            pthread_mutex_unlock(&pool->mutexBusy);
        }
        //销毁线程：当忙的线程*2<存活的线程数 && 存活的线程>最小线程数
        if(busyNum*2 < liveNum && liveNum > pool->minNum){
            pthread_mutex_lock(&pool->mutexPool);
            pool->exitNum = NUMBER; //通过signal前面的内容判断线程自杀的数量
            pthread_mutex_unlock(&pool->mutexPool);
            //让工作中的线程自杀
            for(int i=0; i<NUMBER; ++i){
                pthread_cond_signal(&pool->notEmpty); //唤醒阻塞在条件变量上的线程, 至少有一个被解除阻塞
            }
        }
    }   
}
void threadExit(ThreadPool* pool)
{
    pthread_t tid = pthread_self();
    for(int i=0; i<pool->maxNum; ++i){
        if(pool->threadIDs[i] == tid){
            pool->threadIDs[i] == 0;
            printf("threadExit() called, %ld exiting...", tid);
            break;
        }
    }
    pthread_exit(NULL);
}
void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg)
{
    pthread_mutex_lock(&pool->mutexPool);
    while (pool->queueSize == pool->queueCapacity && !pool->shutdown)
    {
        // 阻塞生产者线程
        pthread_cond_wait(&pool->notFull, &pool->mutexPool);
    }
    if (pool->shutdown)
    {
        pthread_mutex_unlock(&pool->mutexPool);
        return;
    }
    // 添加任务
    pool->taskQ[pool->queueRear].function = func;
    pool->taskQ[pool->queueRear].arg = arg;
    pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
    pool->queueSize++;

    pthread_cond_signal(&pool->notEmpty);
    pthread_mutex_unlock(&pool->mutexPool);
}
void taskFunc(void* arg)
{
    int num = *(int*)arg;
    printf("thread %ld is working, number = %d\n",
        pthread_self(), num);
    sleep(1);
}
int main()
{
    // 创建线程池
    ThreadPool* pool = threadPoolCreate(3, 10, 100);
    printf("Create Sucessfully!\n");
    for (int i = 0; i < 100; ++i)
    {
        printf("%d", i);
        int* num = (int*)malloc(sizeof(int));
        *num = i + 100;
        threadPoolAdd(pool, taskFunc, num);
    }

    sleep(3);

    threadPoolDestroy(pool);
    return 0;
}
