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
    struct superblock* sb;
    struct fs_ops* ops;
    struct block_driver* drv;
};

void vfs_mount_fs(const char* path, const char* dev, const char* fs)
{
    struct fs_binding* bind = (void*)kalloc();

    bind->drv = map_get(b_map, dev, hash, equal);

    if(!bind->drv) {
        panic("cannot find device\n");
    }

    bind->ops = map_get(fs_map, fs, hash, equal);

    if(!bind->ops) {
        panic("cannot find filesystem\n");
    }

    bind->sb = bind->ops->readsb(bind->drv);

    if(!bind->sb) {
        panic("cannot mount filesystem -- wrong type or corrupted\n");
    }

    map_put(root_map, path, bind, hash, equal);
}

struct vfs_inode {
    struct inode* ip;
    struct fs_ops* ops;
    struct block_driver* drv;
    struct superblock* sb;
};

static const char* vfs_rpath(const char* path)
{
    const char** keys = (void*)kalloc();
    map_keys(root_map, (const void**)keys);

    int* best = (void*)kalloc();
    memset(best, 0, 4096);

    int plen = strlen(path);
    int klen = map_size(root_map);

    for(int i = 0; i < plen; i++) {
        for(int j = 0; j < klen; j++) {
            // how good does each key match the path?
            if(best[j] == i && path[i] == keys[j][i]) {
                best[j]++;
            }
        }
    }

    int max = best[0];
    int pos = 0;

    for(int i = 1; i < klen; i++) {
        if(best[i] > max) {
            max = best[i];
            pos = i;
        }
    }

    const char* rpath = keys[pos];

    // cleanup the allocations
    kfree((void*)best);
    kfree((void*)keys);

    return rpath;
}

static char* vfs_rel(const char* path, const char* rpath)
{
    int pos = 0;

    while(*rpath != '\0') {
        if(*path != *rpath) {
            break;
        }

        pos++;
        path++;
        rpath++;
    }

    // path and rpath are the same
    if(*path == '\0') {
        char* buffer = (void*)kalloc();

        buffer[0] = '/';
        buffer[1] = '\0';

        return buffer;
    }

    int len = strlen(path);
    char* buffer = (void*)kalloc();

    buffer[0] = '/';
    for(int i = 0; i < len; i++) {
        buffer[i + 1] = path[i];
    }

    buffer[len + 1] = '\0';
    return buffer;
}

struct vfs_inode* vfs_namei(const char* path)
{
    // Longest prefix matching path
    const char* rpath = vfs_rpath(path);
    struct fs_binding* bind = map_get(root_map, rpath, hash, equal);

    // bad path -- couldn't find a match in the VFS table
    // return a NULL inode, which should raise a trap/fault somewhere
    if(bind == 0) {
        return 0;
    }

    // compute the relative path for the filesystem
    char* rel = vfs_rel(path, rpath);
    struct vfs_inode* vi = (void*)kalloc();

    vi->drv = bind->drv;
    vi->ops = bind->ops;
    vi->sb = bind->sb;

    // get underlying inode
    vi->ip = bind->ops->namei(rel, vi->sb, vi->drv);

    if(vi->ip == 0) {
        kfree((void*)vi);
        kfree(rel);

        return 0;
    }

    kfree(rel);
    return vi;
}

int vfs_readi(struct vfs_inode* vi, char* dst, int off, int size)
{
    // call the underlying fs readi() routine
    return vi->ops->readi(vi->ip, dst, off, size);
}

int vfs_writei(struct vfs_inode* vi, char* src, int off, int size)
{
    // call the underlying fs writei() routine
    // writei() may have modified the in-memory superblock -- so writesb() back to disk
    int res = vi->ops->writei(vi->ip, vi->sb, src, off, size);
    vi->ops->writesb(vi->sb, vi->drv);

    return res;
}
