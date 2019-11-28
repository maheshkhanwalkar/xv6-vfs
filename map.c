#include "map.h"
#include "queue.h"
#include "defs.h"

#define BUCKET_CAPACITY 256

struct entry {
    const void* key;
    void* value;
};

typedef struct link {
    struct link* next;
    struct entry e;
} *link_t;


struct map {
    link_t buckets[BUCKET_CAPACITY];
    int size;
};

map_t map_create()
{
    map_t m = (void*)kalloc();
    memset(m, 0, 4096);
    m->size = 0;

    return m;
}

void map_put(map_t m, const void* key, void* value, int (*hash)(const void*), int (*equal)(const void*, const void*))
{
    if(m == 0) {
        return;
    }

    int h = hash(key);
    int pos = h % BUCKET_CAPACITY;

    if(m->buckets[pos] != 0) {
        link_t ptr = m->buckets[pos];

        while(ptr != 0) {

            if(equal(ptr->e.key, key)){
                ptr->e.value = value;
                return;
            }

            ptr = ptr->next;
        }
    }

    link_t lk = (void*)kalloc();

    struct entry e = {.key = key, .value = value};

    lk->next = m->buckets[pos];
    lk->e = e;

    m->buckets[pos] = lk;
    m->size++;
}

void* map_get(map_t m, const void* key, int (*hash)(const void*), int (*equal)(const void*, const void*))
{
    if(m == 0) {
        return 0;
    }

    int h = hash(key);
    int pos = h % BUCKET_CAPACITY;

    link_t ptr = m->buckets[pos];

    while(ptr != 0) {
        if(equal(ptr->e.key, key)) {
            return ptr->e.value;
        }
    }

    return 0;
}

int map_size(map_t m)
{
    if(m == 0) {
        return 0;
    }

    return m->size;
}

void map_keys(map_t m, const void** buffer)
{
    if(m == 0) {
        *buffer = 0;
        return;
    }

    int pos = 0;

    for(int i = 0; i < BUCKET_CAPACITY; i++)
    {
        link_t ptr = m->buckets[i];

        while(ptr != 0) {
            buffer[pos] = ptr->e.key;

            pos++;
            ptr = ptr->next;
        }
    }
}
