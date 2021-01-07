#ifndef SD_H_
#define SD_H_

#include <stdbool.h>
#include <stdint.h>

#include "lib/GPT.h"
#include "lib/Platform.h"
#include "lib/SPIMaster.h"

#if 1
// 20200901 taylor

#include "diskio.h"
#include "lib/UART.h"
#include "lib/Print.h"

struct SDCard {
    SPIMaster *interface;
    uint32_t   blockLen;
    uint32_t   tranSpeed;
    uint32_t   maxTranSpeed;
};
#endif

typedef struct SDCard SDCard;

/* MMC card type flags (MMC_GET_TYPE) */
#define CT_MMC		0x01		/* MMC ver 3 */
#define CT_SD1		0x02		/* SD ver 1 */
#define CT_SD2		0x04		/* SD ver 2 */
#define CT_SDC		0x06		/* SD */
#define CT_BLOCK	0x08		/* Block addressing */


SDCard  *SD_Open(SPIMaster *interface);
void     SD_Close(SDCard *card);

uint32_t SD_GetBlockLen(const SDCard *card);
bool     SD_SetBlockLen(SDCard *card, uint32_t len);

#if 1
// 20200902 taylor
bool SD_ReadBlock(SPIMaster *interface, uint32_t addr, void *data);
#else
bool     SD_ReadBlock(const SDCard *card, uint32_t addr, void *data);
#endif

#if 1
// 20200901 taylor
DSTATUS SD_disk_initialize (BYTE pdrv);
DSTATUS SD_disk_status (BYTE pdrv);
DRESULT SD_disk_read (BYTE pdrv, BYTE* buff, DWORD sector, UINT count);
DRESULT SD_disk_write (BYTE pdrv, const BYTE* buff, DWORD sector, UINT count);
DRESULT SD_disk_ioctl (BYTE pdrv, BYTE cmd, void* buff);
#endif

#if 0
// 20201014 taylor
// from CodethinkLabs
// Commit: 144f6a9b2e22301c29405cdeae62126d79251b64 [144f6a9]

bool     SD_WriteBlock(SDCard *card, uint32_t addr, const void *data);
#else
bool SD_WriteBlock(SPIMaster* interface, uint32_t addr, void* data);
#endif

#endif // #ifndef SD_H_
