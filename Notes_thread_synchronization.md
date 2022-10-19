## 线程同步  
假设有 4 个线程 A、B、C、D，当前一个线程 A 对内存中的共享资源进行访问的时候，其他线程 B, C, D 都不可以对这块内存进行操作，直到线程 A 对这块内存访问完毕为止，B，C，D 中的一个才能访问这块内存，剩余的两个需要继续阻塞等待，以此类推，直至所有的线程都对这块内存操作完毕。 线程对内存的这种访问方式就称之为线程同步  
所谓的同步并不是多个线程同时对内存进行访问，而是按照先后顺序依次进行的。  
```c
int number = 0;
void* funcA_num(void* arg)
{
    for(int i=0; i<MAX; ++i)
    {
        int cur = number;
        cur++;
        usleep(10);
        number = cur;
        printf("Thread A, id = %lu, number = %d\n", pthread_self(), number);
    }
    return NULL;
}
void* funcB_num(void* arg)
{
    for(int i=0; i<MAX; ++i)
    {
        int cur = number;
        cur++;
        number = cur;
        printf("Thread B, id = %lu, number = %d\n", pthread_self(), number);
        usleep(5);
    }

    return NULL;
}
```
虽然每个线程内部循环了 50 次每次数一个数，但是最终没有数到 100，通过输出的结果可以看到，有些数字被重复数了多次，其原因就是没有对线程进行同步处理，造成了数据的混乱。  
两个线程在数数的时候需要分时复用 CPU 时间片，并且测试程序中调用了 sleep() 导致线程的 CPU 时间片没用完就被迫挂起了，这样就能让 CPU 的上下文切换（保存当前状态，下一次继续运行的时候需要加载保存的状态）更加频繁，更容易再现数据混乱的这个现象。  
数据从物理内存通过三级缓存到CPU的寄存器，比如数到5之后失去了CPU时间片，没有写回物理内存，寄存器和物理内存不同步，这是另一线程从物理内存读取数据，那么得到的是之前的数值。  
线程同步：数据从CPU寄存器到物理内存之间进行同步，同步的方式：加锁，让其他线程阻塞在锁上
## 互斥锁
互斥锁：是线程同步最常用的一种方式，通过互斥锁可以锁定一个代码块，被锁定的这个代码块，所有的线程只能顺序执行 (不能并行处理)，这样多线程访问共享资源数据混乱的问题就可以被解决了，需要付出的代价就是执行效率的降低，因为默认临界区多个线程是可以并行处理的，现在只能串行处理。  
在 Linux 中互斥锁的类型为 pthread_mutex_t，创建一个这种类型的变量就得到了一把互斥锁：
```c
pthread_mutex_t mutex; //记录这把锁是哪个线程锁定的，只有此线程能够解锁，通过init创建
// 初始化互斥锁，restrict: 是一个关键字, 用来修饰指针, 只有这个关键字修饰的指针可以访问指向的内存地址, 其他指针是不行的,所以只有mutex可访问
int pthread_mutex_init(pthread_mutex_t *restrict mutex,const pthread_mutexattr_t *restrict attr);//传入mutex和NULL
// 释放互斥锁资源，删除这个mutex            
int pthread_mutex_destroy(pthread_mutex_t *mutex);
// 修改互斥锁的状态, 将其设定为锁定状态, 这个状态被写入到参数mutex中,若此时已经锁定，那么会阻塞在这里
int pthread_mutex_lock(pthread_mutex_t *mutex);
// 尝试加锁，trylock面对已经锁上时，它不会阻塞，而是返回一个错误号
int pthread_mutex_trylock(pthread_mutex_t *mutex);
// 对互斥锁解锁
int pthread_mutex_unlock(pthread_mutex_t *mutex);

```
尽量减少临界区的代码量  
对thread_count.c的数数函数修改：
```c
    for(int i=0; i<MAX; ++i)
    {
        pthread_mutex_lock(& mutex);
        int cur = number;
        cur++;
        number = cur;
        printf("Thread B, id = %lu, number = %d\n", pthread_self(), number);
        pthread_mutex_unlock(& mutex);
    }
```
线程B调用时互斥锁已经被A锁上了，所以B阻塞来等A，数数能正常进行。在main()中要加上互斥锁的创建与销毁：
```c
pthread_mutex_init(&mutex, NULL);
...
//delete the mutex
pthread_mutex_destroy(&mutex);
```
## 死锁
如果锁使用不当，就会造成死锁这种现象。如果线程死锁造成的后果是：所有的线程都被阻塞，并且线程的阻塞是无法解开的（因为可以解锁的线程也被阻塞了）。
常见造成死锁的场景：1.忘记解锁
```c
// 当前线程A加锁成功, 当前循环完毕没有解锁, 在下一轮循环的时候自己被阻塞了
        // 其余的线程也被阻塞
    	pthread_mutex_lock(&mutex);
    	....
    	.....
        // 忘记解锁，或前面代码提前return，结束了这个线程，造成解锁失败
```
2.重复加锁
```c
// 当前线程A加锁成功
// 其余的线程阻塞
pthread_mutex_lock(&mutex);
// 锁被锁住了, A线程阻塞
pthread_mutex_lock(&mutex);
....
.....
pthread_mutex_unlock(&mutex);
//或者另一个线程在加锁之后，调用了A线程，然后A线程也执行了加锁，此时造成重复加锁
```
3.对共享资源同时加锁，当访问对方的资源时，造成死锁
避免死锁的办法：
避免多次锁定，多检查  

