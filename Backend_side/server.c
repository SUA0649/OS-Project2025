#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#define THREADPOOl 25

pthread_mutex_t mutexqueue = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condqueue = PTHREAD_COND_INITIALIZER;

typedef struct Message
{
    int a, b;
    // ID of message
    // Content of Message
    // ID of sender / Message to broadcast to
} Message;

Message mess[THREADPOOl];
int message_count = 0;

void send_message(Message *m)
{
    printf("%d , %d were the values\n", m->a, m->b);
    return;
}

void *start_thread_pool(void *arg)
{
    while (1)
    {
        Message m;
        int found = 0;
        pthread_mutex_lock(&mutexqueue);

        while (message_count == 0)
        {
            pthread_cond_wait(&condqueue, &mutexqueue);
        }
            m = mess[0];
            int i;
            for(i=0;i<message_count-1;i++){
                mess[i] = mess[i+1];
            }
            message_count--;
        pthread_mutex_unlock(&mutexqueue);
        send_message(&m);
    }
}

void submit_message(Message m){
    pthread_mutex_lock(&mutexqueue);
    mess[message_count] = m;
    message_count++;
    pthread_mutex_unlock(&mutexqueue);
    pthread_cond_signal(&condqueue);
}

int main()
{
    pthread_t th[THREADPOOl];
    int i;
    for (i = 0; i < THREADPOOl; i++)
    {
        if (pthread_create(&th[i], NULL, &start_thread_pool, NULL) != 0)
        {
            perror("There was an error in initializing the thread pool.");
        }
    }

    srand(time(NULL));
    for(i=0;i<25;i++){
        Message m = {
            .a = rand()%30,
            .b = rand()%40
        };
        submit_message(m);
    }

    for (i = 0; i < THREADPOOl; i++)
    {
        if (pthread_join(th[i], NULL) != 0)
        {
            perror("There was an error in the joining of the threads..");
        }
    }
    return 0;
}