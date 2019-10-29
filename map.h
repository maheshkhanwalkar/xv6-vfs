#pragma once

struct map;
typedef struct map* map_t;

map_t map_create();
void map_destroy(map_t m);

void map_put(map_t m, void* key, void* value, int (*hash)(void*), int (*equal)(void*, void*));
void* map_get(map_t m, void* key, int (*hash)(void*), int (*equal)(void*, void*));
