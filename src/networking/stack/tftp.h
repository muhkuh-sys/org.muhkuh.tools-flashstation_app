/***************************************************************************
 *   Copyright (C) 2005, 2006, 2007, 2008, 2009 by Hilscher GmbH           *
 *                                                                         *
 *   Author: Christoph Thelen (cthelen@hilscher.com)                       *
 *                                                                         *
 *   Redistribution or unauthorized use without expressed written          *
 *   agreement from the Hilscher GmbH is forbidden.                        *
 ***************************************************************************/


#include "eth.h"


#ifndef __TFTP_H__
#define __TFTP_H__

#define TFTP_EOF (-1)


#define TFTP_OPCODE_ReadReq     MUS2NUS(1)
#define TFTP_OPCODE_WriteReq    MUS2NUS(2)
#define TFTP_OPCODE_Data        MUS2NUS(3)
#define TFTP_OPCODE_Ack         MUS2NUS(4)
#define TFTP_OPCODE_Error       MUS2NUS(5)
#define TFTP_OPCODE_OptionAck   MUS2NUS(6)


typedef enum
{
	TFTP_ERR_NotDefined                     = MUS2NUS(0),
	TFTP_ERR_FileNotFound                   = MUS2NUS(1),
	TFTP_ERR_AccessViolation                = MUS2NUS(2),
	TFTP_ERR_DiskFull                       = MUS2NUS(3),
	TFTP_ERR_IllegalOperation               = MUS2NUS(4),
	TFTP_ERR_UnknownTransferId              = MUS2NUS(5),
	TFTP_ERR_FileAlreadyExists              = MUS2NUS(6),
	TFTP_ERR_NoSuchUser                     = MUS2NUS(7)
} tTftpError;


typedef enum
{
	TFTPSTATE_Idle          = 0,
	TFTPSTATE_Init          = 1,            /* sent the open packet and waiting for first data packet */
	TFTPSTATE_Transfer      = 2,            /* data transfer phase */
	TFTPSTATE_Finished      = 3,            /* transfer completed */
	TFTPSTATE_Error         = 4
} tTftpState;


typedef struct
{
	unsigned long ulIp;
	unsigned int uiPort;
} tTftpSettings;


tTftpState tftp_getState(void);

void tftp_init(void);

int tftp_open(const char *pcFileName, unsigned char *pucDst, unsigned int uiBlockSize);
void tftp_timer(void);

unsigned char *tftp_getDataPointer(void);
unsigned long tftp_getLoadedBytes(void);
unsigned long *tftp_getHash(void);


#endif  /* __TFTP_H__ */
