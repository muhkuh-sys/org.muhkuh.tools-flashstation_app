/***************************************************************************
 *   Copyright (C) 2005, 2006, 2007, 2008, 2009 by Hilscher GmbH           *
 *                                                                         *
 *   Author: Christoph Thelen (cthelen@hilscher.com)                       *
 *                                                                         *
 *   Redistribution or unauthorized use without expressed written          *
 *   agreement from the Hilscher GmbH is forbidden.                        *
 ***************************************************************************/


#include "rng.h"

#include "netx_io_areas.h"


/* netx4000 has no rng unit, use the systime and the OTP fuses instead. */

unsigned long rng_get_value(void)
{
	HOSTDEF(ptSystime0Area);
	HOSTDEF(ptRAPSysctrlArea);
	unsigned long ulValue;


	/* Get the combination of seconds and nanoseconds. */
	ulValue  = ptSystime0Area->ulSystime_s;
	ulValue ^= ptSystime0Area->ulSystime_ns;
	/* Add the chip ID. */
	ulValue ^= ptRAPSysctrlArea->aulRAP_SYSCTRL_CHIP_ID_[0];
	ulValue ^= ptRAPSysctrlArea->aulRAP_SYSCTRL_CHIP_ID_[1];
	ulValue ^= ptRAPSysctrlArea->aulRAP_SYSCTRL_CHIP_ID_[2];
	ulValue ^= ptRAPSysctrlArea->aulRAP_SYSCTRL_CHIP_ID_[3];

	return ulValue;
}

