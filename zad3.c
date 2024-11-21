#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

char *process_name = NULL;

int N;
int X;
int Y;
int S;

typedef struct
{
    void *map_ptr;
    pthread_mutex_t mutex;
    pthread_cond_t son_finished_condvar;
    long signalizing_son;
    pthread_cond_t notary_rdy_condvar;
    int notary_rdy;

} map_data_t;

map_data_t data;

pthread_barrier_t barrier;

int init_global_var(int argc, char *argv[])
{
    // if (argc != 5)
    // {
    //     printf("%s: init_global_var() incorrect number of arguments.\n", argv[0]);
    //     return -1;
    // }
    // N = atoi(argv[1]);
    // X = atoi(argv[2]);
    // Y = atoi(argv[3]);
    // S = atoi(argv[4]);

    N = 5;
    X = 10;
    Y = 10;
    S = 2;

    if ((data.map_ptr = malloc((long unsigned int)X * (long unsigned int)Y * sizeof(int))) == NULL)
    {
        printf("%s: init_global_var() malloc failed.\n", argv[0]);
        return -2;
    }
    int(*map)[Y][X] = (int(*)[Y][X])data.map_ptr;

    for (int i = 0; i < Y; ++i)
    {
        for (int j = 0; j < X; ++j)
        {
            (*map)[i][j] = 0;
        }
    }
    data.notary_rdy = 0;

    return 0;
}

int init_mutex()
{
    if (pthread_mutex_init(&data.mutex, NULL) != 0)
    {
        printf("%s: init_mutex() mutex init failed.\n", process_name);
        return -1;
    }
    if (pthread_cond_init(&data.son_finished_condvar, NULL) != 0)
    {
        printf("%s: init_mutex() cond init failed.\n", process_name);
        return -2;
    }
    if (pthread_cond_init(&data.notary_rdy_condvar, NULL) != 0)
    {
        printf("%s: init_mutex() cond init failed.\n", process_name);
        return -2;
    }
    if (pthread_barrier_init(&barrier, NULL, N) != 0)
    {
        printf("%s: init_mutex() barier init failed.\n", process_name);
        return -3;
    }

    return 0;
}

void *son(void *thread_id)
{
    long son_id = (long)thread_id;
    int chances = S;
    int rand_x = 0;
    int rand_y = 0;
    int(*map)[Y][X] = (int(*)[Y][X])data.map_ptr;

    pthread_barrier_wait(&barrier);

    printf("%s: son(): Son %ld starts drawing\n.", process_name, son_id);

    while (chances > 0)
    {
        // rand_x = rand() % X;
        // rand_y = rand() % Y;
        rand_x = (int)(drand48() * X);
        rand_y = (int)(drand48() * Y);
        pthread_mutex_lock(&data.mutex);
        if ((*map)[rand_y][rand_x] == 0)
        {
            (*map)[rand_y][rand_x] = (int)son_id;
            printf("%s: son(): Son %ld aquired %i %i. \n", process_name, son_id, rand_x, rand_y);
        }
        --chances;

        if (chances == 0)
        {
            printf("%s: son(): Son %ld finished\n.", process_name, son_id);
            while (data.signalizing_son != 0 || !data.notary_rdy)
            {
                printf("%s: son(): Son %ld  wait\n.", process_name, son_id);
                pthread_cond_wait(&data.notary_rdy_condvar, &data.mutex);
            }
            data.signalizing_son = son_id;
            printf("%s: son(): Son %ld  signal\n.", process_name, son_id);
            pthread_cond_signal(&data.son_finished_condvar);
        }
        pthread_mutex_unlock(&data.mutex);
    }

    pthread_exit(NULL);
}

void *notary(void *arg)
{
    int sons = N;
    int(*map)[Y][X] = (int(*)[Y][X])data.map_ptr;
    int i = 0;
    int j = 0;
    int sum = 0;
    long holder = (long)arg;
    j = (int)holder;
    j = 0;

    printf("%s: notary(): Notary starts competition\n\n", process_name);

    while (sons > 0)
    {
        sum = 0;
        --sons;
        pthread_mutex_lock(&data.mutex);
        data.notary_rdy = 1;
        data.signalizing_son = 0;
        while (data.signalizing_son == 0)
        {
            pthread_cond_signal(&data.notary_rdy_condvar);
            pthread_cond_wait(&data.son_finished_condvar, &data.mutex);
        }

        printf("\n%s: notary(): Son %li map of parcels:\n", process_name, data.signalizing_son);
        for (i = 0; i < Y; ++i)
        {
            for (j = 0; j < X; ++j)
            {
                if ((*map)[i][j] == data.signalizing_son)
                {
                    sum += 1;
                    printf(" %li ", data.signalizing_son);
                }
                else
                    printf(" - ");
            }
            printf("\n");
        }
        printf("%s: notary(): Son %li aquired %i/%i parcels:\n", process_name, data.signalizing_son, sum, X * Y);
        pthread_mutex_unlock(&data.mutex);
    }

    sum = 0;

    pthread_mutex_lock(&data.mutex);
    printf("\n%s: notary(): Competition has come to the end\n", process_name);
    for (i = 0; i < Y; ++i)
    {
        for (j = 0; j < X; ++j)
        {
            if ((*map)[i][j] == 0)
                sum += 1;
        }
    }
    printf("%s: notary(): %i/%i parcels remained unocuppied\n", process_name, sum, X * Y);

    pthread_mutex_unlock(&data.mutex);

    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    process_name = argv[0];
    pthread_t threads[N + 1];

    if (init_global_var(argc, argv) != 0)
        return -1;

    if (init_mutex() != 0)
        return -2;

    srand((unsigned int)time(NULL));

    pthread_create(&threads[0], NULL, notary, NULL);
    for (long i = 1; i < N + 1; ++i)
    {
        printf("%s: main() Tworzenie %li syna.\n", process_name, i);
        if (pthread_create(&threads[i], NULL, son, (void *)i) != 0)
        {
            printf("%s: main() thread creation failed.\n", process_name);
            return -3;
        }
    }

    for (int i = 0; i < N + 1; ++i)
    {
        pthread_join(threads[i], NULL);
    }

    pthread_cond_destroy(&data.son_finished_condvar);
    pthread_mutex_destroy(&data.mutex);
    pthread_barrier_destroy(&barrier);
    free(data.map_ptr);
    pthread_exit(NULL);
}