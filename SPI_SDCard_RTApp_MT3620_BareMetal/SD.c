#include "SD.h"

// This is the maximum number of SD cards which can be opened at once.
#define SD_CARD_MAX 4
#define SPI_SD_TIMEOUT 10 // [s]
#if 1
// 20201014 taylor
// from CodethinkLabs
// Commit: 144f6a9b2e22301c29405cdeae62126d79251b64 [144f6a9]

#define NUM_RETRIES 65536
#endif

static GPT *timer = NULL;

typedef enum {
    GO_IDLE_STATE        =  0,
    SEND_OP_COND         =  1,
    ALL_SEND_CID         =  2,
    SEND_RELATIVE_ADDR   =  3,
    SWITCH_FUNC          =  6,
    SELECT_CARD          =  7,
    SEND_IF_COND         =  8,
    SEND_CSD             =  9,
    SEND_CID             = 10,
    READ_DAT_UNTIL_STOP  = 11,
    STOP_TRANSMISSION    = 12,
    GO_INACTIVE_STATE    = 15,
    SET_BLOCKLEN         = 16,
    READ_SINGLE_BLOCK    = 17,
    READ_MULTIPLE_BLOCK  = 18,
    SET_BLOCK_COUNT      = 23,
    WRITE_BLOCK          = 24,
    WRITE_MULTIPLE_BLOCK = 25,
    PROGRAM_CSD          = 27,
    SET_WRITE_PROT       = 28,
    CLR_WRITE_PROT       = 29,
    SEND_WRITE_PROT      = 30,
    ERASE_WR_BLK_START   = 32,
    ERASE_WR_BLK_END     = 33,
    ERASE                = 38,
    LOCK_UNLOCK          = 42,
    APP_CMD              = 55,
    GEN_CMD              = 56,
    READ_OCR             = 58,
    CRC_ON_OFF           = 59,
} SD_CMD;

typedef enum {
    APP_SET_BUS_WIDTH          =  6,
    APP_SD_STATUS              = 13,
    APP_SEND_NUM_WR_BLOCKS     = 22,
    APP_SET_WR_BLK_ERASE_COUNT = 23,
    APP_SEND_OP_COND           = 41,
    APP_SET_CLR_CARD_DETECT    = 42,
    APP_SEND_SCR               = 51,
} SD_ACMD;

#if 1
// 20201014 taylor
// from CodethinkLabs
// Commit: 144f6a9b2e22301c29405cdeae62126d79251b64 [144f6a9]

typedef enum {
    DATA_TOKEN_READ_SINGLE     = 0xFE,
    DATA_TOKEN_READ_MULT       = 0xFE,
    DATA_TOKEN_WRITE_SINGLE    = 0xFE,
    DATA_TOKEN_WRITE_MULT      = 0xFC,
    DATA_TOKEN_WRITE_MULT_STOP = 0xFD
} SD_DATA_TOKEN;

typedef enum {
    DATA_RESP_ACCEPTED    = 0x5,
    DATA_RESP_CRC_ERROR   = 0xB,
    DATA_RESP_WRITE_ERROR = 0xD
} SD_DATA_RESPONSE;
#endif

typedef struct __attribute__((__packed__)) {
    uint8_t  index;
    uint32_t argument;
    uint8_t  crc;
} SD_CommandFrame;

typedef union __attribute__((__packed__)) {
    struct __attribute__((__packed__)) {
        bool     idle               : 1;
        bool     erase              : 1;
        bool     illegalCommand     : 1;
        bool     comCrcError        : 1;
        bool     eraseSequenceError : 1;
        bool     addressError       : 1;
        bool     parameterError     : 1;
        unsigned zero               : 1;
    };
    uint8_t mask;
} SD_R1;

typedef struct __attribute__((__packed__)) {
    SD_R1    r1;
    uint32_t ocr;
} SD_R3;

typedef struct __attribute__((__packed__)) {
    SD_R1 r1;

    union __attribute__((__packed__)) {
        struct __attribute__((__packed__)) {
            unsigned commandVersion  :  4;
            unsigned reserved        : 16;
            unsigned voltageAccepted :  4;
            unsigned checkPattern    :  8;
        };
        uint32_t mask;
    };
} SD_R7;

#if 1
// 20200901 taylor
#if 0
struct SDCard {
    SPIMaster *interface;
    uint32_t   blockLen;
    uint32_t   tranSpeed;
    uint32_t   maxTranSpeed;
};
#endif
#else
struct SDCard {
    SPIMaster *interface;
    uint32_t   blockLen;
    uint32_t   tranSpeed;
    uint32_t   maxTranSpeed;
};
#endif

typedef struct {
    bool    done;
    int32_t status;
    int32_t count;
} TransferState;

static volatile TransferState transferState = {
    .done   = false,
    .status = ERROR_NONE,
    .count = 0
};

#if 1
// 20200901 taylor

extern UART      *debug;
extern SPIMaster *driver;

uint16_t Timer1, Timer2;					/* 1ms Timer Counter */

static volatile DSTATUS Stat = STA_NOINIT;	/* Disk Status */
static uint8_t CardType;                    /* Type 0:MMC, 1:SDC, 2:Block addressing */
static uint8_t PowerFlag = 0;				/* Power flag */

#endif

static void transferDoneCallback(int32_t status, uintptr_t dataCount)
{
    transferState.done   = true;
    transferState.status = status;
    transferState.count  = dataCount;
}

static void transferStateReset()
{
    transferState.done   = false;
    transferState.status = ERROR_NONE;
    transferState.count  = 0;
}

static uint8_t SD_Crc7(void *data, uintptr_t size)
{
    uint8_t* data_byte = data;
    uint8_t crc = 0x00;
    uintptr_t byte;
    for (byte = 0; byte < size; byte++) {
        uint8_t c = data_byte[byte];
        unsigned bit;
        for (bit = 0; bit < 8; bit++) {
            crc <<= 1;
            if ((c ^ crc) & 0x80) {
                crc ^= 0x09;
            }
            c <<= 1;
        }
        crc &= 0x7F;
    }

    return (crc << 1) | 0x01;
}

typedef enum {
    SPI_READ  = 0,
    SPI_WRITE = 1
} SPI_TRANSFER_TYPE;

#if 0
// 20200901 taylor
bool SPITransfer__AsyncTimeout(
    SPIMaster         *interface,
    void              *data,
    uintptr_t          length,
    SPI_TRANSFER_TYPE  transferType)
{
    if (!interface) {
        return false;
    }

    SPITransfer transfer = {
        .writeData = NULL,
        .readData  = NULL,
        .length    = length,
    };

    int32_t status;
    switch (transferType) {
    case SPI_READ:
        transfer.readData = data;
        status = SPIMaster_TransferSequentialAsync(interface, &transfer, 1, transferDoneCallback);
        break;

    case SPI_WRITE:
        transfer.writeData = data;
        status = SPIMaster_TransferSequentialAsync(interface, &transfer, 1, transferDoneCallback);
        break;

    default:
        return false;
    }

    if (status != ERROR_NONE) {
        return false;
    }

    GPT_StartTimeout(timer, SPI_SD_TIMEOUT, GPT_UNITS_SECOND, NULL);

    while (!transferState.done) {
        __asm__("wfi");
        if (!GPT_IsEnabled(timer)) {
            // Timed out, so cancel
            SPIMaster_TransferCancel(interface);
            break;
        }
    }

    status = transferState.status;
    transferStateReset();

    if (status != ERROR_NONE) {
        return false;
    }

    return true;
}

