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

// next try: "wakeup from sleep - screen black issue" fix try...
#define VOLTAGE_S_UV_7 (0x04)
#define VOLTAGE_0_UV_7 (0x08)
#define VOLTAGE_1_UV_7 (0x0F)
#define VOLTAGE_2_UV_7 (0x12)
#define VOLTAGE_3_UV_7 (0x12)

// next try: reduce lag - may have something to do with voltage... this are the "varun uv values except the sleep value; for overclock use different values
#define VOLTAGE_S_UV_8 (0x04)
#define VOLTAGE_0_UV_8 (0x04) // this may have caused "wakeup from sleep - screen black issue", it's back...
#define VOLTAGE_1_UV_8 (0x0F)
#define VOLTAGE_2_UV_8 (0x13)
#define VOLTAGE_3_UV_8 (0x17)

// next try: reduce lag - may have something to do with voltage... this are the "varun uv values except the sleep value; for overclock use different values
// "wakeup from sleep - screen black issue", it's still there, back to "varun" values
// still there with varuns values, it is sensors issue!
#define VOLTAGE_S_UV_9 (0x04)
#define VOLTAGE_0_UV_9 (0x08)
#define VOLTAGE_1_UV_9 (0x0F)
#define VOLTAGE_2_UV_9 (0x13)
#define VOLTAGE_3_UV_9 (0x17)

// sleep, baby sleep...
#define VOLTAGE_S_UV_10 (0x02)
#define VOLTAGE_0_UV_10 (0x08)
#define VOLTAGE_1_UV_10 (0x0F)
#define VOLTAGE_2_UV_10 (0x13)
#define VOLTAGE_3_UV_10 (0x17) // antutu 8451[817,599]/8276[803,630] (clean/ksm+zram)

// cap max voltages (middle voltages are nearly unused, so only deep sleep and 1GHz have a real effect
#define VOLTAGE_S_UV_11 (0x02)
#define VOLTAGE_0_UV_11 (0x08)
#define VOLTAGE_1_UV_11 (0x0F)
#define VOLTAGE_2_UV_11 (0x12)
#define VOLTAGE_3_UV_11 (0x12) // antutu 8296[814,624]/? (clean/ksm+zram) [] -> top/top values!
// result: slow, bad response, may be not related to voltages, but...

// sleep, baby sleep...
#define VOLTAGE_S_UV_12 (0x02)
#define VOLTAGE_0_UV_12 (0x08)
#define VOLTAGE_1_UV_12 (0x0F)
#define VOLTAGE_2_UV_12 (0x13)
#define VOLTAGE_3_UV_12 (0x1B) // 1B is absolute maximum but will be capped. antutu 8302[806,636]/8466[812,653] (clean/ksm+zram)

#define VOLTAGE_S_DEF (0x08)
#define VOLTAGE_0_DEF (0x08)
#define VOLTAGE_1_DEF (0x0F)
#define VOLTAGE_2_DEF (0x13)
#define VOLTAGE_3_DEF (0x17)



// VOLTAGE_S && VOLTAGE_0
// not working minimum: UNKNOWN
// working: 0x04

// VOLTAGE_3
// not working minimum for VOLTAGE_3 (max freq) is 0x0F -> freeze
// working: 0x12

// this are the actual used values
// todo: get lowest TOP-Voltage with no antutu score decrease; test if score is heigher with stock value
#define VOLTAGE_S VOLTAGE_S_UV_11
#define VOLTAGE_0 VOLTAGE_0_UV_11
#define VOLTAGE_1 VOLTAGE_1_UV_11
#define VOLTAGE_2 VOLTAGE_2_UV_11
#define VOLTAGE_3 VOLTAGE_3_UV_11

#endif // __VOLTAGES_H__

