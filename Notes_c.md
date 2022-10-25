## 线程池的原理
如果并发的线程数量很多，并且每个线程都是执行一个时间很短的任务就结束了，这样频繁创建线程就会大大降低系统的效率，因为频繁创建线程和销毁线程需要时间。  
那么有没有一种办法使得线程可以**复用**，就是执行完一个任务，并不被销毁，而是可以继续执行其他的任务呢？  
线程池是一种多线程处理形式，处理过程中将任务添加到队列，然后在创建线程后自动启动这些任务。线程池线程都是后台线程。每个线程都使用默认的堆栈大小，以默认的优先级运行，并处于多线程单元中。如果某个线程在托管代码中空闲（如正在等待某个事件）, 则线程池将插入另一个辅助线程来使所有处理器保持繁忙。如果所有线程池线程都始终保持繁忙，但队列中包含挂起的工作，则线程池将在一段时间后创建另一个辅助线程但线程的数目永远不会超过最大值。超过最大值的线程可以排队，但他们要等到其他线程完成后才启动。  
在各个编程语言的语种中都有线程池的概念，并且很多语言中直接提供了线程池，作为程序猿直接使用就可以了，下面给大家介绍一下线程池的实现原理：  

线程池的组成主要分为 3 个部分，这三部分配合工作就可以得到一个完整的线程池：  
1.任务队列，存储需要处理的任务，由工作的线程来处理这些任务（生产者消费者模型）  
    生产者将一个待处理的任务添加到任务队列，消费者线程取出任务并处理任务  
    已处理的任务会被从任务队列中删除  
    线程池的使用者，也就是调用线程池函数往任务队列中添加任务的线程就是生产者线程  
    消费者线程通过一个循环不停地取任务，如果任务队列为空，则通过条件变量对线程进行阻塞，则放弃了CPU资源。
2.工作的线程（任务队列任务的消费者） ，N个  
    线程池中维护了一定数量的消费者线程，他们的作用是是不停的读任务队列，从里边取出任务并处理  
    工作的线程相当于是任务队列的消费者角色，  
    如果任务队列为空，工作的线程将会被阻塞 (使用条件变量 / 信号量阻塞)  
    如果阻塞之后有了新的任务，由生产者将阻塞解除，工作线程开始工作  
3.管理者线程（不处理任务队列中的任务），1个（工头）
    它的任务是周期性（通过sleep）的对**任务队列中的任务数量**以及**处于忙状态的工作线程个数**进行检测  
	当任务过多的时候，可以适当的创建一些新的工作线程  
	当任务过少的时候，可以适当的销毁一些工作的线程  
##### 题外话：python多线程性能不行的原因
GIL，中文译为全局解释器锁。在讲解 GIL 之前，首先通过一个例子来直观感受一下 GIL 在 Python 多线程程序运行的影响。
首先运行如下程序：
```python
    import time
    start = time.clock()
    def CountDown(n):
        while n > 0:
            n -= 1
    CountDown(100000)
    print("Time used:",(time.clock() - start))
```
运行结果为：
Time used: 0.0039529000000000005
在我们的印象中，使用多个（适量）线程是可以加快程序运行效率的，因此可以尝试将上面程序改成如下方式：
```python
    import time
    from threading import Thread
    start = time.clock()
    def CountDown(n):
        while n > 0:
            n -= 1
    t1 = Thread(target=CountDown, args=[100000 // 2])
    t2 = Thread(target=CountDown, args=[100000 // 2])
    t1.start()
    t2.start()
    t1.join()
    t2.join()
    print("Time used:",(time.clock() - start))
```
运行结果为：
Time used: 0.006673
可以看到，此程序中使用了 2 个线程来执行和上面代码相同的工作，但从输出结果中可以看到，运行效率非但没有提高，反而降低了。
    如果使用更多线程进行尝试，会发现其运行效率和 2 个线程效率几乎一样（本机器测试使用 4 个线程，其执行效率约为 0.005）。这里不再给出具体测试代码，有兴趣的读者可自行测试。事实上，得到这样的结果是肯定的，因为 GIL 限制了 Python 多线程的性能不会像我们预期的那样。  
那么，什么是 GIL 呢？GIL 是最流程的 CPython 解释器（平常称为 Python）中的一个技术术语，中文译为全局解释器锁，其本质上类似操作系统的 Mutex。GIL 的功能是：在 CPython 解释器中执行的每一个 Python 线程，都会先锁住自己，以阻止别的线程执行。  
当然，CPython 不可能容忍一个线程一直独占解释器，它会轮流执行 Python 线程。这样一来，用户看到的就是“伪”并行，即 Python 线程在交替执行，来模拟真正并行的线程。  
有读者可能会问，既然 CPython 能控制线程伪并行，为什么还需要 GIL 呢？其实，这和 CPython 的底层内存管理有关。
CPython 使用引用计数来管理内容，所有 Python 脚本中创建的实例，都会配备一个引用计数，来记录有多少个指针来指向它。当实例的引用计数的值为 0 时，会自动释放其所占的内存。