#else
static bool SPITransfer__AsyncTimeout(
    SPIMaster         *interface,
    void              *data,
    uintptr_t          length,
    SPI_TRANSFER_TYPE  transferType)
{
    if (!interface) {
        return false;
    }

    SPITransfer transfer = {
        .writeData = NULL,
        .readData  = NULL,
        .length    = length,
    };

    int32_t status;
    switch (transferType) {
    case SPI_READ:
        transfer.readData = data;
        status = SPIMaster_TransferSequentialAsync(interface, &transfer, 1, transferDoneCallback);
        break;

    case SPI_WRITE:
        transfer.writeData = data;
        status = SPIMaster_TransferSequentialAsync(interface, &transfer, 1, transferDoneCallback);
        break;

    default:
        return false;
    }

    if (status != ERROR_NONE) {
        return false;
    }

    GPT_StartTimeout(timer, SPI_SD_TIMEOUT, GPT_UNITS_SECOND, NULL);

    while (!transferState.done) {
        __asm__("wfi");
        if (!GPT_IsEnabled(timer)) {
            // Timed out, so cancel
            SPIMaster_TransferCancel(interface);
            break;
        }
    }

    status = transferState.status;
    transferStateReset();

    if (status != ERROR_NONE) {
        return false;
    }

    return true;
}
#endif

static bool SD_ClockBurst(SPIMaster* interface, unsigned cycles, bool select)
{
    if (cycles == 0) {
        return true;
    }

    int32_t status;
    if (!select) {
        status = SPIMaster_SelectEnable(interface, false);
        if (status != ERROR_NONE) {
            return false;
        }
    }

    // Burst the clock for a bit to allow command to process
    // We use async here so we can timeout if the SD card hangs
    uint8_t dummy[(cycles + 7) / 8];

    if (!SPITransfer__AsyncTimeout(interface, &dummy, sizeof(dummy), SPI_READ)) {
        return false;
    }

    if (!select) {
        status = SPIMaster_SelectEnable(interface, true);
        if (status != ERROR_NONE) {
            return false;
        }
    }

    return true;
}

static bool SD_AwaitResponse(SPIMaster *interface, uintptr_t size, void *response, unsigned retries)
{
    uint8_t byte = 0xFF;
    unsigned i;
    for (i = 0; (i < retries) && (byte == 0xFF); i++) {
        if (!SPITransfer__AsyncTimeout(interface, &byte, 1, SPI_READ)) {
            return false;
        }
    }

    uint8_t* r = response;
    r[0] = byte;
    if ((byte & 0x7C) != 0) {
        // If the response contains an error, it won't have a payload.
        size = 1;
    } else if (size > 1) {
        if (!SPITransfer__AsyncTimeout(interface, &r[1], (size - 1), SPI_READ)) {
            return false;
        }
    }

    return true;
}

static bool SD_CommandIncomplete(SPIMaster *interface, SD_CMD cmd, uint32_t argument,
                                 uintptr_t response_size, void* response)
{
    SD_CommandFrame frame;
    frame.index    = (0b01 << 6) | cmd;
    frame.argument = __builtin_bswap32(argument);
    frame.crc      = SD_Crc7(&frame, (sizeof(frame.index) + sizeof(frame.argument)));


    if (!SPITransfer__AsyncTimeout(interface, &frame, sizeof(frame), SPI_WRITE)) {
        return false;
    }

    // Ignore first byte of response.
    if (!SD_ClockBurst(interface, 8, true)) {
        return false;
    }

    unsigned retries = 32;
    if (!SD_AwaitResponse(interface, response_size, response, retries)) {
        return false;
    }

    return true;
}

static bool SD_Command(SPIMaster *interface, SD_CMD cmd, uint32_t argument,
                       uintptr_t response_size, void* response)
{
    if (!SD_CommandIncomplete(
        interface, cmd, argument, response_size, response)) {
        return false;
    }

    // Burst the clock for a bit to allow command to process
    if (!SD_ClockBurst(interface, 32, false)) {
        return false;
    }

    return true;
}

#if 1
// 20200902 taylor
static bool SD_ReadDataPacket(SPIMaster *interface, uintptr_t size, void *data)
{
    #define DBG_SD_READDATAPACKET
    unsigned retries = 65536;
    uint8_t byte = 0xFF;
    unsigned i;
    for (i = 0; (i < retries) && (byte == 0xFF); i++) {
        if (!SPITransfer__AsyncTimeout(interface, &byte, 1, SPI_READ)) {
            #ifdef DBG_SD_READDATAPACKET
            UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
            #endif
            return false;
        }
    }
    if (byte != 0xFE) {
        #ifdef DBG_SD_READDATAPACKET
        UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
        #endif
        return false;
    }

    uint8_t *data_byte = data;
    uintptr_t packet = 16;
    uintptr_t remain = size;
    for (remain = size; remain; remain -= packet, data_byte += packet) {
        if (packet > remain) {
            packet = remain;
        }

        if (!SPITransfer__AsyncTimeout(interface, data_byte, packet, SPI_READ)) {
            #ifdef DBG_SD_READDATAPACKET
            UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
            #endif
            return false;
        }
    }

    uint16_t crc;
    if (!SPITransfer__AsyncTimeout(interface, &crc, sizeof(crc), SPI_READ)) {
        #ifdef DBG_SD_READDATAPACKET
        UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
        #endif
        return false;
    }

    // TODO: Verify the CRC.

    // Clock burst is required here to give the card time to recover?
    SD_ClockBurst(interface, 32, false);

    return true;
}

#else
static bool SD_ReadDataPacket(const SDCard *card, uintptr_t size, void *data)
{
#if 1
    // 20201014 taylor
    // from CodethinkLabs
    // Commit: 144f6a9b2e22301c29405cdeae62126d79251b64 [144f6a9]

    unsigned retries = NUM_RETRIES;
#else
    unsigned retries = 65536;
#endif
    uint8_t byte = 0xFF;
    unsigned i;
    for (i = 0; (i < retries) && (byte == 0xFF); i++) {
        if (!SPITransfer__AsyncTimeout(card->interface, &byte, 1, SPI_READ)) {
            return false;
        }
    }
#if 1
    // 20201014 taylor
    // from CodethinkLabs
    // Commit: 144f6a9b2e22301c29405cdeae62126d79251b64 [144f6a9]
    if (byte != DATA_TOKEN_READ_SINGLE) {
        return false;
    }
#else
    if (byte != 0xFE) {
        return false;
    }

#endif
    uint8_t *data_byte = data;
    uintptr_t packet = 16;
    uintptr_t remain = size;
    for (remain = size; remain; remain -= packet, data_byte += packet) {
        if (packet > remain) {
            packet = remain;
        }

        if (!SPITransfer__AsyncTimeout(card->interface, data_byte, packet, SPI_READ)) {
            return false;
        }
    }

    uint16_t crc;
    if (!SPITransfer__AsyncTimeout(card->interface, &crc, sizeof(crc), SPI_READ)) {
        return false;
    }

    // TODO: Verify the CRC.

    // Clock burst is required here to give the card time to recover?
    SD_ClockBurst(card->interface, 32, false);

    return true;
}
#endif

#if 1
// 20201014 taylor
// from CodethinkLabs
// Commit: 144f6a9b2e22301c29405cdeae62126d79251b64 [144f6a9]

