#ifndef SD_H_
#define SD_H_

#include <stdbool.h>
#include <stdint.h>

#include "lib/GPT.h"
#include "lib/Platform.h"
#include "lib/SPIMaster.h"

#if 1
// 20200901 taylor

struct SDCard {
    SPIMaster *interface;
    uint32_t   blockLen;
    uint32_t   tranSpeed;
    uint32_t   maxTranSpeed;
};
#endif

typedef struct SDCard SDCard;

SDCard  *SD_Open(SPIMaster *interface);
void     SD_Close(SDCard *card);

uint32_t SD_GetBlockLen(const SDCard *card);
bool     SD_SetBlockLen(SDCard *card, uint32_t len);

bool     SD_ReadBlock(const SDCard *card, uint32_t addr, void *data);

#if 0
// 20200901 taylor
bool SPITransfer__AsyncTimeout(
    SPIMaster         *interface,
    void              *data,
    uintptr_t          length,
    SPI_TRANSFER_TYPE  transferType);
#endif


#endif // #ifndef SD_H_
