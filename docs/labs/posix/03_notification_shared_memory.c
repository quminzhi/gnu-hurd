/*
 * Notification + shared-memory model.
 *
 * A notification carries event bits. The payload lives in shared state. If
 * the same bit is signalled repeatedly before the waiter consumes it, the
 * event is coalesced; use a counter/queue in shared memory when every event
 * matters.
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

struct shared_state {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    unsigned notification_bits;
    unsigned shared_value;
    int done;
};

static void *producer(void *arg) {
    struct shared_state *state = arg;
    pthread_mutex_lock(&state->lock);
    state->shared_value = 42;
    state->notification_bits |= 0x1u; /* data-ready */
    state->notification_bits |= 0x2u; /* metadata-ready */
    pthread_cond_signal(&state->changed);
    pthread_mutex_unlock(&state->lock);
    return NULL;
}

static void *consumer(void *arg) {
    struct shared_state *state = arg;
    pthread_mutex_lock(&state->lock);
    while (state->notification_bits == 0 && !state->done) {
        pthread_cond_wait(&state->changed, &state->lock);
    }
    unsigned bits = state->notification_bits;
    unsigned value = state->shared_value;
    state->notification_bits = 0;
    state->done = 1;
    pthread_mutex_unlock(&state->lock);

    printf("consumer: notification bits=0x%x shared_value=%u\n", bits, value);
    printf("consumer: the notification identified events; shared memory carried the data\n");
    return NULL;
}

int main(void) {
    struct shared_state state = {
        .lock = PTHREAD_MUTEX_INITIALIZER,
        .changed = PTHREAD_COND_INITIALIZER,
        .notification_bits = 0,
        .shared_value = 0,
        .done = 0,
    };
    pthread_t producer_thread;
    pthread_t consumer_thread;

    if (pthread_create(&consumer_thread, NULL, consumer, &state) != 0 ||
        pthread_create(&producer_thread, NULL, producer, &state) != 0) {
        fprintf(stderr, "pthread_create failed\n");
        return EXIT_FAILURE;
    }
    pthread_join(producer_thread, NULL);
    pthread_join(consumer_thread, NULL);
    pthread_cond_destroy(&state.changed);
    pthread_mutex_destroy(&state.lock);
    return EXIT_SUCCESS;
}
