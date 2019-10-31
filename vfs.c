#include "vfs.h"
#include "map.h"
#include "defs.h"

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

    while(*ef != '\0' && *es != '\0')
    {
        if(*ef != *es) {
            return 0;
        }

        ef++;
        es++;
    }

    return *ef == *es;
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

void vfs_register_fs(const char* name, struct fs_ops* ops)
{
    if(!ops) {
        panic("NULL fs_ops!");
    }

    map_put(fs_map, name, ops, hash, equal);
}

struct fs_binding {
    struct fs_ops* ops;
    struct block_driver* drv;
};

void vfs_mount_fs(const char* path, const char* dev, const char* fs)
{
    struct fs_binding* bind = (void*)kalloc();

    bind->drv = map_get(b_map, dev, hash, equal);
    bind->ops = map_get(fs_map, fs, hash, equal);

    map_put(root_map, path, bind, hash, equal);
}

struct vfs_inode {
    struct inode* ip;
    struct fs_ops* ops;
    struct block_driver* drv;
};

struct vfs_inode* vfs_namei(const char* path)
{
    // TODO do longest prefix matching
    // FIXME: exact path matching will not work most times

    struct fs_binding* bind = map_get(root_map, path, hash, equal);

    // bad path -- couldn't find a match in the VFS table
    // return a NULL inode, which should raise a trap/fault somewhere
    if(bind == 0) {
        return 0;
    }

    struct vfs_inode* vi = (void*)kalloc();

    vi->drv = bind->drv;
    vi->ops = bind->ops;
    vi->ip = bind->ops->namei(path, vi->drv);

    return vi;
}