static bool SD_WriteDataPacket(SDCard *card, uintptr_t size, const void *data)
{
    // Clock burst for >= 1 byte
    SD_ClockBurst(card->interface, 2, false);

    // Write data token
    static uint8_t write_token = DATA_TOKEN_WRITE_SINGLE;
    if (!SPITransfer__AsyncTimeout(card->interface, &write_token, 1, SPI_WRITE)) {
        return false;
    }

    // Write data
    uint8_t *data_byte = (uint8_t*)data;
    uintptr_t packet = 16;
    uintptr_t remain = size;
    for (remain = size; remain; remain -= packet, data_byte += packet) {
        if (packet > remain) {
            packet = remain;
        }

        if (!SPITransfer__AsyncTimeout(
            card->interface, data_byte, packet, SPI_WRITE))
        {
            return false;
        }
    }

    // Write crc
    // TODO: implement 16 bit crc calc (SPI mode SD cards ignore CRC)
    static uint16_t blank_crc = 0xFFFF;
    if (!SPITransfer__AsyncTimeout(
        card->interface, &blank_crc, sizeof(blank_crc), SPI_WRITE))
    {
        return false;
    }

    // Read data response
    unsigned retries = NUM_RETRIES;
    uint8_t byte = 0xFF;
    unsigned i;
    for (i = 0; (i < retries) && (byte == 0xFF); i++) {
        if (!SPITransfer__AsyncTimeout(card->interface, &byte, 1, SPI_READ)) {
            return false;
        }
    }
    if ((byte & 0xF) != DATA_RESP_ACCEPTED) {
        return false;
    }

    // Wait while card holds MISO low (busy)
    unsigned busy_waits = NUM_RETRIES;
    byte = 0x00;
    for (i = 0; (i < busy_waits) && (byte == 0x00); i++) {
        if (!SPITransfer__AsyncTimeout(card->interface, &byte, 1, SPI_READ)) {
            return false;
        }
    }

    if (byte == 0x00) {
        return false;
    }

    return true;
}
#endif

static bool SD_ReadCSD(SDCard *card)
{
    SD_R1 response;
    if (!SD_CommandIncomplete(card->interface, SEND_CSD, 0, sizeof(response), &response)) {
        return false;
    }
    if ((response.mask & 0xC0) != 0) {
        return false;
    }

    uint8_t csd[16];
    if (!SD_ReadDataPacket(card, sizeof(csd), csd)) {
        return false;
    }

    uint8_t tranSpeedRaw = csd[3];

    unsigned tranSpeedUnitRaw = (tranSpeedRaw & 0x07);
    unsigned tranSpeedUnit = 10000;
    for (; tranSpeedUnitRaw > 0; tranSpeedUnit *= 10, tranSpeedUnitRaw--);

    unsigned tranSpeedValueRaw = ((tranSpeedRaw >> 3) & 0xF);
    static unsigned tranSpeedValueTable[16] = {
         0, 10, 12, 13,
        15, 20, 25, 30,
        35, 40, 45, 50,
        55, 60, 70, 80,
    };
    unsigned tranSpeedValue = tranSpeedValueTable[tranSpeedValueRaw];

    if ((tranSpeedValue != 0)
        && ((tranSpeedRaw & 0x80) == 0)) {
        card->maxTranSpeed = tranSpeedValue * tranSpeedUnit;
    }

    return true;
}

#if 1
static bool SD_ReadCSD_test(SPIMaster *interface)
{
    SD_R1 response;
    if (!SD_CommandIncomplete(interface, SEND_CSD, 0, sizeof(response), &response)) {
        return false;
    }
    if ((response.mask & 0xC0) != 0) {
        return false;
    }

    uint8_t csd[16];
    if (!SD_ReadDataPacket(interface, sizeof(csd), csd)) {
        return false;
    }

    uint8_t tranSpeedRaw = csd[3];

    unsigned tranSpeedUnitRaw = (tranSpeedRaw & 0x07);
    unsigned tranSpeedUnit = 10000;
    for (; tranSpeedUnitRaw > 0; tranSpeedUnit *= 10, tranSpeedUnitRaw--);

    unsigned tranSpeedValueRaw = ((tranSpeedRaw >> 3) & 0xF);
    static unsigned tranSpeedValueTable[16] = {
         0, 10, 12, 13,
        15, 20, 25, 30,
        35, 40, 45, 50,
        55, 60, 70, 80,
    };
    unsigned tranSpeedValue = tranSpeedValueTable[tranSpeedValueRaw];

    if ((tranSpeedValue != 0)
        && ((tranSpeedRaw & 0x80) == 0)) {
        uint32_t maxTranSpeed = tranSpeedValue * tranSpeedUnit;

        SPIMaster_Configure(interface, 0, 0, maxTranSpeed);
    }

    return true;
}
#endif


static bool SD_GoIdleState(SPIMaster *interface, unsigned retries)
{
    unsigned i;
    for (i = 0; i < retries; i++) {
        SD_R1 response;
        if (!SD_Command(interface, GO_IDLE_STATE, 0, sizeof(response), &response)) {
            return false;
        }

        if (response.mask == 0x01) {
            break;
        }
    }
    return (i < retries);
}

static bool SD_SendIfCond(SPIMaster *interface)
{
    SD_R7 response7;
    if (!SD_Command(interface, SEND_IF_COND, 0x000001AA, sizeof(response7), &response7)) {
        return false;
    }

    SD_R1 response = response7.r1;
    if (response.mask == 0x01) {
        if (__builtin_bswap32(response7.mask) != 0x000001AA) {
            return false;
        }
    } else if (response.mask != 0x05) {
        return false;
    }

    return true;
}

static bool SD_SendOpCond(SPIMaster *interface, unsigned retries)
{
    SD_R1 response;
    if (!SD_Command(interface, APP_CMD, 0, sizeof(response), &response)) {
        return false;
    }

    if (response.mask == 0x01) {
        if (!SD_Command(interface, APP_SEND_OP_COND, 0x40000000, sizeof(response), &response)) {
            return false;
        }

        unsigned i;
        for (i = 1; (i < retries) && (response.mask == 0x01); i++) {
            if (!SD_Command(interface, APP_CMD, 0, sizeof(response), &response)
                || !SD_Command(interface, APP_SEND_OP_COND, 0x40000000, sizeof(response), &response)) {
                return false;
            }
        }
    } else if (response.mask == 0x05) {
        unsigned i;
        for (i = 0; (i == 0) || (response.mask == 0x01); i++) {
            if (!SD_Command(interface, SEND_OP_COND, 0, sizeof(response), &response)) {
                return false;
            }
        }
    }

    return (response.mask == 0x00);
}

static bool SD_Initialize(SPIMaster *interface)
{
    // Transfer 74 or more clock pulses to initialize card.
    return (SD_ClockBurst(interface, 74, false)
        && SD_GoIdleState(interface, 5)
        && SD_SendIfCond(interface)
        && SD_SendOpCond(interface, 256));
}

SDCard *SD_Open(SPIMaster *interface)
{
    static SDCard SD_Cards[SD_CARD_MAX] = {0};
    SDCard *card = NULL;
    unsigned c;
    for (c = 0; c < SD_CARD_MAX; c++) {
        if (!SD_Cards[c].interface) {
            card = &SD_Cards[c];
            break;
        }
    }
    if (!card) {
        return NULL;
    }

    #if 1
    // 20200902 taylor
    if (!(timer = GPT_Open(MT3620_UNIT_GPT1, 993, GPT_MODE_ONE_SHOT))) {
        return NULL;
    }
    #endif

    // Configure SPI Master to 400 kHz.
    SPIMaster_Configure(interface, 0, 0, 400000);

    unsigned retries = 5;
    unsigned i;
    for (i = 0; i < retries; i++) {
        if (SD_Initialize(interface)) {
            break;
        }
    }
    if (i >= retries) {
        return NULL;
    }

    #if 1
    SD_ReadCSD_test(interface);
    #else
    // 20200902 taylor

    card->interface    = interface;
    card->blockLen     = 512;
    card->maxTranSpeed = 400000;
    card->tranSpeed    = 400000;

    if (SD_ReadCSD(card) && (card->maxTranSpeed != card->tranSpeed)) {
        if (SPIMaster_Configure(card->interface, 0, 0, card->maxTranSpeed) == ERROR_NONE) {
            card->tranSpeed = card->maxTranSpeed;
        }
    }
    #endif

    return card;
}

