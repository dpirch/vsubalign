#include "aqueue.h"

#include <pthread.h>

struct aqueue {
    size_t length, offset;
    unsigned pos;
    bool closed;
    pthread_cond_t pushwait, popwait;
    pthread_mutex_t mutex;
    void *buffer[];
};

struct aqueue *aqueue_create(size_t length)
{
    struct aqueue *q = xmalloc(sizeof *q + length * sizeof *q->buffer);
    *q = (struct aqueue) { .length = length };

    for (size_t i = 0; i < length; i++)
        q->buffer[i] = NULL;

    CHECK(!pthread_mutex_init(&q->mutex, NULL));
    CHECK(!pthread_cond_init(&q->pushwait, NULL));
    CHECK(!pthread_cond_init(&q->popwait, NULL));

    return q;
}

void aqueue_delete(struct aqueue *q, void (*destructor)(void*))
{
    if (destructor)
        for (size_t i = 0; i < q->length; i++)
            if (q->buffer[i])
                destructor(q->buffer[i]);

    pthread_cond_destroy(&q->pushwait);
    pthread_cond_destroy(&q->popwait);
    pthread_mutex_destroy(&q->mutex);
    free(q);
}

bool aqueue_push(struct aqueue *q, void *item, unsigned pos)
{
    assert(item);
    CHECK(!pthread_mutex_lock(&q->mutex));

    while (!q->closed && (pos - q->pos >= q->length))
        CHECK(!pthread_cond_wait(&q->pushwait, &q->mutex));

    bool success = !q->closed;
    if (!q->closed) {
        void **slot = q->buffer + (pos - q->pos + q->offset) % q->length;
        assert(!*slot);
        *slot = item;
        CHECK(!pthread_cond_signal(&q->popwait));
    }

    pthread_mutex_unlock(&q->mutex);
    return success;
}

void *aqueue_pop(struct aqueue *q, unsigned *pos)
{
    CHECK(!pthread_mutex_lock(&q->mutex));
    if (pos) *pos = q->pos;
    void **slot = q->buffer + q->offset;

    while (!q->closed && !*slot)
        CHECK(!pthread_cond_wait(&q->popwait, &q->mutex));

    void *item = *slot;
    if (item) {
        *slot = NULL;
        q->pos++;
        q->offset++;
        if (q->offset == q->length) q->offset = 0;
        CHECK(!pthread_cond_broadcast(&q->pushwait));
    }

    pthread_mutex_unlock(&q->mutex);
    return item;
}

void aqueue_close(struct aqueue *q)
{
    CHECK(!pthread_mutex_lock(&q->mutex));
    q->closed = true;
    pthread_mutex_unlock(&q->mutex);
}
