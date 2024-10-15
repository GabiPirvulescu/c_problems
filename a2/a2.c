#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/wait.h>
#include "a2_helper.h"
#include <sys/sem.h>
#include <semaphore.h>
#include <stdbool.h>

#define NUM_CONCURRENT_THREADS 6

pthread_mutex_t mutex_73started = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_73ended = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_running_threads = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cond_running_threads = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_73started = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_73ended = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_block = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_hazard = PTHREAD_COND_INITIALIZER;
int sem_id;
int thread73_started = 0, thread73_ended = 0;
int running_threads = 0, total_running_threads = 0;
bool thread15 = false;
sem_t mysem, sem_block_threads;

void P(int semId, int semNr)
{
    struct sembuf op = {semNr, -1, 0};
    semop(semId, &op, 1);
}

void V(int semId, int semNr)
{
    struct sembuf op = {semNr, 1, 0};
    semop(semId, &op, 1);
}

void *thread7_function(void *id)
{
    int thread_id = *(int *)id;

    if (thread_id == 3)
    {
        pthread_mutex_lock(&mutex_73started);
        while (!thread73_started)
        {
            pthread_cond_wait(&cond_73started, &mutex_73started);
        }
        pthread_mutex_unlock(&mutex_73started);
    }

    if (thread_id == 5)
    {
        P(sem_id, 0);
    }

    info(BEGIN, 7, thread_id);

    if (thread_id == 4)
    {
        pthread_mutex_lock(&mutex_73started);
        thread73_started = 1;
        pthread_cond_signal(&cond_73started);
        pthread_mutex_unlock(&mutex_73started);
    }

    if (thread_id == 4)
    {
        pthread_mutex_lock(&mutex_73ended);
        while (!thread73_ended)
        {
            pthread_cond_wait(&cond_73ended, &mutex_73ended);
        }
        pthread_mutex_unlock(&mutex_73ended);
    }
    info(END, 7, thread_id);
    if (thread_id == 3)
    {
        pthread_mutex_lock(&mutex_73ended);
        thread73_ended = 1;
        pthread_cond_signal(&cond_73ended);
        pthread_mutex_unlock(&mutex_73ended);
    }

    if (thread_id == 5)
    {
        V(sem_id, 1);
    }
    pthread_exit(NULL);
}

int p8()
{
    info(BEGIN, 8, 0);
    info(END, 8, 0);
    return 0;
}

int p7()
{
    info(BEGIN, 7, 0);
    sem_id = semget(30000, 2, IPC_CREAT | 0666);
    int set[2] = {0, 0};
    semctl(sem_id, 0, SETALL, set);
    pthread_t threads[5];
    int thread_ids[5];

    for (int i = 0; i < 5; i++)
    {
        thread_ids[i] = i + 1;
        int ret = pthread_create(&threads[i], NULL, thread7_function, (void *)&thread_ids[i]);
        if (ret)
        {
            printf("Error creating thread %d\n", i + 1);
            return -1;
        }
    }

    for (int i = 0; i < 5; i++)
    {
        pthread_join(threads[i], NULL);
    }

    info(END, 7, 0);
    return 0;
}

void *thread6_function(void *id)
{
    int thread_id = *(int *)id;
    if (thread_id == 2)
    {
        P(sem_id, 1);
    }
    info(BEGIN, 6, thread_id);

    info(END, 6, thread_id);
    if (thread_id == 3)
    {
        V(sem_id, 0);
    }
    pthread_exit(NULL);
}

int p6()
{
    sem_id = semget(30000, 2, IPC_CREAT | 0666);
    int set[2] = {0, 0};
    semctl(sem_id, 0, SETALL, set);
    pthread_t threads[6];
    int thread_ids[6];
    info(BEGIN, 6, 0);

    for (int i = 0; i < 6; i++)
    {
        thread_ids[i] = i + 1;
        pthread_create(&threads[i], NULL, thread6_function, (void *)&thread_ids[i]);
    }
    for (int i = 0; i < 6; i++)
    {
        pthread_join(threads[i], NULL);
    }

    info(END, 6, 0);
    return 0;
}

int p5()
{
    info(BEGIN, 5, 0);

    pid_t pid6 = fork();
    switch (pid6)
    {
    case -1:
        perror("error! unsuccesful fork");
        exit(-1);
    case 0:
        exit(p6());
        break;
    default:
        break;
    }
    waitpid(pid6, NULL, 0);
    info(END, 5, 0);
    return 0;
}