void SD_Close(SDCard *card)
{
    card->interface = NULL;
    GPT_Close(timer);
}


uint32_t SD_GetBlockLen(const SDCard *card)
{
    return (card ? card->blockLen : 0);
}

bool SD_SetBlockLen(SDCard *card, uint32_t len)
{
    if (!card || (len == 0)) {
        return false;
    }

    SD_R1 response;
    if (!SD_Command(card->interface, SET_BLOCKLEN, len, sizeof(response), &response)) {
        return false;
    }

    if (response.mask != 0x00) {
        return false;
    }

    card->blockLen = len;
    return true;
}

#if 1
// 20200907 taylor
static bool SD_WriteStopPacket(SPIMaster *interface)
{
    #define DBG_SD_WRITESTOPPACKET
    
    unsigned retries = 65536;
    uint8_t byte = 0xFF;
    unsigned i;

    for (i = 0; (i < retries) && (byte == 0xFF); i++) {
        if (!SPITransfer__AsyncTimeout(interface, &byte, 1, SPI_READ)) {
        #ifdef DBG_SD_WRITESTOPPACKET
            UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
        #endif
            return false;
        }
    }

    byte = 0xFD;

    if (!SPITransfer__AsyncTimeout(interface, &byte, 1, SPI_WRITE)) {
    #ifdef DBG_SD_WRITESTOPPACKET
        UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
    #endif
        return false;
    }

    SD_ClockBurst(interface, 8, true);
    
    return true;

}


static bool SD_WriteNDataPacket(SPIMaster *interface, uintptr_t size, void *data)
{
    #define DBG_SD_WRITENDATAPACKET
    
    unsigned retries = 65536;
    uint8_t byte = 0xFF;
    unsigned i;

    for (i = 0; (i < retries) && (byte == 0xFF); i++) {
        if (!SPITransfer__AsyncTimeout(interface, &byte, 1, SPI_READ)) {
        #ifdef DBG_SD_WRITENDATAPACKET
            UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
        #endif
            return false;
        }
    }

    byte = 0xFC;

    if (!SPITransfer__AsyncTimeout(interface, &byte, 1, SPI_WRITE)) {
    #ifdef DBG_SD_WRITENDATAPACKET
        UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
    #endif
        return false;
    }

    uint8_t* data_byte = data;
    uintptr_t packet = 16;
    uintptr_t remain = size;
    for (remain = size; remain; remain -= packet, data_byte += packet) {
        if (packet > remain) {
            packet = remain;
        }

        if (!SPITransfer__AsyncTimeout(interface, data_byte, packet, SPI_WRITE)) {
        #ifdef DBG_SD_WRITENDATAPACKET
            UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
        #endif
            return false;
        }
    }

    for (i = 0; (i < retries) && (byte != 0x05); i++) {
        if (!SPITransfer__AsyncTimeout(interface, &byte, 1, SPI_READ)) {
        #ifdef DBG_SD_WRITENDATAPACKET
            UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
        #endif
            return false;
        }
        //UART_Printf(debug,"retries %d/%d %s(%d)\r\n", i, retries, __FILE__, __LINE__);
    }

    SD_ClockBurst(interface, 8, true);
    
    return true;

}
#endif

#if 1
// 20200908 taylor
static bool SD_Wait(SPIMaster* interface)
{
    unsigned retries = 65536;
    uint8_t byte = 0xFF;
    unsigned i;

    for (i = 0; (i < retries) && (byte == 0xFF); i++) {
        if (!SPITransfer__AsyncTimeout(interface, &byte, 1, SPI_READ)) {
#ifdef DBG_SD_WRITEDATAPACKET
            UART_Printf(debug, "false %s(%d)\r\n", __FILE__, __LINE__);
#endif
            return false;
        }
    }
}
#endif


#if 1
// 20200831 taylor
static bool SD_WriteDataPacket(SPIMaster *interface, uintptr_t size, void *data)
{
#if 1
    #define DBG_SD_WRITEDATAPACKET

    unsigned retries = 65536;
    uint8_t byte = 0xFF;
    unsigned i;

    for (i = 0; (i < retries) && (byte == 0xFF); i++) {
        if (!SPITransfer__AsyncTimeout(interface, &byte, 1, SPI_READ)) {
            #ifdef DBG_SD_WRITEDATAPACKET
            UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
            #endif
            return false;
        }
    }

    byte = 0xFE;
#if 1
    if (!SPITransfer__AsyncTimeout(interface, &byte, 1, SPI_WRITE)) {
        #ifdef DBG_SD_WRITEDATAPACKET
        UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
        #endif
        return false;
    }
#else
    for (i = 0; (i < retries) && (byte == 0xFE); i++) {
        if (!SPITransfer__AsyncTimeout(card->interface, &byte, 1, SPI_READ)) {
            return false;
        }
    }
#endif

    uint8_t* data_byte = data;
    uintptr_t packet = 16;
    uintptr_t remain = size;
    for (remain = size; remain; remain -= packet, data_byte += packet) {
        if (packet > remain) {
            packet = remain;
        }

        if (!SPITransfer__AsyncTimeout(interface, data_byte, packet, SPI_WRITE)) {
            #ifdef DBG_SD_WRITEDATAPACKET
            UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
            #endif
            return false;
        }
    }
#if 1
    uint16_t crc;
    if (!SPITransfer__AsyncTimeout(interface, &crc, sizeof(crc), SPI_READ)) {
        return false;
    }
#endif

    for (i = 0; (i < retries) && (byte != 0x05); i++) {
        if (!SPITransfer__AsyncTimeout(interface, &byte, 1, SPI_READ)) {
            #ifdef DBG_SD_WRITEDATAPACKET
            UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
            #endif
            return false;
        }
        //UART_Printf(debug,"retries %d/%d %s(%d)\r\n", i, retries, __FILE__, __LINE__);
    }

    #if 1
    SD_ClockBurst(interface, 8, true);
    #else
    byte = 0xFF;

    for (i = 0; (i < retries) && (byte == 0xFF); i++) {
        if (!SPITransfer__AsyncTimeout(interface, &byte, 1, SPI_READ)) {
            #ifdef DBG_SD_WRITEDATAPACKET
            UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
            #endif
            return false;
        }
    }
    #endif
    
#if 0
    SD_ClockBurst(card->interface, 32, true);
#endif
    return true;

#else
    #define DBG_SD_WRITEDATAPACKET
    
    #ifdef DBG_SD_WRITEDATAPACKET
    UART_Printf(debug,"Start DBG SD_WriteDataPacket %s(%d)\r\n", __FILE__, __LINE__);
    #endif

    unsigned retries = 65536;
    uint8_t byte = 0xFF;
    unsigned i;
    
    for (i = 0; (i < retries) && (byte != 0xFF); i++) {
        if (!SPITransfer__AsyncTimeout(interface, &byte, 1, SPI_READ)) {
            #ifdef DBG_SD_WRITEDATAPACKET
            UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
            #endif
            return false;
        }
    }

    byte = 0xFE;
    
    if (!SPITransfer__AsyncTimeout(interface, &byte, 1, SPI_WRITE)) {
      #ifdef DBG_SD_WRITEDATAPACKET
      UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
      #endif
      return false;
    }    

    uint8_t *data_byte = data;
    uintptr_t packet = 16;
    uintptr_t remain = size;
    for (remain = size; remain; remain -= packet, data_byte += packet) {
        if (packet > remain) {
            packet = remain;
        }

        if (!SPITransfer__AsyncTimeout(interface, data_byte, packet, SPI_WRITE)) {
            #ifdef DBG_SD_WRITEDATAPACKET
            UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
            #endif
            return false;
        }
    }

    uint16_t crc;
    if (!SPITransfer__AsyncTimeout(interface, &crc, sizeof(crc), SPI_READ)) {
        #ifdef DBG_SD_WRITEDATAPACKET
        UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
        #endif
        return false;
    }

    // TODO: Verify the CRC.

    #if 1
    // 20200831 taylor
    #if 0
    SD_ClockBurst(interface, 32, true);

    for (i = 0; (i < retries) && (byte != 0x05); i++) {
        if (!SPITransfer__AsyncTimeout(interface, &byte, 1, SPI_READ)) {
            #ifdef DBG_SD_WRITEDATAPACKET
            UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
            #endif
            return false;
        }
    }
    #else
            
    for (i = 0; (i < retries) && (byte != 0x05); i++) {
        if (!SPITransfer__AsyncTimeout(interface, &byte, 1, SPI_READ)) {
            #ifdef DBG_SD_WRITEDATAPACKET
            UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
            #endif
            return false;
        }
        //UART_Printf(debug,"retries %d/%d %s(%d)\r\n", i, retries, __FILE__, __LINE__);
    }

    SD_ClockBurst(interface, 32, true);
    #endif
    
    #else
    // Clock burst is required here to give the card time to recover?
    SD_ClockBurst(card->interface, 32, false);
    #endif

    return true;
#endif
}

