#include "ide_bus.h"
#include "ata_ide_drive.h"
#include "atapi_ide_drive.h"

ide_drive	*ide_drives[] =
{
	&ata_ide_drive,
	&atapi_ide_drive,
	NULL
};
void	*ide_drive_cookies[4];
