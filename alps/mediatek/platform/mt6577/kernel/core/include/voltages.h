/*****************************************************************************
*
* Filename:
* ---------
*   voltages.h
*
* Project:
* --------
*   Android
*
* Description:
* ------------
*   mtk 6577 1.0 GHz voltages definitions
*
* Author:
* -------
*
****************************************************************************/

#ifndef __VOLTAGES_H__
#define __VOLTAGES_H__

// 0x17: 1.275V
// 0x13: 1.175V
// 0x0F: 1.075V
// 0x08: 0.9V (sleep mode)

#define VOLTAGE_S_UV_1 (0x08)
#define VOLTAGE_0_UV_1 (0x07)
#define VOLTAGE_1_UV_1 (0x0D)
#define VOLTAGE_2_UV_1 (0x11)
#define VOLTAGE_3_UV_1 (0x15)

#define VOLTAGE_S_UV_2 (0x07)
#define VOLTAGE_0_UV_2 (0x07)
#define VOLTAGE_1_UV_2 (0x0D)
#define VOLTAGE_2_UV_2 (0x10)
#define VOLTAGE_3_UV_2 (0x14)

#define VOLTAGE_S_UV_3 (0x06)
#define VOLTAGE_0_UV_3 (0x06)
#define VOLTAGE_1_UV_3 (0x0D)
#define VOLTAGE_2_UV_3 (0x10)
#define VOLTAGE_3_UV_3 (0x13)

// not working -> freeze
#define VOLTAGE_S_UV_4 (0x06)
#define VOLTAGE_0_UV_4 (0x06)
#define VOLTAGE_1_UV_4 (0x0F)
#define VOLTAGE_2_UV_4 (0x0F)
#define VOLTAGE_3_UV_4 (0x0F)

// sleep and 1GHz -> working fine
#define VOLTAGE_S_UV_5 (0x05)
#define VOLTAGE_0_UV_5 (0x05)
#define VOLTAGE_1_UV_5 (0x0D)
#define VOLTAGE_2_UV_5 (0x10)
#define VOLTAGE_3_UV_5 (0x12)

// next try: sleep
#define VOLTAGE_S_UV_6 (0x04)
#define VOLTAGE_0_UV_6 (0x04)
#define VOLTAGE_1_UV_6 (0x0D)
#define VOLTAGE_2_UV_6 (0x10)
#define VOLTAGE_3_UV_6 (0x12)

// next try: wakeup from sleep - screen black issue fix try...
#define VOLTAGE_S_UV_7 (0x04)
#define VOLTAGE_0_UV_7 (0x08)
#define VOLTAGE_1_UV_7 (0x0F)
#define VOLTAGE_2_UV_7 (0x12)
#define VOLTAGE_3_UV_7 (0x12)

// next try: reduce lag - may have something to do with voltage... this are the "varun uv values except the sleep value; for overclock use different values
#define VOLTAGE_S_UV_8 (0x04)
#define VOLTAGE_0_UV_8 (0x04)
#define VOLTAGE_1_UV_8 (0x0F)
#define VOLTAGE_2_UV_8 (0x13)
#define VOLTAGE_3_UV_8 (0x17)

#define VOLTAGE_S_DEF (0x08)
#define VOLTAGE_0_DEF (0x08)
#define VOLTAGE_1_DEF (0x0F)
#define VOLTAGE_2_DEF (0x13)
#define VOLTAGE_3_DEF (0x17)

// VOLTAGE_S && VOLTAGE_0
// not working minimum: UNKNOWN
// working: 0x05

// VOLTAGE_3
// not working minimum for VOLTAGE_3 (max freq) is 0x0F -> freeze
// working: 0x12

#define VOLTAGE_S VOLTAGE_S_UV_8
#define VOLTAGE_0 VOLTAGE_0_UV_8
#define VOLTAGE_1 VOLTAGE_1_UV_8
#define VOLTAGE_2 VOLTAGE_2_UV_8
#define VOLTAGE_3 VOLTAGE_3_UV_8

// OC: not active in sleep mode, maybe only use in VOLTAGE_3/1 Ghz mode; maybe use higher voltages...
#define VOLTAGE_S_OC VOLTAGE_S
#define VOLTAGE_0_OC VOLTAGE_0
#define VOLTAGE_1_OC VOLTAGE_1
#define VOLTAGE_2_OC VOLTAGE_2
#define VOLTAGE_3_OC (0x17)

#endif // __VOLTAGES_H__