int p4()
{
    info(BEGIN, 4, 0);
    info(END, 4, 0);
    return 0;
}

int p3()
{
    info(BEGIN, 3, 0);

    pid_t pid5 = fork();
    switch (pid5)
    {
    case -1:
        perror("error! unsuccesful fork");
        exit(-1);
    case 0:
        exit(p5());
        break;
    default:
        break;
    }
    waitpid(pid5, NULL, 0);
    info(END, 3, 0);
    return 0;
}
bool continuee = false;
void *thread2_function(void *id)
{
    int thread_id = *(int *)id;
    sem_wait(&mysem);

    info(BEGIN, 2, thread_id);

    pthread_mutex_lock(&mutex_running_threads);
    running_threads++;
    total_running_threads++;
    if (thread_id == 15)
    {
        thread15 = true;
        while (running_threads != 6)
        {
            pthread_cond_wait(&cond_running_threads, &mutex_running_threads);
        }
        pthread_mutex_unlock(&mutex_running_threads);
    }
    else
    {
        if (thread15)
        {
            if (running_threads == 6)
            {
                pthread_cond_signal(&cond_running_threads);
            }

            while (thread15)
            {
                pthread_cond_wait(&cond_block, &mutex_running_threads);
            }
            pthread_mutex_unlock(&mutex_running_threads);
            pthread_cond_broadcast(&cond_block);
        }
        else
        {
            if (total_running_threads > 31)

            {
                if (!continuee)
                {
                    while ((total_running_threads - 31) != running_threads)
                    {
                        pthread_cond_wait(&cond_hazard, &mutex_running_threads);
                    }
                }
                while (!continuee)
                {
                    pthread_cond_wait(&cond_block, &mutex_running_threads);
                }
                pthread_mutex_unlock(&mutex_running_threads);
                pthread_cond_broadcast(&cond_block);
            }
            else
            {
                pthread_mutex_unlock(&mutex_running_threads);
            }
        }
    }

    pthread_mutex_lock(&mutex_running_threads);
    running_threads--;
    pthread_mutex_unlock(&mutex_running_threads);

    info(END, 2, thread_id);

    pthread_mutex_lock(&mutex_running_threads);

    if (thread_id == 15)
    {
        continuee = true;
        thread15 = false;
        pthread_cond_broadcast(&cond_block);
    }

    pthread_cond_broadcast(&cond_hazard);

    pthread_mutex_unlock(&mutex_running_threads);

    sem_post(&mysem);

    pthread_exit(NULL);
}

int p2()
{
    info(BEGIN, 2, 0);

    pid_t pid4 = fork();
    switch (pid4)
    {
    case -1:
        perror("error! unsuccesful fork");
        exit(-1);
    case 0:
        exit(p4());
        break;
    default:
        break;
    }

    pid_t pid7 = fork();
    switch (pid7)
    {
    case -1:
        perror("error! unsuccesful fork");
        exit(-1);
    case 0:
        exit(p7());
        break;
    default:
        break;
    }

    waitpid(pid4, NULL, 0);
    waitpid(pid7, NULL, 0);

    pthread_t threads[37];
    int thread_ids[37];
    sem_init(&mysem, 0, NUM_CONCURRENT_THREADS);
    // sem_init(&sem_block_threads, 0, 6);
    for (int i = 0; i < 37; i++)
    {
        thread_ids[i] = i + 1;

        pthread_create(&threads[i], NULL, thread2_function, (void *)&thread_ids[i]);
    }

    for (int i = 0; i < 37; i++)
    {
        pthread_join(threads[i], NULL);
    }

    info(END, 2, 0);
    return 0;
}

void p1()
{
    pid_t pid2 = fork();
    switch (pid2)
    {
    case -1:
        perror("error! unsuccesful fork");
        exit(-1);
    case 0:
        exit(p2());
        break;
    default:
        break;
    }

    pid_t pid3 = fork();
    switch (pid3)
    {
    case -1:
        perror("error! unsuccesful fork");
        exit(-1);
    case 0:
        exit(p3());
        break;
    default:
        break;
    }

    pid_t pid8 = fork();
    switch (pid8)
    {
    case -1:
        perror("error! unsuccesful fork");
        exit(-1);
    case 0:
        exit(p8());
        break;
    default:
        break;
    }

    waitpid(pid2, NULL, 0);
    waitpid(pid3, NULL, 0);
    waitpid(pid8, NULL, 0);
}

int main()
{
    init();
    info(BEGIN, 1, 0);

    p1();

    info(END, 1, 0);
    return 0;
}
