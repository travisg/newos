#ifndef __BLKMAN_H__
#define __BLKMAN_H__

#include <kernel/fs/devfs.h>

// shamelessly pinched from BeOS
// some of them seem to be kind of useless;
// especially I don't like one error having multiple names
// like TIMEOUT or NO_MEMORY
#define ERR_DEV_GENERAL                 -5120

#define ERR_DEV_INVALID_IOCTL           ERR_DEV_GENERAL - 1
#define ERR_DEV_NO_MEMORY               ERR_DEV_GENERAL - 2
#define ERR_DEV_BAD_DRIVE_NUM           ERR_DEV_GENERAL - 3
#define ERR_DEV_NO_MEDIA                ERR_DEV_GENERAL - 4
#define ERR_DEV_UNREADABLE              ERR_DEV_GENERAL - 5
#define ERR_DEV_FORMAT_ERROR            ERR_DEV_GENERAL - 6
#define ERR_DEV_TIMEOUT                 ERR_DEV_GENERAL - 7
#define ERR_DEV_RECALIBRATE_ERROR       ERR_DEV_GENERAL - 8
#define ERR_DEV_SEEK_ERROR              ERR_DEV_GENERAL - 9
#define ERR_DEV_ID_ERROR                ERR_DEV_GENERAL - 10
#define ERR_DEV_READ_ERROR              ERR_DEV_GENERAL - 11
#define ERR_DEV_WRITE_ERROR             ERR_DEV_GENERAL - 12
#define ERR_DEV_NOT_READY               ERR_DEV_GENERAL - 13
#define ERR_DEV_MEDIA_CHANGED           ERR_DEV_GENERAL - 14
#define ERR_DEV_MEDIA_CHANGE_REQUESTED  ERR_DEV_GENERAL - 15
#define ERR_DEV_RESOURCE_CONFLICT       ERR_DEV_GENERAL - 16
#define ERR_DEV_CONFIGURATION_ERROR     ERR_DEV_GENERAL - 17
#define ERR_DEV_DISABLED_BY_USER        ERR_DEV_GENERAL - 18
#define ERR_DEV_DOOR_OPEN               ERR_DEV_GENERAL - 19

typedef struct blkdev_info *blkdev_cookie;
typedef struct blkdev_handle_info *blkdev_handle_cookie;
typedef struct blkman_device_info *blkman_dev_cookie;



typedef struct blkdev_params {
    // these properties can be changed
    uint block_size;
    uint64 capacity;

    // these entries cannot be changed later on
    size_t alignment;               // this must 2^i-1 for some i
    size_t max_blocks;
    size_t dma_boundary;
    bool dma_boundary_solid;
    int max_sg_num;
} blkdev_params;

typedef struct phys_vec {
    addr_t start;                   // physical address
    size_t len;
} phys_vec;

// two reason why to use array of size 1:
// 1. zero-sized arrays aren't standard C
// 2. it's handy if you need a temporary global variable with num=1
typedef struct phys_vecs {
    size_t num;
    size_t total_len;
    phys_vec vec[1];
} phys_vecs;

#define PHYS_VECS(name, size) \
    uint8 name[sizeof(phys_vecs) + (size - 1)*sizeof(phys_vec)]; \
    phys_vecs *name = (phys_vecs *)name

enum {
    // if the media got changed, all read/write accesses must be rejected
    // until get_media_status gets called
    IOCTL_GET_MEDIA_STATUS  = 0x8000
};


#define BLKMAN_MODULE_NAME "generic/blkman/v1"

typedef struct blkdev_interface {
    // iovecs are physical address here
    // pos and num_blocks are in blocks; bytes_transferred in bytes
    // vecs are guaranteed to be sufficient
    int (*open)( blkdev_cookie cookie, blkdev_handle_cookie *handle );
    int (*close)( blkdev_handle_cookie handle );

    int (*read) ( blkdev_handle_cookie handle, const phys_vecs *vecs,
                  off_t pos, size_t num_blocks, size_t *bytes_transferred );
    int (*write) ( blkdev_handle_cookie handle, const phys_vecs *vecs,
                   off_t pos, size_t num_blocks, size_t *bytes_transferred );

    int (*ioctl)( blkdev_handle_cookie handle, int op, void *buf, size_t len );
} blkdev_interface;

typedef struct blkman_interface {
    int (*register_blkdev)( blkdev_interface *interface, blkdev_cookie cookie,
                            const char *name, blkman_dev_cookie *blkman_cookie,
                            blkdev_params *params );
    int (*unregister_blkdev)( blkman_dev_cookie blkman_cookie );

    int (*set_capacity)( blkman_dev_cookie blkman_cookie, uint64 capacity, size_t block_size );
} blkman_interface;

#endif