举个例子，看如下代码：
```python
>>> import sys
>>> a = []
>>> b = a
>>> sys.getrefcount(a)
3
```
可以看到，a 的引用计数值为 3，因为有 a、b 和作为参数传递的 getrefcount 都引用了一个空列表。
假设有两个 Python 线程同时引用 a，那么双方就都会尝试操作该数据，很有可能造成引用计数的条件竞争，导致引用计数只增加 1（实际应增加 2），这造成的后果是，当第一个线程结束时，会把引用计数减少 1，此时可能已经达到释放内存的条件（引用计数为 0），当第 2 个线程再次视图访问 a 时，就无法找到有效的内存了。
所以，CPython 引进 GIL，可以最大程度上规避类似内存管理这样复杂的竞争风险问题。
GIL这么烂，有没有办法绕过呢？我们来看看有哪些现成的方案。
用multiprocessing替代Thread
multiprocessing库的出现很大程度上是为了弥补thread库因为GIL而低效的缺陷。它完整的复制了一套thread所提供的接口方便迁移。唯一的不同就是它使用了多进程而不是多线程。每个进程有自己的独立的GIL，因此也不会出现进程之间的GIL争抢。   
当然multiprocessing也不是万能良药。它的引入会增加程序实现时线程间数据通讯和同步的困难。就拿计数器来举例子，如果我们要多个线程累加同一个变量，对于thread来说，申明一个global变量，用thread.Lock的context包裹住三行就搞定了。而multiprocessing由于进程之间无法看到对方的数据，只能通过在主线程申明一个Queue，put再get或者用share memory的方法。这个额外的实现成本使得本来就非常痛苦的多线程程序编码，变得更加痛苦了。   
##  任务队列实现
```c
// 任务结构体
typedef struct Task
{
    void (*function)(void* arg);//泛型，可接收其他参数类型
    void* arg;
}Task;  //定义后， Task== struct Task
// 线程池结构体
struct ThreadPool
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
    int busyNum;            // 忙的线程的个数
    int liveNum;            // 存活的线程的个数
    int exitNum;            // 要销毁的线程个数
    pthread_mutex_t mutexPool;  // 锁整个的线程池
    pthread_mutex_t mutexBusy;  // 锁busyNum变量
    pthread_cond_t notFull;     // 任务队列是不是满了
    pthread_cond_t notEmpty;    // 任务队列是不是空了
    int shutdown;           // 是不是要销毁线程池, 销毁为1, 不销毁为0
};
```
在threadpool.h中，写线程池所用到的函数
```c
//threadpool.h
#ifndef _THREADPOOL_H //先测试是否被宏定义过
#define _THREADPOOL_H
//define functions of threadpool
typedef struct ThreadPool ThreadPool;//头文件先进行声明
// 创建线程池并初始化
ThreadPool *threadPoolCreate(int min, int max, int queueSize); //得到一个线程池，返回线程池类型，需要的参数：最大最小线程数，任务队列容量
// 销毁线程池
int threadPoolDestroy(ThreadPool* pool);
// 给线程池添加任务
void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg);
// 获取线程池中工作的线程的个数
int threadPoolBusyNum(ThreadPool* pool);
// 获取线程池中活着的线程的个数
int threadPoolAliveNum(ThreadPool* pool);
// 工作的线程(消费者线程)任务函数
void* worker(void* arg);
// 管理者线程任务函数
void* manager(void* arg);
// 单个线程退出
void threadExit(ThreadPool* pool);
#endif  // _THREADPOOL_H ifndef和endif要一起使用，不能存在丢失。

```
然后在threadpool.c里面去写这些函数
do while(0);作用:只做1次的循环，但是它可以用break，所以我们才能释放malloc出的内存！
## worker函数：
访问线程池之前，给pool加锁，访问完了之后进行解锁
如果任务队列为空，说明已经没有任务需要处理了,且线程池没关闭，使用pthread_cond_wait, //1.阻塞线程 2.把附带的互斥锁打开 3.在此线程解除阻塞后，会自动把这把锁锁上  


任务队列也不为空，那么正式开始消费  
维护一个环形队列，取出任务：
```c
Task task;
task.function = pool->taskQ[pool->queueFront].function;
task.arg = pool->taskQ[pool->queueFront].arg;
```
取任务完毕，对线程池进行解锁
注意给arg初始化一段堆内存，防止被系统释放掉
## manager线程：
输入：线程池pool
首先每隔3s检测一次，取此线程池中任务的数量以及当前线程的数量  
然后取出当前在忙的线程的数量
添加线程：任务个数>存活的线程数； 存活的线程数<最大线程数  
销毁线程：当忙的线程*2<存活的线程数 && 存活的线程>最小线程数
怎么取消：让工作中的线程自杀，这些没有工作的线程阻塞在pthread_cond_wait上面，
使用pthread_cond_signal唤醒线程，让他向下执行，在worker里面判断当前是否要销毁该线程
通过signal前面的内容判断线程自杀的数量



