// pthread_join.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
struct Person
{
    int id;
    char name[36];
    int age;
};
// 子线程的处理代码
void* working(void* arg)
{
    printf("我是子线程, 线程ID: %ld\n", pthread_self());
    for(int i=0; i<9; ++i)
    {
        printf("child,i: = %d\n", i);
        if(i == 6)
        {
        /*
        static struct Person p;//static,move the "p" to static zone instead of stack zone
        p.age  = 12;
        strcpy(p.name, "tom");
        p.id = 100;
        // 该函数的参数将这个地址传递给了主线程的pthread_join()
        pthread_exit(&p);
        */
           struct Person* t = (struct Person*)arg;
           t->id = 3;
           strcpy(t->name,"tom");
           t->age = 1;

           pthread_exit(t);
        }
    }
    return NULL;	// 代码执行不到这个位置就退出了
}

int main()
{
    // 1. 创建一个子线程
    pthread_t tid;
    struct Person t;
    //
    pthread_create(&tid, NULL, working, &t);
    printf("子线程创建成功, 线程ID: %ld\n", tid);
    // 2. 子线程不会执行下边的代码, 主线程执行
    printf("我是主线程, 线程ID: %ld\n", pthread_self());
    for(int i=0; i<3; ++i)
    {
        printf("i = %d\n", i);
    }
    // 阻塞等待子线程退出
    void* ptr = NULL;
    // ptr是一个传出参数, 在函数内部让这个指针指向一块有效内存
    // 这个内存地址就是pthread_exit() 参数指向的内存
    pthread_join(tid, &ptr); //  ptr是void*类型,join要求二级指针,也就是传到ptr的地址
    // 打印信息
    struct Person* pp = (struct Person*)ptr;
    printf("子线程返回数据: name: %s, age: %d, id: %d\n", pp->name, pp->age, pp->id);
    printf("子线程资源被成功回收...\n");
    return 0;
}
