#ifndef __BLINKI_CODES_H__
#define __BLINKI_CODES_H__


/*
 *       | off | YEL | END | GRN |
 * ------+-----+-----+-----+-----+
 * mask  |  0  |  1  |  0  |  1  |
 * state |  0  |  0  |  1  |  1  |
 */


/*
 *    00000000001111111111222222222233
 *    01234567890123456789012345678901
 *    YY__YY______________*
 *
 *  M 110011000000000000000
 *  S 000000000000000000001
 *    \__/\__/\__/\__/\__/\__/\__/\__/
 */

/* The mask for the blink sequence YY__YY______________* */
#define BLINKI_M_ETH_WAIT_FOR_LINK_UP        0x00000033
/* The state for the blink sequence YY__YY______________* */
#define BLINKI_S_ETH_WAIT_FOR_LINK_UP        0x00100000


/*
 *    00000000001111111111222222222233
 *    01234567890123456789012345678901
 *    YY__________*
 *
 *  M 11000000000000000000000000000000
 *  S 00000000000010000000000000000000
 *    \__/\__/\__/\__/\__/\__/\__/\__/
 */

/* The mask for the blink sequence YY__________* */
#define BLINKI_M_ROM_ACK_FAILED              0x00000003
/* The state for the blink sequence YY__________* */
#define BLINKI_S_ROM_ACK_FAILED              0x00001000


/*
 *    00000000001111111111222222222233
 *    01234567890123456789012345678901
 *    YY__YY__YY__________*
 *
 *  M 11001100110000000000000000000000
 *  S 00000000000000000000100000000000
 *    \__/\__/\__/\__/\__/\__/\__/\__/
 */

/* The mask for the blink sequence YY__YY__YY__________* */
#define BLINKI_M_FDL_READ_FAILED             0x00000333
/* The state for the blink sequence YY__YY__YY__________* */
#define BLINKI_S_FDL_READ_FAILED             0x00100000


/*
 *    00000000001111111111222222222233
 *    01234567890123456789012345678901
 *    YY__YY__YY__YY__________*
 *
 *  M 11001100110011000000000000000000
 *  S 00000000000000000000000010000000
 *    \__/\__/\__/\__/\__/\__/\__/\__/
 */

/* The mask for the blink sequence YY__YY__YY__YY__________* */
#define BLINKI_M_DOWNLOAD_DEVICEINFO_FAILED  0x00003333
/* The state for the blink sequence YY__YY__YY__YY__________* */
#define BLINKI_S_DOWNLOAD_DEVICEINFO_FAILED  0x01000000


/*
 *    00000000001111111111222222222233
 *    01234567890123456789012345678901
 *    YY__YY__YY__YY___YY_________*
 *
 *  M 11001100110011000110000000000000
 *  S 00000000000000000000000000001000
 *    \__/\__/\__/\__/\__/\__/\__/\__/
 */

/* The mask for the blink sequence YY__YY__YY__YY___YY_________* */
#define BLINKI_M_DOWNLOAD_SWFP_FAILED        0x00033333
/* The state for the blink sequence YY__YY__YY__YY___YY_________* */
#define BLINKI_S_DOWNLOAD_SWFP_FAILED        0x10000000


/*
 *    00000000001111111111222222222233
 *    01234567890123456789012345678901
 *    G_*
 *
 *  M 100
 *  S 101
 *    \__/\__/\__/\__/\__/\__/\__/\__/
 */

/* The mask for the blink sequence G_* */
#define BLINKI_M_FLASHING_OK                 0x00000001
/* The state for the blink sequence G_* */
#define BLINKI_S_FLASHING_OK                 0x00000005


/*
 *    00000000001111111111222222222233
 *    01234567890123456789012345678901
 *    YY__*
 *
 *  M 11000000000000000000000000000000
 *  S 00001000000000000000000000000000
 *    \__/\__/\__/\__/\__/\__/\__/\__/
 */

/* The mask for the blink sequence YY__* */
#define BLINKI_M_FLASHING_FAILED             0x00000003
/* The state for the blink sequence YY__* */
#define BLINKI_S_FLASHING_FAILED             0x00000010


#endif  /* __BLINKI_CODES_H__ */
