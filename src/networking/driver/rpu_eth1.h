
#ifndef ___RPU_ETH1_H__
#define ___RPU_ETH1_H__

#include <stdint.h>

extern const uint32_t BuildTime_rpu_eth1[7];
extern const uint32_t XcCode_rpu_eth1[151];


#define PrgSiz_rpu_eth1        XcCode_rpu_eth1[0]
#define TrlSiz_rpu_eth1        XcCode_rpu_eth1[1]

#define PrgSrt_rpu_eth1        &XcCode_rpu_eth1[2]
#define PrgStp_rpu_eth1        &XcCode_rpu_eth1[129]

#define TrlSrt_rpu_eth1        &XcCode_rpu_eth1[129]
#define TrlStp_rpu_eth1        &XcCode_rpu_eth1[151]

#endif  /* ___RPU_ETH1_H__ */

