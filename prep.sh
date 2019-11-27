parted -s fs.img -- mklabel msdos \
	mkpart primary ext2 0 256KiB \
	mkpart primary ext2 257KiB -1s
