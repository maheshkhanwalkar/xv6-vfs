#include "vfs.h"
#include "map.h"

static map_t b_map, fs_map, root_map;

static int hash(const void* key)
{
    const char* equiv = key;
    int h = 0;

    // not a good hash, but it's ok
    while(*equiv != '\0') {
        h += *equiv;
        equiv++;
    }

    return h;
}

static int equal(const void* first, const void* second)
{
    const char* ef = first;
    const char* es = second;

    while(*ef != '\0' || *es != '\0')
    {
        if(*ef != *es) {
            return 0;
        }

        ef++;
        es++;
    }

    return *ef != *es;
}

void vfs_init()
{
    b_map = map_create();
    fs_map = map_create();
    root_map = map_create();
}

void vfs_register_block(const char* name, struct block_driver* drv)
{
    map_put(b_map, name, drv, hash, equal);
}
