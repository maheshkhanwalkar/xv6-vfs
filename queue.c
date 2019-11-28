#include "queue.h"
#include "types.h"
#include "defs.h"

struct link {
    void* data;
    struct link* next;
};

typedef struct link* link_t;

struct queue {
    link_t head;
    link_t tail;
};

queue_t queue_create()
{
    queue_t q = (queue_t)kalloc();

    q->head = 0;
    q->tail = 0;

    return q;
}

void queue_enq(queue_t q, void* data)
{
    if(q == 0) {
        return;
    }

    link_t elem = (link_t)kalloc();

    elem->data = data;
    elem->next = 0;

    if(q->head == 0) {
        q->head = elem;
        q->tail = elem;
    } else {
        q->tail->next = elem;
        q->tail = elem;
    }
}

void* queue_deq(queue_t q)
{
    if(q == 0 || q->head == 0) {
        return 0;
    }

    void* data = q->head->data;;

    if(q->head == q->tail) {
        kfree((void*)q->head);

        q->head = 0;
        q->tail = 0;

        return data;
    }

    link_t save = q->head->next;
    kfree((void*)q->head);

    q->head = save;
    return data;
}

void* queue_peek(queue_t q)
{
    if(q == 0 || q->head == 0) {
        return 0;
    }

    return q->head->data;
}

void queue_destroy(queue_t q)
{
    if(q == 0) {
        return;
    }

    link_t ptr = q->head;

    while(ptr != 0) {
        link_t save = ptr->next;
        kfree((char*)ptr);

        ptr = save;
    }

    kfree((char*)q);
}
