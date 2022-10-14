// pthread_create.c 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
// 子线程的处理代码 function pointer
void* working(void* arg)
{
    printf("我是子线程, 线程ID: %ld\n", pthread_self());
    for(int i=0; i<9; ++i)
    {
        printf("子线程child == i: = %d\n", i);
    }
    return NULL;
}
int main()
{
    pthread_t tid;
    pthread_create(&tid, NULL, working, NULL);
    pthread_exit(NULL);

    return 0;
}

