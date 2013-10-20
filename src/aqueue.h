#ifndef AQUEUE_H_
#define AQUEUE_H_

#include "common.h"

struct aqueue;

struct aqueue *aqueue_create(size_t length);
void aqueue_delete(struct aqueue *q, void (*destructor)(void*));

bool aqueue_push(struct aqueue *q, void *item, unsigned pos);
void *aqueue_pop(struct aqueue *q, unsigned *pos);
void aqueue_close(struct aqueue *q);

#endif /* AQUEUE_H_ */