#endif

#if 1
// 20200831 taylor
bool SD_WriteBlock(SPIMaster *interface, uint32_t addr, void *data)
{
    #define DBG_SD_WRITEBLOCK

    #ifdef DBG_SD_WRITEBLOCK
    UART_Printf(debug,"Start DBG SD_WriteBlock addr 0x%x %s(%d)\r\n", addr, __FILE__, __LINE__);
    #endif

    if (!interface || !data) {
        return false;
    }

    SD_R1 response;
    if (!SD_CommandIncomplete(interface, WRITE_BLOCK, addr, sizeof(response), &response)) {
        #ifdef DBG_SD_WRITEBLOCK
        UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
        #endif
        return false;
    }

    if (response.mask != 0x00) {
        #ifdef DBG_SD_WRITEBLOCK
        UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
        #endif
        return false;
    }
    UART_Printf(debug,"response.mask 0x%x %s(%d)\r\n", response.mask, __FILE__, __LINE__);

    #ifdef DBG_SD_WRITEBLOCK
    UART_Print(debug,"End DBG SD_WriteBlock\r\n");
    #endif

    return SD_WriteDataPacket(interface, 512, data);
}
#endif

#if 1
// 20200907 taylor
bool SD_WriteNBlock(SPIMaster *interface, uint32_t addr, void *data, uint32_t cnt)
{
    #define DBG_SD_WRITEBLOCK

    #ifdef DBG_SD_WRITEBLOCK
    UART_Printf(debug,"Start DBG SD_WriteBlock addr 0x%x %s(%d)\r\n", addr, __FILE__, __LINE__);
    #endif

    if (!interface || !data) {
        return false;
    }

    SD_R1 response;
    if (!SD_CommandIncomplete(interface, WRITE_MULTIPLE_BLOCK, addr, sizeof(response), &response)) {
        #ifdef DBG_SD_WRITEBLOCK
        UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
        #endif
        return false;
    }

    if (response.mask != 0x00) {
        #ifdef DBG_SD_WRITEBLOCK
        UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
        UART_Printf(debug,"response.mask 0x%x %s(%d)\r\n", response.mask, __FILE__, __LINE__);
        #endif
        return false;
    }

    #ifdef DBG_SD_WRITEBLOCK
    UART_Print(debug,"End DBG SD_WriteBlock\r\n");
    #endif

    do
    {
        if(!SD_WriteNDataPacket(interface, 512, data))
        {
            return false;
        }
        data += 512;
    }
    while (--cnt);

    if(!SD_WriteStopPacket(interface))
    {
        return false;
    }

    return true;
}
#endif


#if 1
// 20200902 taylor
bool SD_ReadBlock(SPIMaster *interface, uint32_t addr, void *data)
{
    #define DBG_SD_READBLOCK

    #ifdef DBG_SD_READBLOCK
    UART_Printf(debug,"Start DBG SD_ReadBlock addr 0x%x %s(%d)\r\n", addr, __FILE__, __LINE__);
    #endif
    
    if (!interface || !data) {
        #ifdef DBG_SD_READBLOCK
        UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
        #endif
        return false;
    }

    SD_R1 response;
    if (!SD_CommandIncomplete(interface, READ_SINGLE_BLOCK, addr, sizeof(response), &response)) {
        #ifdef DBG_SD_READBLOCK
        UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
        #endif
        return false;
    }

    if (response.mask != 0x00) {
        #ifdef DBG_SD_READBLOCK
        UART_Printf(debug,"false %s(%d)\r\n", __FILE__, __LINE__);
        #endif
        return false;
    }

    return SD_ReadDataPacket(interface, 512, data);
}

#else
bool SD_ReadBlock(const SDCard *card, uint32_t addr, void *data)
{
    if (!card || !data) {
        return false;
    }

    SD_R1 response;
    if (!SD_CommandIncomplete(card->interface, READ_SINGLE_BLOCK, addr, sizeof(response), &response)) {
        return false;
    }

    if (response.mask != 0x00) {
        return false;
    }

    return SD_ReadDataPacket(card, card->blockLen, data);
}
#endif

#if 1
// 20200901 taylor

/* slave select */
static void SELECT(void)
{
  int32_t status;

  status = SPIMaster_SelectEnable(driver, true);
  if (status != ERROR_NONE) {
    UART_Printf(debug, "SELECT Error %d\r\n", status);
    UART_Printf(debug, "%s : %d \r\n", __FILE__, __LINE__);
  }

}

/* slave deselect */
static void DESELECT(void)
{
  int32_t status;

  status = SPIMaster_SelectEnable(driver, false);
  if (status != ERROR_NONE) {
    UART_Printf(debug, "DESELECT Error %d\r\n", status);
    UART_Printf(debug, "%s : %d \r\n", __FILE__, __LINE__);
  }

}

/* SPI transmit a byte */
static void SPI_TxByte(uint8_t data)
{
  SPITransfer transfer = {
    .writeData = NULL,
    .readData  = NULL,
    .length    = 1,
  };
  
  int32_t status;

  transfer.writeData = &data;
  status = SPIMaster_TransferSequentialAsync(driver, &transfer, 1, 0);
  if (status != ERROR_NONE) {
    UART_Printf(debug, "SPI_TxByte Error %d\r\n", status);
    UART_Printf(debug, "%s : %d \r\n", __FILE__, __LINE__);
    return false;
  }
}

/* SPI transmit buffer */
static void SPI_TxBuffer(uint8_t *buffer, uint16_t len)
{
  SPITransfer transfer = {
    .writeData = NULL,
    .readData  = NULL,
    .length    = len,
  };
  
  int32_t status;

  transfer.writeData = buffer;
  status = SPIMaster_TransferSequentialAsync(driver, &transfer, 1, 0);
  if (status != ERROR_NONE) {
    UART_Printf(debug, "SPI_TxBuffer Error %d\r\n", status);
    UART_Printf(debug, "%s : %d \r\n", __FILE__, __LINE__);
    return false;
  }

}