对共享资源访问完毕之后，一定要解锁，或者在加锁的使用 trylock，此函数返回的是错误号，不阻塞  

如果程序中有多把锁，可以控制对锁的访问顺序 (顺序访问共享资源，但在有些情况下是做不到的)，另外也可以在对其他互斥锁做加锁操作之前，先释放当前线程拥有的互斥锁。  

项目程序中可以引入一些专门用于死锁检测的模块  
## 读写锁
是互斥锁的升级版。在做读操作的时候可以提高程序的执行效率，如果所有的线程都是做读操作, 那么读是并行的，但是使用互斥锁，读操作也是串行的。  
之所以称其为读写锁，是因为这把锁既可以锁定读操作，也可以锁定写操作。为了方便理解，可以大致认为在这把锁中记录了这些信息：  
    锁的状态：锁定 / 打开  
    锁定的是什么操作：读操作 / 写操作，使用读写锁锁定了读操作，需要先解锁才能去锁定写操作，反之亦然。  
    哪个线程将这把锁锁上了  
读写锁的使用方式也互斥锁的使用方式是完全相同的：找共享资源，确定临界区，在临界区的开始位置加锁（读锁 / 写锁），临界区的结束位置解锁。  
因为通过一把读写锁可以锁定读或者写操作，读写锁的特点：  
    1.使用读写锁的读锁锁定了临界区，线程对临界区的访问是并行的，读锁是共享的。  
    2.使用读写锁的写锁锁定了临界区，线程对临界区的访问是串行的，写锁是独占的。  
    3.使用读写锁分别对两个临界区加了读锁和写锁，两个线程要同时访问者两个临界区，访问写锁临界区的线程继续运行，访问读锁临界区的线程阻塞，因为写锁比读锁的优先级高。  
所以如果说程序中**所有的线程都对共享资源做写操作，使用读写锁没有优势，和互斥锁是一样的**，如果说程序中**所有的线程都对共享资源有写也有读操作，并且对共享资源读的操作越多，读写锁更有优势**。
    Linux 提供的读写锁操作函数原型如下，如果函数调用成功返回 0，失败返回对应的错误号：
