#ifndef __PARTITIONS_H__
#define __PARTITIONS_H__

#define PARTITIONS_MANAGER_MODULE_NAME "/busses/blkdev/partitions_manager"

typedef struct part_device_info *part_device_cookie;

// manager between block devices and partition handlers
typedef struct partitions_manager {
    // use this to (un)register block devices
    // this is done automatically if you use the blkman module
    int (*add_blkdev)( const char *name, part_device_cookie *cookie );
    void (*remove_blkdev)( part_device_cookie device );
} partitions_manager;

typedef struct partition_map {
    // false, if map type not handled
    bool (*check_blkdev)( int fd );
} partition_handler;

#endif