/* SPI receive a byte */
static uint8_t SPI_RxByte(void)
{
  uint8_t data;

  SPITransfer transfer = {
    .writeData = NULL,
    .readData  = NULL,
    .length    = 1,
  };
  
  int32_t status;

  transfer.readData = &data;
  status = SPIMaster_TransferSequentialAsync(driver, &transfer, 1, 0);
  if (status != ERROR_NONE) {
    UART_Printf(debug, "SPI_RxByte Error %d\r\n", status);
    UART_Printf(debug, "%s : %d \r\n", __FILE__, __LINE__);
    return false;
  }

  return data;
}

/* SPI receive a byte via pointer */
static void SPI_RxBytePtr(uint8_t *buff) 
{
  SPITransfer transfer = {
    .writeData = NULL,
    .readData  = NULL,
    .length    = 1,
  };
  
  int32_t status;

  transfer.readData = &buff;
  status = SPIMaster_TransferSequentialAsync(driver, &transfer, 1, 0);
  if (status != ERROR_NONE) {
    UART_Printf(debug, "SPI_RxBytePtr Error %d\r\n", status);
    UART_Printf(debug, "%s : %d \r\n", __FILE__, __LINE__);
    return false;
  }
}

/* power on */
static void SD_PowerOn(void) 
{
    //#define DBG_SD_POWERON
    
	uint8_t args[6];
	int32_t cnt = 0x1FFF;
  bool ret;

  int32_t retries = 5;
  int32_t i;
  
	/* transmit bytes to wake up */
  #if 1
  for (i = 0; i < retries; i++)
  {
    ret = SD_ClockBurst(driver, 74, false);
    if(ret == 1)
    {
      break;
    }
    #ifdef DBG_SD_POWERON
    UART_Printf(debug, "SD_ClockBurst Error 0x%x %s : %d \r\n", __FILE__, __LINE__);
    #endif
    if (i >= retries)
    {
      UART_Printf(debug, "SD_ClockBurst Failed 0x%x %s : %d \r\n", __FILE__, __LINE__);
      return NULL;
    }
  }
  #else
	DESELECT();
	for(int i = 0; i < 10; i++)
	{
		SPI_TxByte(0xFF);
	}
  
	/* slave select */
	SELECT();
  #endif

  #if 1
  for (i = 0; i < retries; i++)
  {
    ret = SD_GoIdleState(driver, 5);
    if(ret == 1)
    {
      break;
    }
    #ifdef DBG_SD_POWERON
    UART_Printf(debug, "SD_GoIdleState Error 0x%x %s : %d \r\n", __FILE__, __LINE__);
    #endif
    if (i >= retries)
    {
      UART_Printf(debug, "SD_GoIdleState Failed 0x%x %s : %d \r\n", __FILE__, __LINE__);
      return NULL;
    }
  }
  #else

	/* make idle state */
	args[0] = CMD0;		/* CMD0:GO_IDLE_STATE */
	args[1] = 0;
	args[2] = 0;
	args[3] = 0;
	args[4] = 0;
	args[5] = 0x95;		/* CRC */

	SPI_TxBuffer(args, sizeof(args));

	
	/* wait response */
	#if 0 //teddy 200428
	while ((SPI_RxByte() != 0x01) && cnt)
	{
		cnt--;
	}
	#endif
	while (1)
	{
		if(SPI_RxByte() == 0x01)
			break;
		if(cnt < 0)
		{
			printf("SPI not Respoinse!!  %s : %d \r\n",__FILE__, __LINE__);
			break;
		}	
		cnt--;
	}
  
	DESELECT();
	SPI_TxByte(0XFF);
  #endif

	PowerFlag = 1;
}


DSTATUS SD_disk_initialize (BYTE drv)
{
    //#define DBG_SD_DISK_INITIALIZE
    
  uint8_t n, type, ocr[4];
  uint32_t ret;

    #ifdef DBG_SD_DISK_INITIALIZE
  UART_Printf(debug, "SD_disk_initialize %s : %d \r\n", __FILE__, __LINE__);
    #endif

	/* single drive, drv should be 0 */
	if(drv) return STA_NOINIT;

	/* no disk */
	if(Stat & STA_NODISK) return Stat;

  // Configure SPI Master to 400 kHz.
  ret = SPIMaster_Configure(driver, 0, 0, 400000);
    #ifdef DBG_SD_DISK_INITIALIZE
  UART_Printf(debug, "SPIMaster_Configure ret = %d %s : %d \r\n", ret, __FILE__, __LINE__);
    #endif

  #if 1
  // 20200902 taylor
  if (!(timer = GPT_Open(MT3620_UNIT_GPT1, 993, GPT_MODE_ONE_SHOT))) {
      return NULL;
  }
  #endif

	/* power on */
	SD_PowerOn();

  #if 0
  /* slave select */
	SELECT();
  #endif
  
	/* check disk type */
	type = 0;

  #if 0  
	/* send GO_IDLE_STATE command */
	if (SD_SendCmd(CMD0, 0) == 1)
	{
	
	#endif

  SD_R7 response7;
  if (!SD_Command(driver, SEND_IF_COND, 0x000001AA, sizeof(response7), &response7)) {
      UART_Printf(debug, "SD_Command failed 0x%x %s : %d \r\n", __FILE__, __LINE__);
      return false;
  }

    #ifdef DBG_SD_DISK_INITIALIZE
  UART_Printf(debug, "response7 0x%x %s : %d \r\n", response7.mask, __FILE__, __LINE__);
  UART_Printf(debug, "commandVersion 0x%x %s : %d \r\n", response7.commandVersion, __FILE__, __LINE__);
  UART_Printf(debug, "voltageAccepted 0x%x %s : %d \r\n", response7.voltageAccepted, __FILE__, __LINE__);
  UART_Printf(debug, "checkPattern 0x%x %s : %d \r\n", response7.checkPattern, __FILE__, __LINE__);  
  #endif

  ocr[3] = (response7.mask>>24)&0xff;
  ocr[2] = (response7.mask>>16)&0xff;
  ocr[1] = (response7.mask>>8)&0xff;
  ocr[0] = response7.mask&0xff;

#ifdef DBG_SD_DISK_INITIALIZE
  UART_Printf(debug, "ocr[3] 0x%x %s : %d \r\n", ocr[3], __FILE__, __LINE__);  
  UART_Printf(debug, "ocr[2] 0x%x %s : %d \r\n", ocr[2], __FILE__, __LINE__);  
  UART_Printf(debug, "ocr[1] 0x%x %s : %d \r\n", ocr[1], __FILE__, __LINE__);  
  UART_Printf(debug, "ocr[0] 0x%x %s : %d \r\n", ocr[0], __FILE__, __LINE__);  
  #endif

  SD_R1 response = response7.r1;
  if (response.mask == 0x01) {
      if (__builtin_bswap32(response7.mask) != 0x000001AA) {
          UART_Printf(debug, "Error %s : %d \r\n", __FILE__, __LINE__);
      }
  } else if (response.mask != 0x05) {
      UART_Printf(debug, "Error %s : %d \r\n", __FILE__, __LINE__);
  }

    #ifdef DBG_SD_DISK_INITIALIZE
  UART_Printf(debug, "response 0x%x %s : %d \r\n", response.mask, __FILE__, __LINE__);
    #endif

  /* voltage range 2.7-3.6V */
	if (ocr[2] == 0x01 && ocr[3] == 0xAA)
  {
    if(false == SD_SendOpCond(driver, 256))
    {
      UART_Printf(debug, "SD_SendOpCond failed %s : %d \r\n", __FILE__, __LINE__);
    }

    SD_R3 response3;
    if (!SD_Command(driver, READ_OCR, 0, sizeof(response3), &response3)) {
        UART_Printf(debug, "SD_Command failed 0x%x %s : %d \r\n", __FILE__, __LINE__);
        return false;
    }

    #ifdef DBG_SD_DISK_INITIALIZE
    UART_Printf(debug, "response3.ocr 0x%x %s : %d \r\n", response3.ocr, __FILE__, __LINE__);
    UART_Printf(debug, "response3.r1.mask 0x%x %s : %d \r\n", response3.r1.mask, __FILE__, __LINE__);
    #endif

    ocr[3] = (response3.ocr>>24)&0xff;
    ocr[2] = (response3.ocr>>16)&0xff;
    ocr[1] = (response3.ocr>>8)&0xff;
    ocr[0] = response3.ocr&0xff;

    #ifdef DBG_SD_DISK_INITIALIZE
    UART_Printf(debug, "ocr[3] 0x%x %s : %d \r\n", ocr[3], __FILE__, __LINE__);
    UART_Printf(debug, "ocr[2] 0x%x %s : %d \r\n", ocr[2], __FILE__, __LINE__);
    UART_Printf(debug, "ocr[1] 0x%x %s : %d \r\n", ocr[1], __FILE__, __LINE__);
    UART_Printf(debug, "ocr[0] 0x%x %s : %d \r\n", ocr[0], __FILE__, __LINE__);
    #endif

    /* SDv2 (HC or SC) */
		type = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;
    #ifdef DBG_SD_DISK_INITIALIZE
    UART_Printf(debug, "type 0x%x %s : %d \r\n", type, __FILE__, __LINE__);
    #endif
  }

  CardType = type;

  /* Clear STA_NOINIT */
	if (type)
	{
		Stat &= ~STA_NOINIT;
	}
	else
	{
		/* Initialization failed */
		PowerFlag = 0;
	}

	return Stat;

  #if 0
  }
  #endif

  


}