```c
#include <pthread.h>
pthread_rwlock_t rwlock;
// 初始化读写锁，类似互斥锁
int pthread_rwlock_init(pthread_rwlock_t *restrict rwlock,
           const pthread_rwlockattr_t *restrict attr);
// 释放读写锁占用的系统资源，同互斥锁
int pthread_rwlock_destroy(pthread_rwlock_t *rwlock);
```
#### 读写锁常用函数
```c
// 在程序中对读写锁加读锁, 锁定的是读操作
int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock);
// 在程序中对读写锁加写锁, 锁定的是写操作
int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock);
//以上两种加锁只能2选1，unlock操作是一样的
//trylock:
// 这个函数可以有效的避免死锁
// 如果加读锁失败, 不会阻塞当前线程, 直接返回错误号
int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock);
//如果读写锁是打开的，那么加锁成功；如果读写锁已经锁定了读操作，调用这个函数依然可以加锁成功，因为读锁是共享的；如果读写锁已经锁定了写操作，调用这个函数加锁失败，对应的线程不会被阻塞，可以在程序中对函数返回值进行判断，添加加锁失败之后的处理动作。
```
调用这个函数，如果读写锁是打开的，那么加锁成功；如果读写锁已经锁定了读操作，调用这个函数依然可以加锁成功，因为读锁是**共享**的；如果读写锁已经锁定了写操作，调用这个函数的线程会被阻塞。
#### 读写锁的使用
实现8 个线程操作同一个全局变量，3 个线程不定时写同一全局资源，5 个线程不定时读同一全局资源。  
```c
//创建8个线程，3个写5个读
pthread_t wtid[3];
pthread_t rtid[5];
for(int i=0; i<3; ++i)
{
pthread_create(&wtid[i], NULL, writeNum, NULL);
}
for(int i=0; i<5; ++i)
{
pthread_create(&rtid[i], NULL, readNum, NULL);
}
//创建读写锁
pthread_rwlock_init(&rwlock, NULL);
//线程资源回收
// 释放资源
for(int i=0; i<3; ++i)
{
pthread_join(wtid[i], NULL);
}
for(int i=0; i<5; ++i)
{
pthread_join(rtid[i], NULL);
}
//读操作的线程
void* readNum(void* arg)
{
    while(1)
    {
        pthread_rwlock_rdlock(&rwlock);
        printf("--全局变量number = %d, tid = %ld\n", number, pthread_self());
        pthread_rwlock_unlock(&rwlock);
        usleep(rand() % 10);
    }
    return NULL;
}
//写操作的线程
 void* writeNum(void* arg)
{
    while(1)
    {
        pthread_rwlock_wrlock(&rwlock);
        int cur = number;
        cur ++;
        number = cur;
        printf("++写操作完毕, number : %d, tid = %ld\n", number, pthread_self());
        pthread_rwlock_unlock(&rwlock);
        // 添加sleep目的是要看到多个线程交替工作
        usleep(rand() % 10);
    }

    return NULL;
}
```
## 条件变量
严格意义上来说，条件变量的主要作用不是处理线程同步，而是进行线程的阻塞。如果在多线程程序中只使用条件变量无法实现线程的同步，必须要配合互斥锁来使用。  
虽然条件变量和互斥锁都能阻塞线程，但是二者的效果是不一样的，二者的区别如下：  
    假设有 A-Z 26 个线程，这 26 个线程共同访问同一把互斥锁，如果线程 A 加锁成功，那么其余 B-Z 线程访问互斥锁都阻塞，所有的线程只能顺序访问临界区
    条件变量只有在满足指定条件下才会阻塞线程，如果条件不满足，多个线程可以同时进入临界区，同时读写临界资源，这种情况下还是会出现共享资源中数据的混乱。  
一般情况下条件变量用于处理生产者和消费者模型，并且和互斥锁配合使用。条件变量类型对应的类型为 pthread_cond_t  
#### 生产者消费者模型：
生产者和消费者模型的组成：  
    生产者线程 -> 若干个  
        生产商品或者任务放入到任务队列中  
        任务队列满了就阻塞，不满的时候就工作
        通过一个生产者的条件变量控制生产者线程阻塞和非阻塞
    消费者线程 -> 若干个
        读任务队列，将任务或者数据取出
        任务队列中有数据就消费，没有数据就阻塞
        通过一个消费者的条件变量控制消费者线程阻塞和非阻塞
    队列 -> 存储任务 / 数据，对应一块内存，为了读写访问可以通过一个数据结构维护这块内存
        可以是数组、链表，也可以使用 stl 容器：queue /stack/list/vector
