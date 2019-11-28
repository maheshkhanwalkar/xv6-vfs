#pragma once

struct map;
typedef struct map* map_t;

map_t map_create();
void map_destroy(map_t m);

void map_put(map_t m, const void* key, void* value, int (*hash)(const void*), int (*equal)(const void*, const void*));
void* map_get(map_t m, const void* key, int (*hash)(const void*), int (*equal)(const void*, const void*));
int map_size(map_t m);
void map_keys(map_t m, const void** buffer);