DSTATUS SD_disk_status (BYTE pdrv)
{
    //#define DBG_SD_DISK_STATUS

    #ifdef DBG_SD_DISK_STATUS
  UART_Printf(debug, "SD_disk_status %s : %d \r\n", __FILE__, __LINE__);
    #endif

  if (pdrv) return STA_NOINIT;
	return Stat;

}

DRESULT SD_disk_read (BYTE pdrv, BYTE* buff, DWORD sector, UINT count)
{
    //#define DBG_SD_DISK_READ

    #ifdef DBG_SD_DISK_READ
  UART_Printf(debug, "Start SD_disk_read %s : %d \r\n", __FILE__, __LINE__);
    #endif

  /* pdrv should be 0 */
	if (pdrv || !count) return RES_PARERR;

	/* no disk */
	if (Stat & STA_NOINIT) return RES_NOTRDY;

	/* convert to byte address */
	if (!(CardType & CT_SD2)) sector *= 512;

	if (count == 1)
	{
	#ifdef DBG_SD_DISK_READ
      UART_Printf(debug, "count %d sector %d %s : %d \r\n", count, sector, __FILE__, __LINE__);
    #endif
		/* READ_SINGLE_BLOCK */
		//if ((SD_SendCmd(CMD17, sector) == 0) && SD_RxDataBlock(buff, 512)) count = 0;
        if(true == SD_ReadBlock(driver, sector, buff))
        {
#ifdef DBG_SD_DISK_READ
            UART_Printf(debug, "SD_ReadBlock finished %s : %d \r\n", __FILE__, __LINE__);
#endif
            count = 0;
        }
        else
        {
            UART_Printf(debug, "SD_ReadBlock failed %s : %d \r\n", __FILE__, __LINE__);
        }
	}
	else
	{
	#ifdef DBG_SD_DISK_READ
      UART_Printf(debug, "count %d %s : %d \r\n", count, __FILE__, __LINE__);
    #endif
    
        #ifdef DBG_SD_DISK_READ
        UART_Printf(debug, "End SD_disk_read %s : %d \r\n", __FILE__, __LINE__);
        #endif
    
        return RES_ERROR;
	#if 0
		/* READ_MULTIPLE_BLOCK */
		if (SD_SendCmd(CMD18, sector) == 0)
		{
			do {
				if (!SD_RxDataBlock(buff, 512)) break;
				buff += 512;
			} while (--count);

			/* STOP_TRANSMISSION */
			SD_SendCmd(CMD12, 0);
		}
  #endif
	}

    #ifdef DBG_SD_DISK_READ
    UART_Printf(debug, "End SD_disk_read %s : %d \r\n\r\n", __FILE__, __LINE__);
    #endif

	return count ? RES_ERROR : RES_OK;

}

DRESULT SD_disk_write (BYTE pdrv, const BYTE* buff, DWORD sector, UINT count)
{
    #define DBG_SD_DISK_WRITE

    #ifdef DBG_SD_DISK_WRITE
    UART_Printf(debug, "Start SD_disk_write %s : %d \r\n", __FILE__, __LINE__);
    #endif

  /* pdrv should be 0 */
	if (pdrv || !count) return RES_PARERR;

	/* no disk */
	if (Stat & STA_NOINIT) return RES_NOTRDY;

	/* write protection */
	if (Stat & STA_PROTECT) return RES_WRPRT;

	/* convert to byte address */
	if (!(CardType & CT_SD2)) sector *= 512;

	if (count == 1)
	{
	#ifdef DBG_SD_DISK_WRITE
      UART_Printf(debug, "count %d sector %d %s : %d \r\n", count, sector, __FILE__, __LINE__);
    #endif
    
	  #if 1
    if(true == SD_WriteBlock(driver, sector, buff))
    {
    #ifdef DBG_SD_DISK_WRITE
      UART_Printf(debug, "SD_WriteBlock finished %s : %d \r\n", __FILE__, __LINE__);
    #endif
      count = 0;
    }
    else
    {
      UART_Printf(debug, "SD_WriteBlock failed %s : %d \r\n", __FILE__, __LINE__);
    }
    #else
		/* WRITE_BLOCK */
		if ((SD_SendCmd(CMD24, sector) == 0) && SD_TxDataBlock(buff, 0xFE))
			count = 0;
    #endif
	}
	else
	{
	    #ifdef DBG_SD_DISK_WRITE
        UART_Printf(debug, "count %d %s : %d \r\n", count, __FILE__, __LINE__);
        #endif
        
        #ifdef DBG_SD_DISK_WRITE
        UART_Printf(debug, "End SD_disk_write %s : %d \r\n", __FILE__, __LINE__);
        #endif
        
	    #if 1

        do
        {
            if(true == SD_WriteBlock(driver, sector, buff))
            {
                buff += 512;
                sector ++;
            }
            else
            {
                count = 1;
                break;
            }
        }while(--count);
        
        

        #if 1
        #else
        if(false == SD_WriteNBlock(driver, sector, buff, count))
        {
            count = 1;
        }
        else
        {
            count = 0;
        }
        #endif

    #else
		/* WRITE_MULTIPLE_BLOCK */
		if (CardType & CT_SD1)
		{
			SD_SendCmd(CMD55, 0);
			SD_SendCmd(CMD23, count); /* ACMD23 */
		}

		if (SD_SendCmd(CMD25, sector) == 0)
		{
			do {
				if(!SD_TxDataBlock(buff, 0xFC)) break;
				buff += 512;
			} while (--count);

			/* STOP_TRAN token */
			if(!SD_TxDataBlock(0, 0xFD))
			{
				count = 1;
			}
		}
    #endif
	}  

    #ifdef DBG_SD_DISK_WRITE
    UART_Printf(debug, "End SD_disk_write %s : %d \r\n\r\n", __FILE__, __LINE__);
    #endif

	return count ? RES_ERROR : RES_OK;

}