操作函数：
```c
pthread_cond_t cond;
// 初始化
int pthread_cond_init(pthread_cond_t *restrict cond,
      const pthread_condattr_t *restrict attr);
// 销毁释放资源        
int pthread_cond_destroy(pthread_cond_t *cond);

// 线程阻塞函数, 哪个线程调用这个函数, 哪个线程就会被阻塞, 条件变量用于阻塞线程，mutex用于线程同步
int pthread_cond_wait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex);


// 将线程阻塞一定的时间长度, 时间到达之后, 线程就解除阻塞了
int pthread_cond_timedwait(pthread_cond_t *restrict cond,
           pthread_mutex_t *restrict mutex, const struct timespec *restrict abstime);

```
const struct timespec *restrict abstime 记录从1970.1.1 0:0到当前的时间，使用的时候要转化为相对时间：
```c
time_t mytim = time(NULL);	// 1970.1.1 0:0:0 到当前的总秒数
struct timespec tmsp;
tmsp.tv_nsec = 0;
tmsp.tv_sec = time(NULL) + 100;	// 线程阻塞100s
```
唤醒被阻塞的线程：
```c
// 唤醒阻塞在条件变量上的线程, 至少有一个被解除阻塞
int pthread_cond_signal(pthread_cond_t *cond);
// 唤醒阻塞在条件变量上的线程, 被阻塞的线程全部解除阻塞
int pthread_cond_broadcast(pthread_cond_t *cond);
```
#### 消费者模型的说明
pthread_cond_wait函数有三个功能：1.阻塞线程 2.把附带的互斥锁打开 3.在此线程解除阻塞后，会自动把这把锁锁上
如果生产者有生产上限，则需设置新的条件变量去唤醒消费者，不能用之前的条件量，因为一个是上限一个是下限
```c
// 消费者的回调函数
void* consumer(void* arg)
{
    while(1)
    {
        pthread_mutex_lock(&mutex);//消费者加锁
        // 一直消费, 删除链表中的一个节点
//        if(head == NULL)   // 这样写有bug
        while(head == NULL)
        {
            // 链表中已经没有节点可以消费了，消费者线程需要阻塞
            // 线程加互斥锁成功, 但是生产者的生产的时候发现已经锁上了, 锁还没解开 ->死锁
            // pthread_cond函数会在内部自动将线程拥有的锁解开
            pthread_cond_wait(&cond, &mutex); //阻塞之前会把这把锁打开，让其他线程能抢到这个资源
            // 当消费者线程解除阻塞之后, 会自动将这把锁锁上
            // 这时候当前这个线程又重新拥有了这把互斥锁
        }
        // 取出链表的头结点, 将其删除
        struct Node* pnode = head;
        printf("--consumer: number: %d, tid = %ld\n", pnode->number, pthread_self());
        head  = pnode->next;
        free(pnode);
        pthread_mutex_unlock(&mutex);     
        sleep(rand() % 3);
    }
    return NULL;
}
```
## 信号量
用信号量处理生产者消费者模型：信号量用在多线程多任务同步的，一个线程完成了某一个动作就通过信号量告诉别的线程，别的线程再进行某些动作。信号量不一定是锁定某一个资源，而是流程上的概念，比如：有 A，B 两个线程，B 线程要等 A 线程完成某一任务以后再进行自己下面的步骤，这个任务并不一定是锁定某一资源，还可以是进行一些计算或者数据处理之类。  
信号量（信号灯）与互斥锁和条件变量的主要不同在于” 灯” 的概念，灯亮则意味着资源可用，灯灭则意味着不可用。信号量主要阻塞线程，不能完全保证线程安全，如果要保证线程安全，需要信号量和互斥锁一起使用。  
信号量和条件变量一样用于处理生产者和消费者模型，用于阻塞生产者线程或者消费者线程的运行。信号的类型为 sem_t 对应的头文件为 <semaphore.h>  
```c
sem_t sem;
int sem_init(sem_t *sem, int pshared, unsigned int value);//value信号量的资源数
int sem_destroy(sem_t *sem);
```
pshared:指定是线程同步还是进程同步，0为线程同步，非0为进程同步
sem_wait(sem_t *sem); 调用一次sem_Wait函数，并且 sem 中的资源数 >0，线程**不会阻塞**，线程会占用 sem 中的一个资源，因此资源数-1，直到 sem 中的资源数减为 0 时，资源被耗尽，因此线程也就被阻塞了。     
sem_trywait(sem_t *sem);同wait,但是资源数为0的时候前者会阻塞，trywait返回错误号，不阻塞  
int sem_post(sem_t *sem); 让sem的资源数量+1，和wait相反
int sem_getvalue(sem_t *sem,int * sval); 查看资源数，并返回到第二个参数sval传出，相当于返回值
总资源数大于1的情况：信号量还需要和互斥锁联合使用
```c
// 消费者
sem_wait(&csem);
pthread_mutex_lock(&mutex);

// 生产者
sem_wait(&csem);
pthread_mutex_lock(&mutex);
```
加锁和信号量是不能颠倒的。如果先加锁，同时遇到了sem_wait的资源为0，阻塞的情况，那么会发生拿到互斥锁的线程阻塞==>死锁     
先阻塞再看是否要加锁。  



