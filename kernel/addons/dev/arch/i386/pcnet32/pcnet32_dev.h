/*
**
**  PCNet-PCI (Lance) network card implementation, derived from
**  AMD whitepaper's pseudo-code at:
**  http://www.amd.com/us-en/assets/content_type/white_papers_and_tech_docs/19669.pdf
**  
**  (c)2001 Graham Batty
**  License: NewOS License
**
*/

#ifndef _PCNET32_DEV_H
#define _PCNET32_DEV_H

/* FreeBSD says:
#define PCI_DEVICE_ID_PCNet_PCI	0x20001022
#define PCI_DEVICE_ID_PCHome_PCI 0x20011022
*/

/* AMD Vendor ID */
#define AMD_VENDORID                          0x1022

/* PCNet/Home Device IDs */
#define PCNET_DEVICEID                        0x2000
#define PCHOME_DEVICEID                       0x2001

#endif