DRESULT SD_disk_ioctl (BYTE pdrv, BYTE cmd, void* buff)
{
    #define DBG_SD_DISK_IOCTL

    #ifdef DBG_SD_DISK_IOCTL
  UART_Printf(debug, "SD_disk_ioctl %s : %d \r\n", __FILE__, __LINE__);
    #endif

  DRESULT res;
	uint8_t n, csd[16], *ptr = buff;
	WORD csize;
  SD_R1 response;

	/* pdrv should be 0 */
	if (pdrv) return RES_PARERR;
	res = RES_ERROR;

    #ifdef DBG_SD_DISK_IOCTL
  UART_Printf(debug, "cmd 0x%x %s : %d \r\n", cmd, __FILE__, __LINE__);
    #endif

	if (cmd == CTRL_POWER)
	{
		switch (*ptr)
		{
		case 0:
			PowerFlag = 0;  /* Power Off */
			res = RES_OK;
			break;
		case 1:
			SD_PowerOn();		/* Power On */
			res = RES_OK;
			break;
		case 2:
			*(ptr + 1) = PowerFlag;
			res = RES_OK;		/* Power Check */
			break;
		default:
			res = RES_PARERR;
		}
	}
	else
	{
		/* no disk */
		if (Stat & STA_NOINIT) return RES_NOTRDY;

		switch (cmd)
		{
		case GET_SECTOR_COUNT:
      #if 1
      
      if (!SD_CommandIncomplete(driver, SEND_CSD, 0, sizeof(response), &response)) {
        UART_Printf(debug, "SD_CommandIncomplete failed %s : %d \r\n", __FILE__, __LINE__);
        return false;
      }
      if ((response.mask & 0xC0) != 0) {
        UART_Printf(debug, "SD_CommandIncomplete failed %s : %d \r\n", __FILE__, __LINE__);
        return false;
      }

      uint8_t csd[16];
      if (!SD_ReadDataPacket(driver, sizeof(csd), csd)) {
        UART_Printf(debug, "SD_CommandIncomplete failed %s : %d \r\n", __FILE__, __LINE__);
        return false;
      }

        if ((csd[0] >> 6) == 1)
        {
        	/* SDC V2 */
        	csize = csd[9] + ((WORD) csd[8] << 8) + 1;
        	*(DWORD*) buff = (DWORD) csize << 10;
        }
        else
        {
        	/* MMC or SDC V1 */
        	n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
        	csize = (csd[8] >> 6) + ((WORD) csd[7] << 2) + ((WORD) (csd[6] & 3) << 10) + 1;
        	*(DWORD*) buff = (DWORD) csize << (n - 9);
        }
        res = RES_OK;

      uint8_t tranSpeedRaw = csd[3];

      unsigned tranSpeedUnitRaw = (tranSpeedRaw & 0x07);
      unsigned tranSpeedUnit = 10000;
      for (; tranSpeedUnitRaw > 0; tranSpeedUnit *= 10, tranSpeedUnitRaw--);

      unsigned tranSpeedValueRaw = ((tranSpeedRaw >> 3) & 0xF);
      static unsigned tranSpeedValueTable[16] = {
           0, 10, 12, 13,
          15, 20, 25, 30,
          35, 40, 45, 50,
          55, 60, 70, 80,
      };
      unsigned tranSpeedValue = tranSpeedValueTable[tranSpeedValueRaw];

      #if 0
    // 20200902 taylor

    card->interface    = interface;
    card->blockLen     = 512;
    card->maxTranSpeed = 400000;
    card->tranSpeed    = 400000;

    if (SD_ReadCSD(card) && (card->maxTranSpeed != card->tranSpeed)) {
        if (SPIMaster_Configure(card->interface, 0, 0, card->maxTranSpeed) == ERROR_NONE) {
            card->tranSpeed = card->maxTranSpeed;
        }
    }
    #endif

      if ((tranSpeedValue != 0)
          && ((tranSpeedRaw & 0x80) == 0)) {
          #if 1
          #ifdef DBG_SD_DISK_IOCTL
          UART_Printf(debug, "maxTranSpeed 0x%x %s : %d \r\n", tranSpeedValue * tranSpeedUnit, __FILE__, __LINE__);
          #endif

        if (SPIMaster_Configure(driver, 0, 0, tranSpeedValue * tranSpeedUnit) != ERROR_NONE)
        {
            UART_Printf(debug, "SPIMaster_Configure failed %s : %d \r\n", __FILE__, __LINE__);
        }
          #else
          card->maxTranSpeed = ;
          #endif
      }

      //return true;
      #else
			/* SEND_CSD */
			if ((SD_SendCmd(CMD9, 0) == 0) && SD_RxDataBlock(csd, 16))
			{
				if ((csd[0] >> 6) == 1)
				{
					/* SDC V2 */
					csize = csd[9] + ((WORD) csd[8] << 8) + 1;
					*(DWORD*) buff = (DWORD) csize << 10;
				}
				else
				{
					/* MMC or SDC V1 */
					n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
					csize = (csd[8] >> 6) + ((WORD) csd[7] << 2) + ((WORD) (csd[6] & 3) << 10) + 1;
					*(DWORD*) buff = (DWORD) csize << (n - 9);
				}
				res = RES_OK;
			}
      #endif
			break;
		case GET_SECTOR_SIZE:
			*(WORD*) buff = 512;
			res = RES_OK;
			break;
		case CTRL_SYNC:
      #if 1
            if(SD_Wait(driver))
            {
                res = RES_OK;
            }
            else
            {
                UART_Printf(debug, "CTRL_SYNC failed %s : %d \r\n", __FILE__, __LINE__);
                res = RES_ERROR;
            }
      #else
      
			if (SD_ReadyWait() == 0xFF) res = RES_OK;
      #endif
			break;
		case MMC_GET_CSD:
			/* SEND_CSD */
    #if 0
			if (SD_SendCmd(CMD9, 0) == 0 && SD_RxDataBlock(ptr, 16)) res = RES_OK;
      #endif
			break;
		case MMC_GET_CID:
			/* SEND_CID */
    #if 0
			if (SD_SendCmd(CMD10, 0) == 0 && SD_RxDataBlock(ptr, 16)) res = RES_OK;
      #endif
			break;
		case MMC_GET_OCR:      
			/* READ_OCR */
    #if 0
			if (SD_SendCmd(CMD58, 0) == 0)
			{
				for (n = 0; n < 4; n++)
				{
					*ptr++ = SPI_RxByte();
				}
				res = RES_OK;
			}
      #endif
      
		case GET_BLOCK_SIZE:
			*(WORD*) buff = 0x762c00;
			res = RES_OK;
			break;
		default:
			res = RES_PARERR;
		}
	}

	return res;

}


#endif

#if 1
// 20201014 taylor
// from CodethinkLabs
// Commit: 144f6a9b2e22301c29405cdeae62126d79251b64 [144f6a9]

bool SD_WriteBlock(SDCard *card, uint32_t addr, const void *data)
{
    if (!card || !data) {
        return false;
    }

    SD_R1 response;
    if (!SD_CommandIncomplete(card->interface, WRITE_BLOCK, addr, sizeof(response), &response)) {
        return false;
    }

    if (response.mask != 0x00) {
        return false;
    }

    return SD_WriteDataPacket(card, card->blockLen, data);
}
#endif
