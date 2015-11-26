#ifndef __ATAPI_IDE_DRIVE_H__
#define __ATAPI_IDE_DRIVE_H__

#include "ide_drive.h"

extern  ide_drive   atapi_ide_drive;

struct atapi_msf {
    uint8 reserved;
    uint8 minute;
    uint8 second;
    uint8 frame;
};
#define MAX_TRACKS 99
struct atapi_toc_header {
    /*  int         last_session_lba;
        int         xa_flag;
        unsigned    capacity;*/
    uint16      toc_length;
    uint8       first_track;
    uint8       last_track;
};
struct atapi_toc_entry {
    uint8   reserved1;
    uint8   control : 4;
    uint8   adr     : 4;
    uint8   track;
    uint8   reserved2;
    union {
        unsigned lba;
        struct atapi_msf msf;
    } addr;
};

struct atapi_toc {
    struct atapi_toc_header hdr;
    struct atapi_toc_entry  ent[MAX_TRACKS+1];
};


struct mode_page_header {
    uint16 mode_data_length;
    uint8 medium_type;
    uint8 reserved1;
    uint8 reserved2;
    uint8 reserved3;
    uint16 desc_length;
};

typedef enum {
    mechtype_caddy = 0,
    mechtype_tray  = 1,
    mechtype_popup = 2,
    mechtype_individual_changer = 4,
    mechtype_cartridge_changer  = 5
} mechtype_t;
struct atapi_capabilities_page {
    struct mode_page_header header;
    uint8 page_code           : 6;
    uint8 reserved1           : 1;
    uint8 parameters_saveable : 1;
    uint8     page_length;
    /* Drive supports read from CD-R discs (orange book, part II) */
    uint8 cd_r_read           : 1; /* reserved in 1.2 */
    /* Drive can read from CD-R/W (CD-E) discs (orange book, part III) */
    uint8 cd_rw_read          : 1; /* reserved in 1.2 */
    /* Drive supports reading CD-R discs with addressing method 2 */
    uint8 method2             : 1;
    /* Drive supports reading of DVD-ROM discs */
    uint8 dvd_rom             : 1;
    /* Drive supports reading of DVD-R discs */
    uint8 dvd_r_read          : 1;
    /* Drive supports reading of DVD-RAM discs */
    uint8 dvd_ram_read        : 1;
    uint8 reserved2      : 2;
    /* Drive can write to CD-R discs (orange book, part II) */
    uint8 cd_r_write          : 1; /* reserved in 1.2 */
    /* Drive can write to CD-R/W (CD-E) discs (orange book, part III) */
    uint8 cd_rw_write    : 1; /* reserved in 1.2 */
    /* Drive can fake writes */
    uint8 test_write          : 1;
    uint8 reserved3a          : 1;
    /* Drive can write DVD-R discs */
    uint8 dvd_r_write         : 1;
    /* Drive can write DVD-RAM discs */
    uint8 dvd_ram_write       : 1;
    uint8 reserved3           : 2;
    /* Drive supports audio play operations. */
    uint8 audio_play          : 1;
    /* Drive can deliver a composite audio/video data stream. */
    uint8 composite           : 1;
    /* Drive supports digital output on port 1. */
    uint8 digport1            : 1;
    /* Drive supports digital output on port 2. */
    uint8 digport2            : 1;
    /* Drive can read mode 2, form 1 (XA) data. */
    uint8 mode2_form1         : 1;
    /* Drive can read mode 2, form 2 data. */
    uint8 mode2_form2         : 1;
    /* Drive can read multisession discs. */
    uint8 multisession        : 1;
    uint8 reserved4           : 1;
    /* Drive can read Red Book audio data. */
    uint8 cdda                : 1;
    /* Drive can continue a read cdda operation from a loss of streaming.*/
    uint8 cdda_accurate       : 1;
    /* Subchannel reads can return combined R-W information. */
    uint8 rw_supported        : 1;
    /* R-W data will be returned deinterleaved and error corrected. */
    uint8 rw_corr             : 1;
    /* Drive supports C2 error pointers. */
    uint8 c2_pointers         : 1;
    /* Drive can return International Standard Recording Code info. */
    uint8 isrc                : 1;
    /* Drive can return Media Catalog Number (UPC) info. */
    uint8 upc                 : 1;
    uint8 reserved5           : 1;
    /* Drive can lock the door. */
    uint8 lock                : 1;
    /* Present state of door lock. */
    uint8 lock_state          : 1;
    /* State of prevent/allow jumper. */
    uint8 prevent_jumper      : 1;
    /* Drive can eject a disc or changer cartridge. */
    uint8 eject               : 1;
    uint8 reserved6           : 1;
    /* Drive mechanism types. */
    mechtype_t mechtype  : 3;
    /* Audio level for each channel can be controlled independently. */
    uint8 separate_volume     : 1;
    /* Audio for each channel can be muted independently. */
    uint8 separate_mute       : 1;
    /* Changer can report exact contents of slots. */
    uint8 disc_present        : 1;  /* reserved in 1.2 */
    /* Drive supports software slot selection. */
    uint8 sss                 : 1;  /* reserved in 1.2 */
    uint8 reserved7           : 4;
    /* Note: the following four fields are returned in big-endian form. */
    /* Maximum speed (in kB/s). */
    unsigned short maxspeed;
    /* Number of discrete volume levels. */
    unsigned short n_vol_levels;
    /* Size of cache in drive, in kB. */
    unsigned short buffer_size;
    /* Current speed (in kB/s). */
    unsigned short curspeed;
    char pad[4];
};

#endif
