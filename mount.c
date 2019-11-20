#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, const char* argv[])
{
    if(argc < 3) {
        printf(1, "error. expected device and path\n");
        printf(1, "e.g. mount /dev/sda1 /mnt\n");
        exit();
    }

    const char* dev = argv[1];
    const char* mnt = argv[2];

    mount(dev, mnt, "sfs");
    exit();
}
