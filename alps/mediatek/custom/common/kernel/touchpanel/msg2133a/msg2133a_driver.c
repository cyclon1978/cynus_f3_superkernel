/* Copyright Statement:
*
* This software/firmware and related documentation ("MediaTek Software") are
* protected under relevant copyright laws. The information contained herein
* is confidential and proprietary to MediaTek Inc. and/or its licensors.
* Without the prior written permission of MediaTek inc. and/or its licensors,
* any reproduction, modification, use or disclosure of MediaTek Software,
* and information contained herein, in whole or in part, shall be strictly prohibited.
*/
/* MediaTek Inc. (C) 2010. All rights reserved.
*
* BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
* THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
* RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
* AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
* NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
* SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
* SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
* THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
* THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
* CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
* SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
* STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
* CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
* AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
* OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
* MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*
* The following software/firmware and/or related documentation ("MediaTek Software")
* have been modified by MediaTek Inc. All revisions are subject to any receiver's
* applicable license agreements with MediaTek Inc.
*/


/* edit by Magnum 2012-6-1 
   some advices below, if you can not understand this driver code easily .
   1. refer to some other drivers code ,such as cypress.
   2. read the code separately according to the macro(hong) such as  TP_FIRMWARE_UPDATE and 
      CONFIG_TOUCHSCREEN_MSG2133_SUPPORT_ISP
   3. if you want a ctp working ,you can delete the macros which do other things.
      you should just power on and  enable the i2c device ,the most important it that 
      set the right slave address.

   4. you can upgrade tp firmware both apk and mstart_tpupgrade .
      CONFIG_TOUCHSCREEN_MSG2133_SUPPORT_ISP make that mstar_tpupgrade useful,
      TP_FIRMWARE_UPDATE enable that apk upgrade firmware.
      CONFIG_TOUCHSCREEN_MSG2133_SUPPORT_ISP depends on TP_FIRMWARE_UPDATE.
*/

/*  edit by Magnum 2012-8-11
    Mstar CTP can simulate Promxity Sensor. when you in a call ,the code will send a special register to msg2133 ic,
    and ic will go into another state that can change state when your face near the screen.
    if you want to add the func,you must do two things below:
    1: you shoule make sure the mobile load the msg2133 tp driver.
    2: just value CUSTOM_KERNEL_ALSPS = msg2133_alsps in ProjectConfig.
    you can  achieve it. 
*/

#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include "tpd.h"
#include <cust_eint.h>
#include <linux/rtpm_prio.h>
#include <linux/wakelock.h>
#include <asm/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include "tpd_custom_msg2133.h"
#include "cust_gpio_usage.h"

#ifdef MT6575
#include <mach/mt_boot.h>
#include <mach/mt_pm_ldo.h>
#endif

#ifdef MT6577
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#endif

#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
#include <linux/input/sweep2wake.h>
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif 

//#define CTP_SIMULATE_PS
#ifdef CTP_SIMULATE_PS  //edit by Magnum 2012-7-27
#include <linux/sensors_io.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/hwmsen_helper.h>
static char ps_mode = -1;   // 1: turn on , 0: turn off , -1:default
static char ps_data = 1;   // 1: far,      0: close    , 1 :default
static char ps_data_pre = 1;   // 1: far,      0: close    , 1 :default
static int tpd_load_status_ok= -1 ; // 1: probe ok , other: probe other ctp driver.
extern void ctp_ps_event_handler(void);
extern bool power_pressed;

enum
{
	DISABLE_CTP_PS,
	ENABLE_CTP_PS,
};
#endif
 
extern struct tpd_device *tpd;
 
static struct i2c_client *i2c_client = NULL;
static struct task_struct *mythread = NULL;
 
static DECLARE_WAIT_QUEUE_HEAD(waiter);
 
static struct early_suspend early_suspend;
extern struct tpd_device *tpd;

#ifdef CONFIG_HAS_EARLYSUSPEND
static void tpd_early_suspend(struct early_suspend *handler);
static void tpd_late_resume(struct early_suspend *handler);
#endif 

#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
static int s2w_st_flag=0;
#endif

 
extern void mt65xx_eint_unmask(unsigned int line);
extern void mt65xx_eint_mask(unsigned int line);
extern void mt65xx_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 mt65xx_eint_set_sens(kal_uint8 eintno, kal_bool sens);
extern void mt65xx_eint_registration(kal_uint8 eintno, kal_bool Dbounce_En,
									  kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
									  kal_bool auto_umask);
void msg2133_tpd_eint_interrupt_handler(void);
static int __devinit tpd_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
static int __devexit tpd_remove(struct i2c_client *client);
static int touch_event_handler(void *unused);
static int tpd_initialize(struct i2c_client * client);
static void tpd_reset(void);
static void powerkey_call_reset(void);

static int ctp_suspend = 1;  //edit by Magnum 2012-10-23 resume depend on suspend .please attention it. 
static int tpd_flag = 0;
#define TPD_OK 0
#define MSG_TPIO_RESET      GPIO_CTP_RST_PIN
//#define MSG_TPIO_WAKEUP   GPIO_CTP_WAKE_PIN		//Power Down
//#define MSG_TPIO_TPEN       GPIO152		//Power enable
#define MSG_TPIO_EINT     GPIO_CTP_EINT_PIN
#define	CTP_ID_MSG21XX		0
#define	CTP_ID_MSG21XXA		1
static int CTP_IC_TYPE = CTP_ID_MSG21XX;

static const int TPD_KEYSFACTORY[TPD_KEY_COUNT] =  TPD_FACTORYKEYS;
static int msg2133_ts_get_fw_version(void);
#ifdef TPD_HAVE_BUTTON 
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
extern void tpd_button(unsigned int x, unsigned int y, unsigned int down);

#endif
				 
#define U8 unsigned char
#define S8 signed char
#define U16 unsigned short
#define S16 signed short
#define U32 unsigned int
#define S32 signed int
#define TOUCH_ADDR_MSG21XX   0x26//0x4C

//firmware
#define FW_ADDR_MSG21XX      0x62  	//0xC4
static char vendor_version = 0;
static char panel_version = 0;

#define CONFIG_TOUCHSCREEN_MSG2133_SUPPORT_ISP

static int point_num = 0;
static int p_point_num = 0;
static int key_index = -1;

struct touch_info
{
	unsigned short y[3];
	unsigned short x[3];
	unsigned short p[3];
	unsigned short count;
};

typedef struct
{
	unsigned short pos_x;
	unsigned short pos_y;
	unsigned short pos_x2;
	unsigned short pos_y2;
	unsigned short temp2;
	unsigned short temp;
	short dst_x;
	short dst_y;
	unsigned char checksum;
} SHORT_TOUCH_STATE;

 static const struct i2c_device_id msg2133_i2c_id[] = {{TPD_DEVICE,0},{}};
 static struct i2c_board_info __initdata msg2133_i2c_tpd={ I2C_BOARD_INFO(TPD_DEVICE, TOUCH_ADDR_MSG21XX )};
 static int boot_mode = 0;
 static int key_event_flag=0;

static void powerkey_call_reset()
{
	TPD_DEBUG("===============Magnum_TPD RESET.....\n");
	mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_EINT_PIN, GPIO_OUT_ZERO);
	hwPowerDown(MT65XX_POWER_LDO_VGP2, "TP");
	mt_set_gpio_mode(MSG_TPIO_RESET, 0x00);		//GPIO mode
	mt_set_gpio_pull_enable(MSG_TPIO_RESET, 1);	//external pull up???
	mt_set_gpio_pull_select(MSG_TPIO_RESET, GPIO_PULL_DOWN);
	mt_set_gpio_dir(MSG_TPIO_RESET,1);
	mt_set_gpio_out(MSG_TPIO_RESET,0);  //
	msleep(10);
	hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_2800, "TP");
	msleep(10);
	mt_set_gpio_mode(MSG_TPIO_RESET, 0x00);		//GPIO mode
	mt_set_gpio_pull_enable(MSG_TPIO_RESET, 1);	//external pull up???
	mt_set_gpio_pull_select(MSG_TPIO_RESET, GPIO_PULL_UP);
	mt_set_gpio_dir(MSG_TPIO_RESET,1);
	mt_set_gpio_out(MSG_TPIO_RESET,1);
	msleep(100);
	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

}

static void tpd_reset(void)
{

	TPD_DEBUG("===============Magnum_TPD RESET.....\n");
	mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_EINT_PIN, GPIO_OUT_ZERO);
	hwPowerDown(MT65XX_POWER_LDO_VGP2, "TP");
	mt_set_gpio_mode(MSG_TPIO_RESET, 0x00);		//GPIO mode
	mt_set_gpio_pull_enable(MSG_TPIO_RESET, 1);	//external pull up???
	mt_set_gpio_pull_select(MSG_TPIO_RESET, GPIO_PULL_DOWN);
	mt_set_gpio_dir(MSG_TPIO_RESET,1);
	mt_set_gpio_out(MSG_TPIO_RESET,0);  //
	msleep(10);
	hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_2800, "TP");
	msleep(10);
	mt_set_gpio_mode(MSG_TPIO_RESET, 0x00);		//GPIO mode
	mt_set_gpio_pull_enable(MSG_TPIO_RESET, 1);	//external pull up???
	mt_set_gpio_pull_select(MSG_TPIO_RESET, GPIO_PULL_UP);
	mt_set_gpio_dir(MSG_TPIO_RESET,1);
	mt_set_gpio_out(MSG_TPIO_RESET,1);
	msleep(300);
	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

}

 static void tpd_down(int x, int y, int p) {
	 input_report_abs(tpd->dev, ABS_PRESSURE, p);
	 input_report_key(tpd->dev, BTN_TOUCH, 1);
	 input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 128);
	 input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	 input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
	 //TPD_DEBUG("D[%4d %4d %4d] ", x, y, p);
	 input_mt_sync(tpd->dev);
	 TPD_DOWN_DEBUG_TRACK(x,y);

printk("[SWEEP2WAKE]: tpd down\n");
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
		if (sweep2wake) {
printk("[SWEEP2WAKE]: detecting sweep\n");
			detect_sweep2wake(x, y, jiffies, p);
		}
#endif

 }

 static int tpd_up(int x, int y,int *count) {
printk("[SWEEP2WAKE]: inside tpd up\n");
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
s2w_st_flag = 0;
				if (sweep2wake > 0) {
					printk("[sweep2wake]:line : %d | func : %s\n", __LINE__, __func__);
printk("[SWEEP2WAKE]: resetin s2w param\n");
					printk("[sweep2wake]:line : %d | func : %s\n", __LINE__, __func__);
					exec_count = true;
					barrier[0] = false;
					barrier[1] = false;
					scr_on_touch = false;
					tripoff = 0;
					tripon = 0;
					triptime = 0;
				}
				if (doubletap2wake && scr_suspended) {
printk("[SWEEP2WAKE]: detecting d2w\n");
					doubletap2wake_func(x, y, jiffies);
				}
#endif


	 if(*count>0) {
		 input_report_abs(tpd->dev, ABS_PRESSURE, 0);
		 input_report_key(tpd->dev, BTN_TOUCH, 0);
		 input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
		 input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
		 input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
		 //TPD_DEBUG("U[%4d %4d %4d] ", x, y, 0);
		 input_mt_sync(tpd->dev);
		 TPD_UP_DEBUG_TRACK(x,y);
		 (*count)--;
		 return 1;
	 } return 0;
 }


#define __FIRMWARE_UPDATE__
#ifdef __FIRMWARE_UPDATE__

/*adair:0777Ϊ\B4\F2\BF\AAapk\C9\FD\BC\B6\B9\A6\C4ܣ\AC0664Ϊ\B9ر\D5apk\C9\FD\BC\B6\B9\A6\C4ܣ\AC\CE\DE\D0轫\BA\EA__FIRMWARE_UPDATE__\B9ر\D5*/
#define CTP_AUTHORITY 0777//0664

//#define ENABLE_AUTO_UPDATA
#if 0
#define TP_DEBUG(format, ...)	printk(KERN_INFO "MSG2133_MSG21XXA_update_INFO***" format "\n", ## __VA_ARGS__)
#else
#define TP_DEBUG(format, ...)
#endif
#if 0//adair:\D5\FDʽ\B0汾\B9ر\D5
#define TP_DEBUG_ERR(format, ...)	printk(KERN_ERR "MSG2133_MSG21XXA_update_ERR***" format "\n", ## __VA_ARGS__)
#else
#define TP_DEBUG_ERR(format, ...)
#endif
static  char *fw_version;
static int update_switch = 0;
static u8 temp[94][1024];
//u8  Fmr_Loader[1024];
u32 crc_tab[256];
static u8 g_dwiic_info_data[1024];   // Buffer for info data

static int FwDataCnt;
struct class *firmware_class;
struct device *firmware_cmd_dev;

#define N_BYTE_PER_TIME (8)//adair:1024\B5\C4Լ\CA\FD,\B8\F9\BE\DDƽ̨\D0޸\C4
#define UPDATE_TIMES (1024/N_BYTE_PER_TIME)

#if 0//adair:\B8\F9\BE\DDƽ̨\B2\BBͬѡ\D4\F1\B2\BBͬλ\B5\C4i2c\B5\D8ַ
#define FW_ADDR_MSG21XX   (0xC4)
#define FW_ADDR_MSG21XX_TP   (0x4C)
#define FW_UPDATE_ADDR_MSG21XX   (0x92)
#else
#define FW_ADDR_MSG21XX   (0xC4>>1)
#define FW_ADDR_MSG21XX_TP   (0x4C>>1)
#define FW_UPDATE_ADDR_MSG21XX   (0x92>>1)
#endif

/*adair:\D2\D4\CF\C25\B8\F6\D2\D4Hal\BF\AAͷ\B5ĺ\AF\CA\FD\D0\E8Ҫ\B8\F9\BE\DDƽ̨\D0޸\C4*/
/*disable irq*/
static void HalDisableIrq(void)
{
	mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	 mt65xx_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_TOUCH_PANEL_POLARITY, NULL, 1);
}
/*enable irq*/
static void HalEnableIrq(void)
{
	mt65xx_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_TOUCH_PANEL_POLARITY, msg2133_tpd_eint_interrupt_handler, 1);
	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
}
/*reset the chip*/
static void _HalTscrHWReset(void)
{
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_EN_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, 0);
	msleep(120);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, 1);
	msleep(300);
}
static void HalTscrCReadI2CSeq(u8 addr, u8* read_data, u16 size)
{
    int ret;
    i2c_client->addr = addr;
    ret = i2c_master_recv(i2c_client, read_data, size);
    i2c_client->addr = FW_ADDR_MSG21XX_TP;

    if(ret <= 0)
    {
		TP_DEBUG_ERR("HalTscrCReadI2CSeq error %d,addr = %d\n", ret,addr);
	}
}

static void i2c_write(u8 addr, u8 *pbt_buf, int dw_lenth)
{
	int ret;
	i2c_client->addr = addr;
	i2c_client->addr|=I2C_ENEXT_FLAG;
	ret = i2c_master_send(i2c_client, pbt_buf, dw_lenth);
	i2c_client->addr = FW_ADDR_MSG21XX_TP;
	i2c_client->addr|=I2C_ENEXT_FLAG;

	if(ret <= 0)
	{
		printk("i2c_write_interface error line = %d, ret = %d\n", __LINE__, ret);
	}
}

static int HalTscrCDevWriteI2CSeq(u8 addr, u8* data, u16 size)
{
	int ret;
	i2c_client->addr = addr;
	ret = i2c_master_send(i2c_client, data, size);
	i2c_client->addr = FW_ADDR_MSG21XX_TP;

	if(ret <= 0)
	{
		TP_DEBUG_ERR("HalTscrCDevWriteI2CSeq error %d,addr = %d\n", ret,addr);
	}
	return ret;
}


static int dbbusDWIICEnterSerialDebugMode(void)
{
    u8 data[5];

    // Enter the Serial Debug Mode
    data[0] = 0x53;
    data[1] = 0x45;
    data[2] = 0x52;
    data[3] = 0x44;
    data[4] = 0x42;
    return HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, data, 5);
}

static void dbbusDWIICStopMCU(void)
{
    u8 data[1];

    // Stop the MCU
    data[0] = 0x37;
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, data, 1);
}

static void dbbusDWIICIICUseBus(void)
{
    u8 data[1];

    // IIC Use Bus
    data[0] = 0x35;
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, data, 1);
}

static void dbbusDWIICIICReshape(void)
{
    u8 data[1];

    // IIC Re-shape
    data[0] = 0x71;
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, data, 1);
}

static void dbbusDWIICIICNotUseBus(void)
{
    u8 data[1];

    // IIC Not Use Bus
    data[0] = 0x34;
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, data, 1);
}

static void dbbusDWIICNotStopMCU(void)
{
    u8 data[1];

    // Not Stop the MCU
    data[0] = 0x36;
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, data, 1);
}

static void dbbusDWIICExitSerialDebugMode(void)
{
    u8 data[1];

    // Exit the Serial Debug Mode
    data[0] = 0x45;
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, data, 1);

    // Delay some interval to guard the next transaction
    udelay ( 150);//200 );        // delay about 0.2ms
}

static void drvISP_EntryIspMode(void)
{
    u8 bWriteData[5] =
    {
        0x4D, 0x53, 0x54, 0x41, 0x52
    };
	TP_DEBUG("\n******%s come in*******\n",__FUNCTION__);
    HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, bWriteData, 5);
    udelay ( 150 );//200 );        // delay about 0.1ms
}

static u8 drvISP_Read(u8 n, u8* pDataToRead)    //First it needs send 0x11 to notify we want to get flash data back.
{
    u8 Read_cmd = 0x11;
    unsigned char dbbus_rx_data[2] = {0};
    HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &Read_cmd, 1);
    //msctpc_LoopDelay ( 1 );        // delay about 100us*****
    udelay( 800 );//200);
    if (n == 1)
    {
        HalTscrCReadI2CSeq(FW_UPDATE_ADDR_MSG21XX, &dbbus_rx_data[0], 2);
        *pDataToRead = dbbus_rx_data[0];
        TP_DEBUG("dbbus=%d,%d===drvISP_Read=====\n",dbbus_rx_data[0],dbbus_rx_data[1]);
  	}
    else
    {
        HalTscrCReadI2CSeq(FW_UPDATE_ADDR_MSG21XX, pDataToRead, n);
    }

    return 0;
}

static void drvISP_WriteEnable(void)
{
    u8 bWriteData[2] =
    {
        0x10, 0x06
    };
    u8 bWriteData1 = 0x12;
    HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, bWriteData, 2);
    udelay(150);//1.16
    HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1);
}


static void drvISP_ExitIspMode(void)
{
    u8 bWriteData = 0x24;
    HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData, 1);
    udelay( 150 );//200);
}

static u8 drvISP_ReadStatus(void)
{
    u8 bReadData = 0;
    u8 bWriteData[2] =
    {
        0x10, 0x05
    };
    u8 bWriteData1 = 0x12;

    HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, bWriteData, 2);
    //msctpc_LoopDelay ( 1 );        // delay about 100us*****
    udelay(150);//200);
    drvISP_Read(1, &bReadData);
    HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1);
    return bReadData;
}


static void drvISP_BlockErase(u32 addr)
{
	u8 bWriteData[5] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
	u8 bWriteData1 = 0x12;
	TP_DEBUG("\n******%s come in*******\n",__FUNCTION__);
	u32 timeOutCount=0;
	drvISP_WriteEnable();

	//Enable write status register
	bWriteData[0] = 0x10;
	bWriteData[1] = 0x50;
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, bWriteData, 2);
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1);

	//Write Status
	bWriteData[0] = 0x10;
	bWriteData[1] = 0x01;
	bWriteData[2] = 0x00;
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, bWriteData, 3);
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1);

	//Write disable
	bWriteData[0] = 0x10;
	bWriteData[1] = 0x04;
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, bWriteData, 2);
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1);
	//msctpc_LoopDelay ( 1 );        // delay about 100us*****
	udelay(150);//200);
	timeOutCount=0;
	while ( ( drvISP_ReadStatus() & 0x01 ) == 0x01 )
	{
		timeOutCount++;
		if ( timeOutCount >= 100000 ) break; /* around 1 sec timeout */
	}
	drvISP_WriteEnable();

	bWriteData[0] = 0x10;
	bWriteData[1] = 0xC7;//0xD8;        //Block Erase
	//bWriteData[2] = ((addr >> 16) & 0xFF) ;
	//bWriteData[3] = ((addr >> 8) & 0xFF) ;
	//bWriteData[4] = (addr & 0xFF) ;
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, bWriteData, 2);
	//HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData, 5);
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1);
		//msctpc_LoopDelay ( 1 );        // delay about 100us*****
	udelay(150);//200);
	timeOutCount=0;
	while ( ( drvISP_ReadStatus() & 0x01 ) == 0x01 )
	{
		timeOutCount++;
		if ( timeOutCount >= 500000 ) break; /* around 5 sec timeout */
	}
}

static void drvISP_Program(u16 k, u8* pDataToWrite)
{
    u16 i = 0;
    u16 j = 0;
    //u16 n = 0;
    u8 TX_data[133];
    u8 bWriteData1 = 0x12;
    u32 addr = k * 1024;
		u32 timeOutCount=0;
    for (j = 0; j < 8; j++)   //128*8 cycle
    {
        TX_data[0] = 0x10;
        TX_data[1] = 0x02;// Page Program CMD
        TX_data[2] = (addr + 128 * j) >> 16;
        TX_data[3] = (addr + 128 * j) >> 8;
        TX_data[4] = (addr + 128 * j);
        for (i = 0; i < 128; i++)
        {
            TX_data[5 + i] = pDataToWrite[j * 128 + i];
        }
        //msctpc_LoopDelay ( 1 );        // delay about 100us*****
        udelay(150);//200);

        timeOutCount=0;
		while ( ( drvISP_ReadStatus() & 0x01 ) == 0x01 )
		{
			timeOutCount++;
			if ( timeOutCount >= 100000 ) break; /* around 1 sec timeout */
		}

        drvISP_WriteEnable();
        HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, TX_data, 133);   //write 133 byte per cycle
        HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1);
    }
}

static ssize_t firmware_update_show ( struct device *dev,
                                      struct device_attribute *attr, char *buf )
{
    return sprintf ( buf, "%s\n", fw_version );
}


static void drvISP_Verify ( u16 k, u8* pDataToVerify )
{
    u16 i = 0, j = 0;
    u8 bWriteData[5] ={ 0x10, 0x03, 0, 0, 0 };
    u8 RX_data[256];
    u8 bWriteData1 = 0x12;
    u32 addr = k * 1024;
    u8 index = 0;
    u32 timeOutCount;
    for ( j = 0; j < 8; j++ ) //128*8 cycle
    {
        bWriteData[2] = ( u8 ) ( ( addr + j * 128 ) >> 16 );
        bWriteData[3] = ( u8 ) ( ( addr + j * 128 ) >> 8 );
        bWriteData[4] = ( u8 ) ( addr + j * 128 );
        udelay ( 100 );        // delay about 100us*****

        timeOutCount = 0;
        while ( ( drvISP_ReadStatus() & 0x01 ) == 0x01 )
        {
            timeOutCount++;
            if ( timeOutCount >= 100000 ) break; /* around 1 sec timeout */
        }

        HalTscrCDevWriteI2CSeq ( FW_UPDATE_ADDR_MSG21XX, bWriteData, 5 ); //write read flash addr
        udelay ( 100 );        // delay about 100us*****
        drvISP_Read ( 128, RX_data );
        HalTscrCDevWriteI2CSeq ( FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1 ); //cmd end
        for ( i = 0; i < 128; i++ ) //log out if verify error
        {
            if ( ( RX_data[i] != 0 ) && index < 10 )
            {
                //TP_DEBUG("j=%d,RX_data[%d]=0x%x\n",j,i,RX_data[i]);
                index++;
            }
            if ( RX_data[i] != pDataToVerify[128 * j + i] )
            {
                TP_DEBUG ( "k=%d,j=%d,i=%d===============Update Firmware Error================", k, j, i );
            }
        }
    }
}

static void drvISP_ChipErase()
{
    u8 bWriteData[5] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
    u8 bWriteData1 = 0x12;
    u32 timeOutCount = 0;
    drvISP_WriteEnable();

    //Enable write status register
    bWriteData[0] = 0x10;
    bWriteData[1] = 0x50;
    HalTscrCDevWriteI2CSeq ( FW_UPDATE_ADDR_MSG21XX, bWriteData, 2 );
    HalTscrCDevWriteI2CSeq ( FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1 );

    //Write Status
    bWriteData[0] = 0x10;
    bWriteData[1] = 0x01;
    bWriteData[2] = 0x00;
    HalTscrCDevWriteI2CSeq ( FW_UPDATE_ADDR_MSG21XX, bWriteData, 3 );
    HalTscrCDevWriteI2CSeq ( FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1 );

    //Write disable
    bWriteData[0] = 0x10;
    bWriteData[1] = 0x04;
    HalTscrCDevWriteI2CSeq ( FW_UPDATE_ADDR_MSG21XX, bWriteData, 2 );
    HalTscrCDevWriteI2CSeq ( FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1 );
    udelay ( 100 );        // delay about 100us*****
    timeOutCount = 0;
    while ( ( drvISP_ReadStatus() & 0x01 ) == 0x01 )
    {
        timeOutCount++;
        if ( timeOutCount >= 100000 ) break; /* around 1 sec timeout */
    }
    drvISP_WriteEnable();

    bWriteData[0] = 0x10;
    bWriteData[1] = 0xC7;

    HalTscrCDevWriteI2CSeq ( FW_UPDATE_ADDR_MSG21XX, bWriteData, 2 );
    HalTscrCDevWriteI2CSeq ( FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1 );
    udelay ( 100 );        // delay about 100us*****
    timeOutCount = 0;
    while ( ( drvISP_ReadStatus() & 0x01 ) == 0x01 )
    {
        timeOutCount++;
        if ( timeOutCount >= 500000 ) break; /* around 5 sec timeout */
    }
}

/* update the firmware part, used by apk*/
/*show the fw version*/

static void firmware_update_c2 ()
{
    u8 i;
    u8 dbbus_tx_data[4];
    unsigned char dbbus_rx_data[2] = {0};

    // set FRO to 50M
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x11;
    dbbus_tx_data[2] = 0xE2;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
    dbbus_rx_data[0] = 0;
    dbbus_rx_data[1] = 0;
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
    TP_DEBUG ( "dbbus_rx_data[0]=0x%x", dbbus_rx_data[0] );
    dbbus_tx_data[3] = dbbus_rx_data[0] & 0xF7;  //Clear Bit 3
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // set MCU clock,SPI clock =FRO
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x22;
    dbbus_tx_data[3] = 0x00;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x23;
    dbbus_tx_data[3] = 0x00;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // Enable slave's ISP ECO mode
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x08;
    dbbus_tx_data[2] = 0x0c;
    dbbus_tx_data[3] = 0x08;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // Enable SPI Pad
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x02;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
    TP_DEBUG ( "dbbus_rx_data[0]=0x%x", dbbus_rx_data[0] );
    dbbus_tx_data[3] = ( dbbus_rx_data[0] | 0x20 ); //Set Bit 5
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // WP overwrite
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x0E;
    dbbus_tx_data[3] = 0x02;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // set pin high
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x10;
    dbbus_tx_data[3] = 0x08;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    dbbusDWIICIICNotUseBus();
    dbbusDWIICNotStopMCU();
    dbbusDWIICExitSerialDebugMode();

    drvISP_EntryIspMode();
    drvISP_ChipErase();
    _HalTscrHWReset();
    mdelay ( 300 );

    // Program and Verify
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();

    // Disable the Watchdog
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x3C;
    dbbus_tx_data[2] = 0x60;
    dbbus_tx_data[3] = 0x55;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x3C;
    dbbus_tx_data[2] = 0x61;
    dbbus_tx_data[3] = 0xAA;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    //Stop MCU
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x0F;
    dbbus_tx_data[2] = 0xE6;
    dbbus_tx_data[3] = 0x01;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // set FRO to 50M
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x11;
    dbbus_tx_data[2] = 0xE2;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
    dbbus_rx_data[0] = 0;
    dbbus_rx_data[1] = 0;
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
    TP_DEBUG ( "dbbus_rx_data[0]=0x%x", dbbus_rx_data[0] );
    dbbus_tx_data[3] = dbbus_rx_data[0] & 0xF7;  //Clear Bit 3
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // set MCU clock,SPI clock =FRO
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x22;
    dbbus_tx_data[3] = 0x00;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x23;
    dbbus_tx_data[3] = 0x00;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // Enable slave's ISP ECO mode
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x08;
    dbbus_tx_data[2] = 0x0c;
    dbbus_tx_data[3] = 0x08;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // Enable SPI Pad
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x02;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
    TP_DEBUG ( "dbbus_rx_data[0]=0x%x", dbbus_rx_data[0] );
    dbbus_tx_data[3] = ( dbbus_rx_data[0] | 0x20 ); //Set Bit 5
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // WP overwrite
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x0E;
    dbbus_tx_data[3] = 0x02;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // set pin high
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x10;
    dbbus_tx_data[3] = 0x08;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    dbbusDWIICIICNotUseBus();
    dbbusDWIICNotStopMCU();
    dbbusDWIICExitSerialDebugMode();

    ///////////////////////////////////////
    // Start to load firmware
    ///////////////////////////////////////
    drvISP_EntryIspMode();

    for ( i = 0; i < 94; i++ ) // total  94 KB : 1 byte per R/W
    {
        drvISP_Program ( i, temp[i] ); // program to slave's flash
        drvISP_Verify ( i, temp[i] ); //verify data
    }
    TP_DEBUG_ERR ( "update_C2 OK\n" );
    drvISP_ExitIspMode();
    _HalTscrHWReset();
    FwDataCnt = 0;
    HalEnableIrq();
  //  return size;
}

static u32 Reflect ( u32 ref, char ch ) //unsigned int Reflect(unsigned int ref, char ch)
{
    u32 value = 0;
    u32 i = 0;

    for ( i = 1; i < ( ch + 1 ); i++ )
    {
        if ( ref & 1 )
        {
            value |= 1 << ( ch - i );
        }
        ref >>= 1;
    }
    return value;
}

u32 Get_CRC ( u32 text, u32 prevCRC, u32 *crc32_table )
{
    u32  ulCRC = prevCRC;
	ulCRC = ( ulCRC >> 8 ) ^ crc32_table[ ( ulCRC & 0xFF ) ^ text];
    return ulCRC ;
}
static void Init_CRC32_Table ( u32 *crc32_table )
{
    u32 magicnumber = 0x04c11db7;
    u32 i = 0, j;

    for ( i = 0; i <= 0xFF; i++ )
    {
        crc32_table[i] = Reflect ( i, 8 ) << 24;
        for ( j = 0; j < 8; j++ )
        {
            crc32_table[i] = ( crc32_table[i] << 1 ) ^ ( crc32_table[i] & ( 0x80000000L ) ? magicnumber : 0 );
        }
        crc32_table[i] = Reflect ( crc32_table[i], 32 );
    }
}

typedef enum
{
	EMEM_ALL = 0,
	EMEM_MAIN,
	EMEM_INFO,
} EMEM_TYPE_t;

static void drvDB_WriteReg8Bit ( u8 bank, u8 addr, u8 data )
{
    u8 tx_data[4] = {0x10, bank, addr, data};
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, tx_data, 4 );
}

static void drvDB_WriteReg ( u8 bank, u8 addr, u16 data )
{
    u8 tx_data[5] = {0x10, bank, addr, data & 0xFF, data >> 8};
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, tx_data, 5 );
}

static unsigned short drvDB_ReadReg ( u8 bank, u8 addr )
{
    u8 tx_data[3] = {0x10, bank, addr};
    u8 rx_data[2] = {0};

    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, tx_data, 3 );
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &rx_data[0], 2 );
    return ( rx_data[1] << 8 | rx_data[0] );
}

static int drvTP_erase_emem_c32 ( void )
{
    /////////////////////////
    //Erase  all
    /////////////////////////

    //enter gpio mode
    drvDB_WriteReg ( 0x16, 0x1E, 0xBEAF );

    // before gpio mode, set the control pin as the orginal status
    drvDB_WriteReg ( 0x16, 0x08, 0x0000 );
    drvDB_WriteReg8Bit ( 0x16, 0x0E, 0x10 );
    mdelay ( 10 ); //MCR_CLBK_DEBUG_DELAY ( 10, MCU_LOOP_DELAY_COUNT_MS );

    // ptrim = 1, h'04[2]
    drvDB_WriteReg8Bit ( 0x16, 0x08, 0x04 );
    drvDB_WriteReg8Bit ( 0x16, 0x0E, 0x10 );
    mdelay ( 10 ); //MCR_CLBK_DEBUG_DELAY ( 10, MCU_LOOP_DELAY_COUNT_MS );

    // ptm = 6, h'04[12:14] = b'110
    drvDB_WriteReg8Bit ( 0x16, 0x09, 0x60 );
    drvDB_WriteReg8Bit ( 0x16, 0x0E, 0x10 );

    // pmasi = 1, h'04[6]
    drvDB_WriteReg8Bit ( 0x16, 0x08, 0x44 );
    // pce = 1, h'04[11]
    drvDB_WriteReg8Bit ( 0x16, 0x09, 0x68 );
    // perase = 1, h'04[7]
    drvDB_WriteReg8Bit ( 0x16, 0x08, 0xC4 );
    // pnvstr = 1, h'04[5]
    drvDB_WriteReg8Bit ( 0x16, 0x08, 0xE4 );
    // pwe = 1, h'04[9]
    drvDB_WriteReg8Bit ( 0x16, 0x09, 0x6A );
    // trigger gpio load
    drvDB_WriteReg8Bit ( 0x16, 0x0E, 0x10 );

    return ( 1 );
}

static void firmware_update_c32 ( EMEM_TYPE_t emem_type )
{
    u8  dbbus_tx_data[4];
    u8  dbbus_rx_data[2] = {0};
      // Buffer for slave's firmware

    u32 i, j;
    u32 crc_main, crc_main_tp;
    u32 crc_info, crc_info_tp;
    u16 reg_data = 0;

    crc_main = 0xffffffff;
    crc_info = 0xffffffff;

#if 1
    /////////////////////////
    // Erase  all
    /////////////////////////
    drvTP_erase_emem_c32();
    mdelay ( 1000 ); //MCR_CLBK_DEBUG_DELAY ( 1000, MCU_LOOP_DELAY_COUNT_MS );

    //ResetSlave();
    _HalTscrHWReset();
    //drvDB_EnterDBBUS();
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay ( 300 );

    // Reset Watchdog
    drvDB_WriteReg8Bit ( 0x3C, 0x60, 0x55 );
    drvDB_WriteReg8Bit ( 0x3C, 0x61, 0xAA );

    /////////////////////////
    // Program
    /////////////////////////

    //polling 0x3CE4 is 0x1C70
    do
    {
        reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
    }
    while ( reg_data != 0x1C70 );


    drvDB_WriteReg ( 0x3C, 0xE4, 0xE38F );  // for all-blocks

    //polling 0x3CE4 is 0x2F43
    do
    {
        reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
    }
    while ( reg_data != 0x2F43 );


    //calculate CRC 32
    Init_CRC32_Table ( &crc_tab[0] );

    for ( i = 0; i < 33; i++ ) // total  33 KB : 2 byte per R/W
    {
        if ( i < 32 )   //emem_main
        {
            if ( i == 31 )
            {
                temp[i][1014] = 0x5A; //Fmr_Loader[1014]=0x5A;
                temp[i][1015] = 0xA5; //Fmr_Loader[1015]=0xA5;

                for ( j = 0; j < 1016; j++ )
                {
                    //crc_main=Get_CRC(Fmr_Loader[j],crc_main,&crc_tab[0]);
                    crc_main = Get_CRC ( temp[i][j], crc_main, &crc_tab[0] );
                }
            }
            else
            {
                for ( j = 0; j < 1024; j++ )
                {
                    //crc_main=Get_CRC(Fmr_Loader[j],crc_main,&crc_tab[0]);
                    crc_main = Get_CRC ( temp[i][j], crc_main, &crc_tab[0] );
                }
            }
        }
        else  // emem_info
        {
            for ( j = 0; j < 1024; j++ )
            {
                //crc_info=Get_CRC(Fmr_Loader[j],crc_info,&crc_tab[0]);
                crc_info = Get_CRC ( temp[i][j], crc_info, &crc_tab[0] );
            }
        }

        //drvDWIIC_MasterTransmit( DWIIC_MODE_DWIIC_ID, 1024, Fmr_Loader );
        HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX_TP, temp[i], 1024 );

        // polling 0x3CE4 is 0xD0BC
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
        }
        while ( reg_data != 0xD0BC );

        drvDB_WriteReg ( 0x3C, 0xE4, 0x2F43 );
    }

    //write file done
    drvDB_WriteReg ( 0x3C, 0xE4, 0x1380 );

    mdelay ( 10 ); //MCR_CLBK_DEBUG_DELAY ( 10, MCU_LOOP_DELAY_COUNT_MS );
    // polling 0x3CE4 is 0x9432
    do
    {
        reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
    }
    while ( reg_data != 0x9432 );

    crc_main = crc_main ^ 0xffffffff;
    crc_info = crc_info ^ 0xffffffff;

    // CRC Main from TP
    crc_main_tp = drvDB_ReadReg ( 0x3C, 0x80 );
    crc_main_tp = ( crc_main_tp << 16 ) | drvDB_ReadReg ( 0x3C, 0x82 );

    //CRC Info from TP
    crc_info_tp = drvDB_ReadReg ( 0x3C, 0xA0 );
    crc_info_tp = ( crc_info_tp << 16 ) | drvDB_ReadReg ( 0x3C, 0xA2 );

    TP_DEBUG ( "crc_main=0x%x, crc_info=0x%x, crc_main_tp=0x%x, crc_info_tp=0x%x\n",
               crc_main, crc_info, crc_main_tp, crc_info_tp );

    //drvDB_ExitDBBUS();
    if ( ( crc_main_tp != crc_main ) || ( crc_info_tp != crc_info ) )
    {
        TP_DEBUG_ERR ( "update_C32 FAILED\n" );
		_HalTscrHWReset();
        FwDataCnt = 0;
    	HalEnableIrq();
  //      return ( 0 );
    }

    TP_DEBUG_ERR ( "update_C32 OK\n" );
	_HalTscrHWReset();
    FwDataCnt = 0;
	HalEnableIrq();

 //   return size;
#endif
}

static int drvTP_erase_emem_c33 ( EMEM_TYPE_t emem_type )
{
    // stop mcu
    drvDB_WriteReg ( 0x0F, 0xE6, 0x0001 );

    //disable watch dog
    drvDB_WriteReg8Bit ( 0x3C, 0x60, 0x55 );
    drvDB_WriteReg8Bit ( 0x3C, 0x61, 0xAA );

    // set PROGRAM password
    drvDB_WriteReg8Bit ( 0x16, 0x1A, 0xBA );
    drvDB_WriteReg8Bit ( 0x16, 0x1B, 0xAB );

    //proto.MstarWriteReg(F1.loopDevice, 0x1618, 0x80);
    drvDB_WriteReg8Bit ( 0x16, 0x18, 0x80 );

    if ( emem_type == EMEM_ALL )
    {
        drvDB_WriteReg8Bit ( 0x16, 0x08, 0x10 ); //mark
    }

    drvDB_WriteReg8Bit ( 0x16, 0x18, 0x40 );
    mdelay ( 10 );

    drvDB_WriteReg8Bit ( 0x16, 0x18, 0x80 );

    // erase trigger
    if ( emem_type == EMEM_MAIN )
    {
        drvDB_WriteReg8Bit ( 0x16, 0x0E, 0x04 ); //erase main
    }
    else
    {
        drvDB_WriteReg8Bit ( 0x16, 0x0E, 0x08 ); //erase all block
    }

    return ( 1 );
}

static int drvTP_read_emem_dbbus_c33 ( EMEM_TYPE_t emem_type, u16 addr, size_t size, u8 *p, size_t set_pce_high )
{
    u32 i;

    // Set the starting address ( must before enabling burst mode and enter riu mode )
    drvDB_WriteReg ( 0x16, 0x00, addr );

    // Enable the burst mode ( must before enter riu mode )
    drvDB_WriteReg ( 0x16, 0x0C, drvDB_ReadReg ( 0x16, 0x0C ) | 0x0001 );

    // Set the RIU password
    drvDB_WriteReg ( 0x16, 0x1A, 0xABBA );

    // Enable the information block if pifren is HIGH
    if ( emem_type == EMEM_INFO )
    {
        // Clear the PCE
        drvDB_WriteReg ( 0x16, 0x18, drvDB_ReadReg ( 0x16, 0x18 ) | 0x0080 );
        mdelay ( 10 );

        // Set the PIFREN to be HIGH
        drvDB_WriteReg ( 0x16, 0x08, 0x0010 );
    }

    // Set the PCE to be HIGH
    drvDB_WriteReg ( 0x16, 0x18, drvDB_ReadReg ( 0x16, 0x18 ) | 0x0040 );
    mdelay ( 10 );

    // Wait pce becomes 1 ( read data ready )
    while ( ( drvDB_ReadReg ( 0x16, 0x10 ) & 0x0004 ) != 0x0004 );

    for ( i = 0; i < size; i += 4 )
    {
        // Fire the FASTREAD command
        drvDB_WriteReg ( 0x16, 0x0E, drvDB_ReadReg ( 0x16, 0x0E ) | 0x0001 );

        // Wait the operation is done
        while ( ( drvDB_ReadReg ( 0x16, 0x10 ) & 0x0001 ) != 0x0001 );

        p[i + 0] = drvDB_ReadReg ( 0x16, 0x04 ) & 0xFF;
        p[i + 1] = ( drvDB_ReadReg ( 0x16, 0x04 ) >> 8 ) & 0xFF;
        p[i + 2] = drvDB_ReadReg ( 0x16, 0x06 ) & 0xFF;
        p[i + 3] = ( drvDB_ReadReg ( 0x16, 0x06 ) >> 8 ) & 0xFF;
    }

    // Disable the burst mode
    drvDB_WriteReg ( 0x16, 0x0C, drvDB_ReadReg ( 0x16, 0x0C ) & ( ~0x0001 ) );

    // Clear the starting address
    drvDB_WriteReg ( 0x16, 0x00, 0x0000 );

    //Always return to main block
    if ( emem_type == EMEM_INFO )
    {
        // Clear the PCE before change block
        drvDB_WriteReg ( 0x16, 0x18, drvDB_ReadReg ( 0x16, 0x18 ) | 0x0080 );
        mdelay ( 10 );
        // Set the PIFREN to be LOW
        drvDB_WriteReg ( 0x16, 0x08, drvDB_ReadReg ( 0x16, 0x08 ) & ( ~0x0010 ) );

        drvDB_WriteReg ( 0x16, 0x18, drvDB_ReadReg ( 0x16, 0x18 ) | 0x0040 );
        while ( ( drvDB_ReadReg ( 0x16, 0x10 ) & 0x0004 ) != 0x0004 );
    }

    // Clear the RIU password
    drvDB_WriteReg ( 0x16, 0x1A, 0x0000 );

    if ( set_pce_high )
    {
        // Set the PCE to be HIGH before jumping back to e-flash codes
        drvDB_WriteReg ( 0x16, 0x18, drvDB_ReadReg ( 0x16, 0x18 ) | 0x0040 );
        while ( ( drvDB_ReadReg ( 0x16, 0x10 ) & 0x0004 ) != 0x0004 );
    }

    return ( 1 );
}


static int drvTP_read_info_dwiic_c33 ( void )
{
    u8  dwiic_tx_data[5];
    u8  dwiic_rx_data[4];
    u16 reg_data=0;
    mdelay ( 300 );

    // Stop Watchdog
    drvDB_WriteReg8Bit ( 0x3C, 0x60, 0x55 );
    drvDB_WriteReg8Bit ( 0x3C, 0x61, 0xAA );

    drvDB_WriteReg ( 0x3C, 0xE4, 0xA4AB );

	drvDB_WriteReg ( 0x1E, 0x04, 0x7d60 );

    // TP SW reset
    drvDB_WriteReg ( 0x1E, 0x04, 0x829F );
	mdelay ( 1 );
    dwiic_tx_data[0] = 0x10;
    dwiic_tx_data[1] = 0x0F;
    dwiic_tx_data[2] = 0xE6;
    dwiic_tx_data[3] = 0x00;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dwiic_tx_data, 4 );
    mdelay ( 100 );
    do{
        reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
    }
    while ( reg_data != 0x5B58 );
    dwiic_tx_data[0] = 0x72;
    dwiic_tx_data[1] = 0x80;
    dwiic_tx_data[2] = 0x00;
    dwiic_tx_data[3] = 0x04;
    dwiic_tx_data[4] = 0x00;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX_TP , dwiic_tx_data, 5 );
    mdelay ( 50 );

    // recive info data
    //HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX_TP, &g_dwiic_info_data[0], 1024 );
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX_TP, &g_dwiic_info_data[0], 8 );
    return ( 1 );
}

static int drvTP_info_updata_C33 ( u16 start_index, u8 *data, u16 size )
{
    // size != 0, start_index+size !> 1024
    u16 i;
    for ( i = 0; i < size; i++ )
    {
        g_dwiic_info_data[start_index] = * ( data + i );
        start_index++;
    }
    return ( 1 );
}

static ssize_t firmware_update_c33 (EMEM_TYPE_t emem_type )
{
    u8  dbbus_tx_data[4];
    u8  dbbus_rx_data[2] = {0};
    u8  life_counter[2];
    u32 i, j;
    u32 crc_main, crc_main_tp;
    u32 crc_info, crc_info_tp;

    int update_pass = 1;
    u16 reg_data = 0;

    crc_main = 0xffffffff;
    crc_info = 0xffffffff;
    drvTP_read_info_dwiic_c33();

    if ( 0/*g_dwiic_info_data[0] == 'M' && g_dwiic_info_data[1] == 'S' && g_dwiic_info_data[2] == 'T' && g_dwiic_info_data[3] == 'A' && g_dwiic_info_data[4] == 'R' && g_dwiic_info_data[5] == 'T' && g_dwiic_info_data[6] == 'P' && g_dwiic_info_data[7] == 'C' */)
    {
        // updata FW Version
        //drvTP_info_updata_C33 ( 8, &temp[32][8], 5 );

		g_dwiic_info_data[8]=temp[32][8];
		g_dwiic_info_data[9]=temp[32][9];
		g_dwiic_info_data[10]=temp[32][10];
		g_dwiic_info_data[11]=temp[32][11];
        // updata life counter
        life_counter[1] = (( ( (g_dwiic_info_data[13] << 8 ) | g_dwiic_info_data[12]) + 1 ) >> 8 ) & 0xFF;
        life_counter[0] = ( ( (g_dwiic_info_data[13] << 8 ) | g_dwiic_info_data[12]) + 1 ) & 0xFF;
		g_dwiic_info_data[12]=life_counter[0];
		g_dwiic_info_data[13]=life_counter[1];
        //drvTP_info_updata_C33 ( 10, &life_counter[0], 3 );
        drvDB_WriteReg ( 0x3C, 0xE4, 0x78C5 );
		drvDB_WriteReg ( 0x1E, 0x04, 0x7d60 );
        // TP SW reset
        drvDB_WriteReg ( 0x1E, 0x04, 0x829F );

        mdelay ( 50 );
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );

        }
        while ( reg_data != 0x2F43 );
        // transmit lk info data
        HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX_TP , &g_dwiic_info_data[0], 1024 );
        //polling 0x3CE4 is 0xD0BC
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
        }
        while ( reg_data != 0xD0BC );
    }

    //erase main
    drvTP_erase_emem_c33 ( EMEM_MAIN );
    mdelay ( 1000 );

    //ResetSlave();
    _HalTscrHWReset();

    //drvDB_EnterDBBUS();
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay ( 300 );

    /////////////////////////
    // Program
    /////////////////////////

    //polling 0x3CE4 is 0x1C70
    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
        }
        while ( reg_data != 0x1C70 );
    }

    switch ( emem_type )
    {
        case EMEM_ALL:
            drvDB_WriteReg ( 0x3C, 0xE4, 0xE38F );  // for all-blocks
            break;
        case EMEM_MAIN:
            drvDB_WriteReg ( 0x3C, 0xE4, 0x7731 );  // for main block
            break;
        case EMEM_INFO:
            drvDB_WriteReg ( 0x3C, 0xE4, 0x7731 );  // for info block

            drvDB_WriteReg8Bit ( 0x0F, 0xE6, 0x01 );

            drvDB_WriteReg8Bit ( 0x3C, 0xE4, 0xC5 ); //
            drvDB_WriteReg8Bit ( 0x3C, 0xE5, 0x78 ); //

            drvDB_WriteReg8Bit ( 0x1E, 0x04, 0x9F );
            drvDB_WriteReg8Bit ( 0x1E, 0x05, 0x82 );

            drvDB_WriteReg8Bit ( 0x0F, 0xE6, 0x00 );
            mdelay ( 100 );
            break;
    }
    // polling 0x3CE4 is 0x2F43
    do
    {
        reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
    }
    while ( reg_data != 0x2F43 );
    // calculate CRC 32
    Init_CRC32_Table ( &crc_tab[0] );

    for ( i = 0; i < 33; i++ ) // total  33 KB : 2 byte per R/W
    {
        if ( emem_type == EMEM_INFO )
			i = 32;

        if ( i < 32 )   //emem_main
        {
            if ( i == 31 )
            {
                temp[i][1014] = 0x5A; //Fmr_Loader[1014]=0x5A;
                temp[i][1015] = 0xA5; //Fmr_Loader[1015]=0xA5;

                for ( j = 0; j < 1016; j++ )
                {
                    //crc_main=Get_CRC(Fmr_Loader[j],crc_main,&crc_tab[0]);
                    crc_main = Get_CRC ( temp[i][j], crc_main, &crc_tab[0] );
                }
            }
            else
            {
                for ( j = 0; j < 1024; j++ )
                {
                    //crc_main=Get_CRC(Fmr_Loader[j],crc_main,&crc_tab[0]);
                    crc_main = Get_CRC ( temp[i][j], crc_main, &crc_tab[0] );
                }
            }
        }
        else  //emem_info
        {
            for ( j = 0; j < 1024; j++ )
            {
                //crc_info=Get_CRC(Fmr_Loader[j],crc_info,&crc_tab[0]);
                crc_info = Get_CRC ( g_dwiic_info_data[j], crc_info, &crc_tab[0] );
            }
            if ( emem_type == EMEM_MAIN ) break;
        }
        //drvDWIIC_MasterTransmit( DWIIC_MODE_DWIIC_ID, 1024, Fmr_Loader );
        #if 1
        {
            u32 n = 0;
            for(n=0;n<UPDATE_TIMES;n++)
            {
                HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX_TP, temp[i]+n*N_BYTE_PER_TIME, N_BYTE_PER_TIME );
            }
        }
        #else
        HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX_TP, temp[i], 1024 );
        #endif
        // polling 0x3CE4 is 0xD0BC
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
        }
        while ( reg_data != 0xD0BC );
        drvDB_WriteReg ( 0x3C, 0xE4, 0x2F43 );
    }
    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        // write file done and check crc
        drvDB_WriteReg ( 0x3C, 0xE4, 0x1380 );
    }
    mdelay ( 10 ); //MCR_CLBK_DEBUG_DELAY ( 10, MCU_LOOP_DELAY_COUNT_MS );

    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        // polling 0x3CE4 is 0x9432
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
        }while ( reg_data != 0x9432 );
    }

    crc_main = crc_main ^ 0xffffffff;
    crc_info = crc_info ^ 0xffffffff;

    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        // CRC Main from TP
        crc_main_tp = drvDB_ReadReg ( 0x3C, 0x80 );
        crc_main_tp = ( crc_main_tp << 16 ) | drvDB_ReadReg ( 0x3C, 0x82 );

        // CRC Info from TP
        crc_info_tp = drvDB_ReadReg ( 0x3C, 0xA0 );
        crc_info_tp = ( crc_info_tp << 16 ) | drvDB_ReadReg ( 0x3C, 0xA2 );
    }
    TP_DEBUG ( "crc_main=0x%x, crc_info=0x%x, crc_main_tp=0x%x, crc_info_tp=0x%x\n",
               crc_main, crc_info, crc_main_tp, crc_info_tp );

    //drvDB_ExitDBBUS();
    update_pass = 1;
	if ( emem_type == EMEM_ALL ) 
    {
        if ( crc_main_tp != crc_main )
            update_pass = 0;

        if ( crc_info_tp != crc_info )
            update_pass = 0;
    }
	if ( emem_type == EMEM_MAIN ) 
    {
        if ( crc_main_tp != crc_main )
            update_pass = 0;
    }
    if ( !update_pass )
    {
        TP_DEBUG_ERR ( "update_C33 FAILED\n" );
	_HalTscrHWReset();
        FwDataCnt = 0;
    	HalEnableIrq();
   //     return size;
    }
    TP_DEBUG_ERR ( "update_C33 OK\n" );
	_HalTscrHWReset();
    FwDataCnt = 0;
    HalEnableIrq();
//    return size;
}

#define _FW_UPDATE_C3_
static int msg2133x_do_upgrade()
{
	u8 i;
	u8 dbbus_tx_data[4];
	unsigned char dbbus_rx_data[2] = {0};
	update_switch=1;
	HalDisableIrq();

    _HalTscrHWReset();

    // Erase TP Flash first
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay ( 300 );

    // Disable the Watchdog
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x3C;
    dbbus_tx_data[2] = 0x60;
    dbbus_tx_data[3] = 0x55;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x3C;
    dbbus_tx_data[2] = 0x61;
    dbbus_tx_data[3] = 0xAA;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );
    // Stop MCU
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x0F;
    dbbus_tx_data[2] = 0xE6;
    dbbus_tx_data[3] = 0x01;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );
#ifdef _FW_UPDATE_C3_
	
    /////////////////////////
    // Difference between C2 and C3
    /////////////////////////
	// c2:2133 c32:2133a(2) c33:2138
    //check id
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0xCC;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
    TP_DEBUG_ERR ( "111dbbus_rx version[0]=0x%x", dbbus_rx_data[0] );
    if ( dbbus_rx_data[0] == 2 )
    {
        // check version
        dbbus_tx_data[0] = 0x10;
        dbbus_tx_data[1] = 0x3C;
        dbbus_tx_data[2] = 0xEA;
        HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
        HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
        TP_DEBUG_ERR ( "dbbus_rx version[0]=0x%x", dbbus_rx_data[0] );

        if ( dbbus_rx_data[0] == 3 ){
            firmware_update_c33 (EMEM_MAIN );
		}
        else{

             firmware_update_c32 (EMEM_ALL );
        }
    }
    else
    {
         firmware_update_c2 ();
    }
    
#else
	
    // set FRO to 50M
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x11;
    dbbus_tx_data[2] = 0xE2;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
    dbbus_rx_data[0] = 0;
    dbbus_rx_data[1] = 0;
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
    TP_DEBUG ( "dbbus_rx_data[0]=0x%x", dbbus_rx_data[0] );
    dbbus_tx_data[3] = dbbus_rx_data[0] & 0xF7;  //Clear Bit 3
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // set MCU clock,SPI clock =FRO
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x22;
    dbbus_tx_data[3] = 0x00;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x23;
    dbbus_tx_data[3] = 0x00;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // Enable slave's ISP ECO mode
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x08;
    dbbus_tx_data[2] = 0x0c;
    dbbus_tx_data[3] = 0x08;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // Enable SPI Pad
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x02;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
    TP_DEBUG ( "dbbus_rx_data[0]=0x%x", dbbus_rx_data[0] );
    dbbus_tx_data[3] = ( dbbus_rx_data[0] | 0x20 ); //Set Bit 5
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // WP overwrite
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x0E;
    dbbus_tx_data[3] = 0x02;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // set pin high
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x10;
    dbbus_tx_data[3] = 0x08;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    dbbusDWIICIICNotUseBus();
    dbbusDWIICNotStopMCU();
    dbbusDWIICExitSerialDebugMode();

    drvISP_EntryIspMode();
    drvISP_ChipErase();
    _HalTscrHWReset();
    mdelay ( 300 );

    // 2.Program and Verify
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();

    // Disable the Watchdog
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x3C;
    dbbus_tx_data[2] = 0x60;
    dbbus_tx_data[3] = 0x55;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x3C;
    dbbus_tx_data[2] = 0x61;
    dbbus_tx_data[3] = 0xAA;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // Stop MCU
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x0F;
    dbbus_tx_data[2] = 0xE6;
    dbbus_tx_data[3] = 0x01;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // set FRO to 50M
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x11;
    dbbus_tx_data[2] = 0xE2;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
    dbbus_rx_data[0] = 0;
    dbbus_rx_data[1] = 0;
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
    TP_DEBUG ( "dbbus_rx_data[0]=0x%x", dbbus_rx_data[0] );
    dbbus_tx_data[3] = dbbus_rx_data[0] & 0xF7;  //Clear Bit 3
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // set MCU clock,SPI clock =FRO
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x22;
    dbbus_tx_data[3] = 0x00;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x23;
    dbbus_tx_data[3] = 0x00;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // Enable slave's ISP ECO mode
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x08;
    dbbus_tx_data[2] = 0x0c;
    dbbus_tx_data[3] = 0x08;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // Enable SPI Pad
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x02;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
    TP_DEBUG ( "dbbus_rx_data[0]=0x%x", dbbus_rx_data[0] );
    dbbus_tx_data[3] = ( dbbus_rx_data[0] | 0x20 ); //Set Bit 5
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // WP overwrite
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x0E;
    dbbus_tx_data[3] = 0x02;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // set pin high
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x10;
    dbbus_tx_data[3] = 0x08;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    dbbusDWIICIICNotUseBus();
    dbbusDWIICNotStopMCU();
    dbbusDWIICExitSerialDebugMode();

    ///////////////////////////////////////
    // Start to load firmware
    ///////////////////////////////////////
    drvISP_EntryIspMode();

    for ( i = 0; i < 94; i++ ) // total  94 KB : 1 byte per R/W
    {
        drvISP_Program ( i, temp[i] ); // program to slave's flash
        drvISP_Verify ( i, temp[i] ); //verify data
    }
    TP_DEBUG ( "update OK\n" );
    drvISP_ExitIspMode();
    FwDataCnt = 0;
   
#endif
	 update_switch=0;
	msg2133_ts_get_fw_version();
	if(panel_version>0)
		return 1;
	else
		return 0;
}

static ssize_t firmware_update_store ( struct device *dev,
                                       struct device_attribute *attr, const char *buf, size_t size )
{
    		msg2133x_do_upgrade();
  		return size;
}

static DEVICE_ATTR(update, CTP_AUTHORITY, firmware_update_show, firmware_update_store);
/*test=================*/
/*Add by Tracy.Lin for update touch panel firmware and get fw version*/

static ssize_t firmware_version_show(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
    TP_DEBUG("*** firmware_version_show fw_version = %s***\n", fw_version);
    return sprintf(buf, "%s\n", fw_version);
}

static ssize_t firmware_version_store(struct device *dev,
                                      struct device_attribute *attr, const char *buf, size_t size)
{
    unsigned char dbbus_tx_data[3];
    unsigned char dbbus_rx_data[4] ;
    unsigned short major=0, minor=0;
/*
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();

*/
    fw_version = kzalloc(sizeof(char), GFP_KERNEL);

    //Get_Chip_Version();
    dbbus_tx_data[0] = 0x53;
    dbbus_tx_data[1] = 0x00;
    if(CTP_IC_TYPE = CTP_ID_MSG21XXA)//MSG2133A
        dbbus_tx_data[2] = 0x2A;
    else//MSG2133
        dbbus_tx_data[2] = 0x74;
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX_TP, &dbbus_tx_data[0], 3);
    HalTscrCReadI2CSeq(FW_ADDR_MSG21XX_TP, &dbbus_rx_data[0], 4);

    major = (dbbus_rx_data[1]<<8)+dbbus_rx_data[0];
    minor = (dbbus_rx_data[3]<<8)+dbbus_rx_data[2];

    TP_DEBUG_ERR("***major = %d ***\n", major);
    TP_DEBUG_ERR("***minor = %d ***\n", minor);
    sprintf(fw_version,"%03d%03d", major, minor);
    //TP_DEBUG(printk("***fw_version = %s ***\n", fw_version);)

    return size;
}
static DEVICE_ATTR(version, CTP_AUTHORITY, firmware_version_show, firmware_version_store);

static ssize_t firmware_data_show(struct device *dev,
                                  struct device_attribute *attr, char *buf)
{
    return FwDataCnt;
}

static ssize_t firmware_data_store(struct device *dev,
                                   struct device_attribute *attr, const char *buf, size_t size)
{
	int i;
	TP_DEBUG("***FwDataCnt = %d ***\n", FwDataCnt);
	memcpy(temp[FwDataCnt], buf, 1024);
	FwDataCnt++;
	return size;
}
static DEVICE_ATTR(data, CTP_AUTHORITY, firmware_data_show, firmware_data_store);
#ifdef ENABLE_AUTO_UPDATA
#include "msg21xxFW.h"
#define	CTP_ID_MSG21XX		0
#define	CTP_ID_MSG21XXA		1

unsigned char getchipType(void)
{
    u8 dbbus_tx_data[4];
    unsigned char dbbus_rx_data[2] = {0};

	_HalTscrHWReset();
    mdelay ( 100 );

	dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay ( 100 );

    // Disable the Watchdog
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x3C;
    dbbus_tx_data[2] = 0x60;
    dbbus_tx_data[3] = 0x55;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x3C;
    dbbus_tx_data[2] = 0x61;
    dbbus_tx_data[3] = 0xAA;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );
    // Stop MCU
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x0F;
    dbbus_tx_data[2] = 0xE6;
    dbbus_tx_data[3] = 0x01;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );
    /////////////////////////
    // Difference between C2 and C3
    /////////////////////////
	// c2:2133 c32:2133a(2) c33:2138
    //check id
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0xCC;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
    if ( dbbus_rx_data[0] == 2 )
    {
    	return CTP_ID_MSG21XXA;
    }
    else
    {
    	return CTP_ID_MSG21XX;
    }

}

unsigned int getFWPrivateVersion(void)
{
    unsigned char dbbus_tx_data[3];
    unsigned char dbbus_rx_data[4] ;
    unsigned short major=0, minor=0;

	_HalTscrHWReset();
    mdelay ( 200 );

    dbbus_tx_data[0] = 0x53;
    dbbus_tx_data[1] = 0x00;
    if(CTP_IC_TYPE = CTP_ID_MSG21XXA)//MSG2133A
        dbbus_tx_data[2] = 0x2A;
    else//MSG2133
        dbbus_tx_data[2] = 0x74;
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX_TP, &dbbus_tx_data[0], 3);
    HalTscrCReadI2CSeq(FW_ADDR_MSG21XX_TP, &dbbus_rx_data[0], 4);

    major = (dbbus_rx_data[1]<<8)+dbbus_rx_data[0];
    minor = (dbbus_rx_data[3]<<8)+dbbus_rx_data[2];

    TP_DEBUG_ERR("***FW Version major = %d ***\n", major);
    TP_DEBUG_ERR("***FW Version minor = %d ***\n", minor);

    _HalTscrHWReset();
    mdelay ( 100 );

    return ((major<<16)|(minor));
}
static int fwAutoUpdate(void *unused)
{
    firmware_update_store(NULL, NULL, NULL, 0);
}
#endif

#endif

#ifdef CONFIG_TOUCHSCREEN_MSG2133_SUPPORT_ISP
struct tinno_ts_data {
	struct wake_lock wake_lock;
	atomic_t isp_opened;
	uint8_t *isp_pBuffer;
	struct i2c_client *client;
};
static struct tinno_ts_data *g_pts = NULL;

static struct mutex tp_mutex;

#define MSG2133_TOUCH_IO_MAGIC ('G')
#define MSG2133_IOCTL_UPDATE_FIRMWARE		_IO(MSG2133_TOUCH_IO_MAGIC , 0x00)
#define MSG2133_IOCTL_GET_VENDORID		_IOR(MSG2133_TOUCH_IO_MAGIC, 0x01, int)
#define MSG2133_IOCTL_RESET			    _IO(MSG2133_TOUCH_IO_MAGIC, 0x02)
#define MSG2133_IOC_MAXNR				(0x03)

static u8 tpd_down_state=0;
static int down_x=0;
static int down_y=0;
static void msg2133_fts_isp_register(struct i2c_client *client);
#endif

#ifdef CONFIG_TOUCHSCREEN_MSG2133_SUPPORT_ISP

static inline int _lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1) {
		return 0;
	} else {
		atomic_dec(excl);
		return -1;
	}
}

static inline void _unlock(atomic_t *excl)
{
	atomic_dec(excl);
}

static ssize_t msg2133_fts_isp_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
	//ret = copy_to_user(buf,&acc, sizeof(acc));
	TPD_DEBUG("");
	return -EIO;
}

static ssize_t msg2133_fts_isp_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
	char * tmp = NULL;
	tmp = kmalloc(count, GFP_KERNEL);
	if (tmp == NULL)
	    return -ENOMEM;

	if (copy_from_user(tmp, buf, count)) {
	    return -EFAULT;
	}

	int i;
	TP_DEBUG("***FwDataCnt = %d ***\n", FwDataCnt);

	memcpy(temp[FwDataCnt],tmp, 1024);

	FwDataCnt++;
	return count;
}

static int msg2133_fts_isp_open(struct inode *inode, struct file *file)
{
	struct tinno_ts_data *ts = file->private_data;

	TPD_DEBUG("try to open isp.\n");

	if (_lock(&g_pts->isp_opened)){
		TPD_DEBUG("isp is already opened.");
		return -EBUSY;
	}

	mutex_lock(&tp_mutex);

	file->private_data = g_pts;

        mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	wake_lock(&g_pts->wake_lock);

	TPD_DEBUG("isp open success.");
	return 0;
}

static int msg2133_fts_isp_close(struct inode *inode, struct file *file)
{
	struct tinno_ts_data *ts = file->private_data;

	TPD_DEBUG("try to close isp.");

 	mutex_unlock(&tp_mutex);

   	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

	file->private_data = NULL;

	_unlock ( &ts->isp_opened );

	wake_unlock(&ts->wake_lock);

	TPD_DEBUG("close isp success!");
	return 0;
}

static int msg2133_fts_isp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct tinno_ts_data *ts = file->private_data;
	int flag;
	int rc = 0;
	uint8_t  command = 0xFC;
	uint8_t  checksum,calib_buf;

	if ( !atomic_read( &g_pts->isp_opened ) ){
		TPD_DEBUG("no opened isp.\n");
		return -ENODEV;
	}

	/* check cmd */
	if(_IOC_TYPE(cmd) != MSG2133_TOUCH_IO_MAGIC)
	{
		TPD_DEBUG("cmd magic type error\n");
		return -EINVAL;
	}
	if(_IOC_NR(cmd) > MSG2133_IOC_MAXNR)
	{
		TPD_DEBUG("cmd number error\n");
		return -EINVAL;
	}

	if(_IOC_DIR(cmd) & _IOC_READ)
		rc = !access_ok(VERIFY_WRITE,(void __user*)arg, _IOC_SIZE(cmd));
	else if(_IOC_DIR(cmd) & _IOC_WRITE)
		rc = !access_ok(VERIFY_READ, (void __user*)arg, _IOC_SIZE(cmd));
	if(rc)
	{
		TPD_DEBUG("cmd access_ok error\n");
		return -EINVAL;
	}

	switch (cmd) {
	case MSG2133_IOCTL_UPDATE_FIRMWARE:
		printk("Magnum update firmware .....");
		 if(msg2133x_do_upgrade())
		 	printk("OK OK OK update firmware .....");
		 else
		 	printk("Fail Fail Fail update firmware .....");
		break;
	case MSG2133_IOCTL_RESET:
		{
		    	tpd_reset();
		        break;}
	case MSG2133_IOCTL_GET_VENDORID:
            {

                TPD_DEBUG("Magnum vendor_id=0x%x\n", vendor_version);

                if(copy_to_user(argp,&vendor_version,sizeof(char))!=0)
                {
                    TPD_DEBUG(KERN_INFO "copy_to_user error\n");
                    rc = -EFAULT;
                }
                break;
            }
	default:
		TPD_DEBUG("invalid command %d\n", _IOC_NR(cmd));
		rc = -EINVAL;
		break;
	}

	return rc;
}


static const struct file_operations msg2133_fts_isp_fops = {
	.owner = THIS_MODULE,
	.read = msg2133_fts_isp_read,
	.write = msg2133_fts_isp_write,
	.open = msg2133_fts_isp_open,
	.release = msg2133_fts_isp_close,
	.unlocked_ioctl = msg2133_fts_isp_ioctl,
};

static struct miscdevice msg2133_fts_isp_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msg2133_fts_isp",
	.fops = &msg2133_fts_isp_fops,
};

static void msg2133_fts_isp_register(struct i2c_client *client)
{
	struct tinno_ts_data *ts;
	int err,ret;

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		ret = -ENOMEM;
		goto err_detect_failed;
	}

	wake_lock_init(&ts->wake_lock, WAKE_LOCK_SUSPEND, "fts_tp_isp");
	ts->client = i2c_client;
	err = misc_register(&msg2133_fts_isp_device);
	if (err) {
		TPD_DEBUG(KERN_ERR "fts_isp_device device register failed\n");
		goto exit_misc_device_register_failed;
	}else
		g_pts = ts;
	return;

err_detect_failed:
	kfree(ts);
	return;

exit_misc_device_register_failed:
	wake_lock_destroy(&ts->wake_lock);
}

#endif

#ifdef CTP_SIMULATE_PS

static u8 msg2133_get_ps(void)
{
      return  ps_data;
}
static void ctp_convert(char mode,bool force)
{
    U8 ps_store_data[4];
    TPD_DEBUG("current_CTP_PS mode=%d\n", mode);
	if(force==false)
	{
		if((!tpd_load_status_ok)||(mode==ps_mode)){
			return;
		}
	}
	if(DISABLE_CTP_PS == mode)
	{
		ps_mode = 0;
		TPD_DEBUG("DISABLE_CTP_PS mode=%d\n", mode);
		ps_store_data[0] = 0x52;
		ps_store_data[1] = 0x00;
		ps_store_data[2] = 0x62;
		ps_store_data[3] = 0xa1;
		HalTscrCDevWriteI2CSeq(TOUCH_ADDR_MSG21XX, &ps_store_data[0], 4);
	}
	else if(ENABLE_CTP_PS == mode)
	{
		TPD_DEBUG("ENABLE_CTP_PS mode=%d\n", mode);
		ps_mode = 1;
		ps_store_data[0] = 0x52;
		ps_store_data[1] = 0x00;
		ps_store_data[2] = 0x62;
		ps_store_data[3] = 0xa0;
		HalTscrCDevWriteI2CSeq(TOUCH_ADDR_MSG21XX, &ps_store_data[0], 4);
	}
}

void tpd_resume_process_in_ps_mode(struct i2c_client *client)
{
	TPD_DEBUG("tpd_resume_process_in_ps_mode\n");
	if(power_pressed ){
		U8 ps_store_data[4];
		TPD_DEBUG("tpd_resume_process_in_ps_mode power_pressed=%d\n", power_pressed);
		ps_data_pre=ps_data=1;
	    	ctp_ps_event_handler();  //do not work...
		powerkey_call_reset();
		TPD_DEBUG("ENABLE_CTP_PS1 \n");
	}

}
#endif


//get tp_firmware
static int msg2133_get_fw_version_stored()
{
//edit by Magnum 2012-3-1
//    panel_version = tinno_ts_get_fw_version();
    return panel_version;
}

static int msg2133_get_vendor_version_stored()
{
     return vendor_version;
}

static int msg2133_ts_get_fw_version()
{
	TPD_DEBUG("magnum msg2133_ts_get_fw_version \n");
	unsigned char dbbus_tx_data[3];
	unsigned char dbbus_rx_data[4] ;
	unsigned short major = 0, minor = 0;
	int ret;
	fw_version = kzalloc(sizeof(char), GFP_KERNEL);
	dbbus_tx_data[0] = 0x53;
	dbbus_tx_data[1] = 0x00;
	if(CTP_IC_TYPE = CTP_ID_MSG21XXA)//MSG2133A
	    dbbus_tx_data[2] = 0x2A;
	else//MSG2133
	    dbbus_tx_data[2] = 0x74;

	ret = i2c_master_send(i2c_client,&dbbus_tx_data[0], 3);
	if(ret <= 0)
	{
	    TPD_DEBUG("Magnum i2c_write_interface error line = %d, ret = %d\n", __LINE__, ret);
		return -1;
	}

	ret = i2c_master_recv(i2c_client,&dbbus_rx_data[0], 4);
	 if(ret <= 0)
	{
	    TPD_DEBUG("Magnum i2c_read_interface error line = %d, ret = %d\n", __LINE__, ret);
		return -1;
	}
	major = (dbbus_rx_data[1] << 8) + dbbus_rx_data[0];
	minor = (dbbus_rx_data[3] << 8) + dbbus_rx_data[2];
	TPD_DEBUG("***major = %x ***\n", major);
	TPD_DEBUG("***minor = %x ***\n", minor);
	vendor_version = major;   //major is vendor_version ,eg: YD ,BYD ,Truly
	panel_version = minor;    //minor is panel_version ,eg BM-24, Truly-16.
	TPD_DEBUG("*** vendor_version = %x ***\n", vendor_version);
	TPD_DEBUG("*** panel_version = %x ***\n", panel_version);
	sprintf(fw_version, "%02d%02d", major, minor);
	TPD_DEBUG("***fw_version = %s ***\n", fw_version);
	TPD_DEBUG("tpd_local_init boot mode = %d \n", boot_mode);

	return 1;
}
//end get tp_firmware


unsigned char tpd_check_sum(unsigned char *pval)
{
	int i, sum = 0;

	for(i = 0; i < 7; i++)
	{
		sum += pval[i];
	}

	return (unsigned char)((-sum) & 0xFF);
}


static bool msg2133_i2c_read(char *pbt_buf, int dw_lenth)
{
	int ret;
	i2c_client->timing = 100;
	i2c_client->addr|=I2C_ENEXT_FLAG;
     // TPD_DEBUG("Magnum msg_i2c read  ..............\n");
	ret = i2c_master_recv(i2c_client, pbt_buf, dw_lenth);
	if(ret <= 0)
	{
        TPD_DEBUG("Magnum msg_i2c read error ..............\n");
		return false;
	}
      //  TPD_DEBUG("Magnum msg_i2c read OK ..............\n");
	return true;
}

BYTE touch_flag=0x00;
static int tpd_touchinfo(struct touch_info *cinfo)
{
	SHORT_TOUCH_STATE ShortTouchState;
	BYTE reg_val[8] = {0};
	unsigned int  temp = 0;

	if(update_switch)
		return false;
	if(!msg2133_i2c_read(reg_val, 8))
		return false;
       int ii = 0;
       /*while(++ii <8){
            TPD_DEBUG("Magnum original reg_val-%d ==%x \n",ii,reg_val[ii]);
       }*/

	ShortTouchState.pos_x = ((reg_val[1] & 0xF0) << 4) | reg_val[2];
	ShortTouchState.pos_y = ((reg_val[1] & 0x0F) << 8) | reg_val[3];
	ShortTouchState.dst_x = ((reg_val[4] & 0xF0) << 4) | reg_val[5];
	ShortTouchState.dst_y = ((reg_val[4] & 0x0F) << 8) | reg_val[6];

  //  TPD_DEBUG("ShortTouchState.pos_x  === %d  \n ShortTouchState.pos_y === %d \n ShortTouchState.dst_x=== %d \n  ShortTouchState.dst_y=== %d \n ",ShortTouchState.pos_x,ShortTouchState.pos_y,ShortTouchState.dst_x,ShortTouchState.dst_y);

	if((ShortTouchState.dst_x) & 0x0800)
	{
		ShortTouchState.dst_x |= 0xF000;
	}

	if((ShortTouchState.dst_y) & 0x0800)
	{
		ShortTouchState.dst_y |= 0xF000;
	}

	ShortTouchState.pos_x2 = ShortTouchState.pos_x + ShortTouchState.dst_x;
	ShortTouchState.pos_y2 = ShortTouchState.pos_y + ShortTouchState.dst_y;
	temp = tpd_check_sum(reg_val);
	if(temp == reg_val[7])
	{
		if(reg_val[0] == 0x52) //CTP  ID
		{
			if(reg_val[1] == 0xFF&& reg_val[4] == 0xFF)
			{
                                  //default ==== reg_val[6]
        #ifdef CTP_SIMULATE_PS
				if(reg_val[5] == 0x80) // close to
				{
				    TPD_DEBUG("slaver report i am close...\n");
					ps_data = 0;
				}
				else if(reg_val[5] == 0x40) // leave
				{
				    TPD_DEBUG("slaver report i am far away...\n");
					ps_data = 1;
				}
				// when ic reset himself,it will send c0 to host,and when host in call ,convert mode.
				else if(reg_val[5] == 0xc0)
				{
                    TPD_DEBUG("report call state %d to msg2133...\n",ps_mode);
					ctp_convert(ps_mode,true);
				}
				else
		#endif
				if(reg_val[5]==0xFF||reg_val[5]==0x00)
				{
			//		TPD_DEBUG("Magnum Num 000000 ======\n");
					if(touch_flag)
					{
						touch_flag=0;
						point_num = 0;
					}
					else
					{
						return  false;
					}
				}
				else if(reg_val[5] == 0x01)
				{
				//Key 1
               TPD_DEBUG("Magnum Key menu ======\n");
				key_index = 1;
				cinfo->x[0] =TPD_CUST_KEY_X3;
				cinfo->y[0] = TPD_CUST_KEY_Y;
				point_num = 1;
				touch_flag=1;
         //       TPD_DEBUG(" x === %d  y === %d \n",cinfo->x[0],cinfo->y[0]);
				}
				else if(reg_val[5] == 0x02)
				{
                TPD_DEBUG("Magnum Key home ======\n");
				key_index = 2;
				cinfo->x[0] = TPD_CUST_KEY_X2;
				cinfo->y[0] = TPD_CUST_KEY_Y;
				point_num = 1;
				touch_flag=1;
        //        TPD_DEBUG(" x === %d  y === %d \n",cinfo->x[0],cinfo->y[0]);
					//Key 2
				}
				else if(reg_val[5] == 0x04)
				{
				    TPD_DEBUG("Magnum Key return ======\n");
					key_index = 3;
					cinfo->x[0] = TPD_CUST_KEY_X1;//tpd_keys_dim_local[key_index-1][0];
					cinfo->y[0] = TPD_CUST_KEY_Y;//tpd_keys_dim_local[key_index-1][1];
					point_num = 1;
					touch_flag=1;
				}
				#ifdef TPD_CUST_KEY_X4
				else if(reg_val[5] == 0x08)
				{
					key_index = 0;
					cinfo->x[0] = TPD_CUST_KEY_X4;
					cinfo->y[0] = TPD_CUST_KEY_Y;
					point_num = 1;
				}
				#endif
				else
				{
					return  false;
				}
			}
			else if(ShortTouchState.pos_x > 2047 || ShortTouchState.pos_y > 2047)
			{
				return  false;
			}
			else if((ShortTouchState.dst_x == 0) && (ShortTouchState.dst_y == 0))
			{
				cinfo->x[0] = (ShortTouchState.pos_x * TPD_CUST_RES_X) / 2048;
				cinfo->y[0] = (ShortTouchState.pos_y * TPD_CUST_RES_Y) / 2048;
				point_num = 1;
				touch_flag=1;
			}
			else
			{
                TPD_DEBUG("Point NUm ==== 222222 \n");
				if(ShortTouchState.pos_x2 > 2047 || ShortTouchState.pos_y2 > 2047)
					return false;
				cinfo->x[0] = (ShortTouchState.pos_x *TPD_CUST_RES_X) / 2048;
				cinfo->y[0] = (ShortTouchState.pos_y *  TPD_CUST_RES_Y) / 2048;
				cinfo->x[1] = (ShortTouchState.pos_x2 * TPD_CUST_RES_X) / 2048;
				cinfo->y[1] = (ShortTouchState.pos_y2 * TPD_CUST_RES_Y) / 2048;
				point_num = 2;
				touch_flag=1;
			}
		}
		else{
              TPD_DEBUG("Magnum 0x52 err ....\n");
		}
		return true;
	}
	else
	{
	    TPD_DEBUG("Magnum checksum err ....\n");
		return  false;
	}

}

 static int touch_event_handler(void *unused)
 {
    struct touch_info cinfo;
	int touch_state = 3;
	unsigned long time_eclapse;
	struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
	sched_setscheduler(current, SCHED_RR, &param);
	do
	{
	  //edit by Magnum 2012-8-30: if tp go bad in call ,reset the TP
	  #ifdef CTP_SIMULATE_PS
	    TPD_DEBUG(" ps mode == %d && power_pressed == %d \n",ps_mode,power_pressed);
	    if(ps_mode == 1 && power_pressed )//&& ps_data == 0 ) // && !tpd_touchinfo(&cinfo))
	    {
			tpd_resume_process_in_ps_mode(i2c_client);
		}
	  #endif
		mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(waiter, tpd_flag != 0);
		tpd_flag = 0;
		set_current_state(TASK_RUNNING);
		if(tpd_touchinfo(&cinfo))
	          {
				if(point_num == 1)
				{
	    		 //           TPD_DEBUG("Magnum report to host ........\n");
	    				 TPD_DEBUG("MSG_X = %d,MSG_Y = %d\n", cinfo.x[0], cinfo.y[0]);
	               			 if(cinfo.y[0] < TPD_CUST_RES_Y){
						TPD_DEBUG("Magnum point is in Working area ........\n");
						if(cinfo.x[0] == 0) cinfo.x[0] = 1;
						if(cinfo.y[0] == 0) cinfo.y[0] = 1;
	                    			tpd_down(cinfo.x[0], cinfo.y[0], 1);
	                    			input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, 0);
	                		 }
	                		else
			                {
			                   if (FACTORY_BOOT== boot_mode || RECOVERY_BOOT == boot_mode)
			                   {
			                      TPD_DEBUG("Magnum point is FACTORY_BOOT or RECOVERY_BOOT  KEY area ........\n");
			                      tpd_button(cinfo.x[0], cinfo.y[0], 1);

			                   	}
							   else
							   	{
							   	  TPD_DEBUG("Magnum point is NORMAL_BOOT KEY area ........\n");
			                       tpd_down(cinfo.x[0], cinfo.y[0], 1);
								   input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, 0);
			                    }
			               }
					input_sync(tpd->dev);
				}
				else if (point_num == 2){
					// ps_data = 0;
				//	 TPD_DEBUG("ps_data ==== %d........\n",ps_data);
					 TPD_DEBUG("Magnum point Number 22222222........\n");
					 TPD_DEBUG("First point MSG_X = %d,MSG_Y = %d\n", cinfo.x[0], cinfo.y[0]);
					 TPD_DEBUG("Second point MSG_X = %d,MSG_Y = %d\n", cinfo.x[1], cinfo.y[1]);
					if((cinfo.y[0] < TPD_CUST_RES_Y)&&(cinfo.y[1] < TPD_CUST_RES_Y)){
					tpd_down(cinfo.x[0], cinfo.y[0], 1);
					input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, 0);
					tpd_down(cinfo.x[1], cinfo.y[1], 1);
					input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, 1);
					}
		            else if((cinfo.y[0] > TPD_CUST_RES_Y)&&(cinfo.y[1] < TPD_CUST_RES_Y))
		            {
					 	tpd_down(cinfo.x[1], cinfo.y[1], 1);
		                input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, 0);
		            }
		            else if((cinfo.y[0] < TPD_CUST_RES_Y)&&(cinfo.y[1] > TPD_CUST_RES_Y))
		            {
				 	    tpd_down(cinfo.x[0], cinfo.y[0], 1);
		                input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, 0);
		            }
		            input_sync(tpd->dev);
	         	}

	            else if(point_num == 0){
		//		       TPD_DEBUG("Magnum  tp release/........\n");
	                	if(key_index > -1)
						{
		//				TPD_DEBUG("UP  key ====%d",tpd_keys_local[key_index]);
	                  if (NORMAL_BOOT != boot_mode)
	                      tpd_button(cinfo.x[0], cinfo.y[0], 0);
			           	  key_index = -1;
						}
					input_mt_sync(tpd->dev);
					input_sync(tpd->dev);
				}
	}
	else{
        TPD_DEBUG("Magnun get touch info err......\n");
	}

	//edit by Magnum 2012-7-27
#ifdef CTP_SIMULATE_PS
	  if(tpd_load_status_ok&&(ps_data_pre!=ps_data)){
	      	ctp_ps_event_handler();
			ps_data_pre=ps_data;
	  }
#endif

 }while(!kthread_should_stop());

	 return 0;
 }

 static int tpd_i2c_detect (struct i2c_client *client, int kind, struct i2c_board_info *info)
 {
	int error;
	TPD_DEBUG("TPD detect \n");
	strcpy(info->type, TPD_DEVICE);
	return 0;
 }


 void msg2133_tpd_eint_interrupt_handler(void)
 {
	// mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM); //1009 mask eint
	 TPD_DEBUG("TPD interrupt has been triggered \n");
	 tpd_flag = 1;
	 wake_up_interruptible(&waiter);
 }


static int MSG2133_init(void)
{
    unsigned char dbbus_tx_data[3];
    unsigned char dbbus_rx_data[4];
    hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_2800, "TP");
    tpd_reset();
    if(dbbusDWIICEnterSerialDebugMode()<=0)
    {
            return -1;
    }
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay ( 100 );
    /////////////////////////
    // Difference between C2 and C3
    /////////////////////////
	// c2:2133 c32:2133a(2) c33:2138
    //check id
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0xCC;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
    if ( dbbus_rx_data[0] == 2 )
    {
    	CTP_IC_TYPE = CTP_ID_MSG21XXA;
    }
    else
    {
    	CTP_IC_TYPE = CTP_ID_MSG21XX;
    }
   tpd_reset();
   return 0;
}

#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
/* gives back true if only one touch is recognized */
bool is_single_touch(void)
{
	/*int i = 0, cnt = 0;

	for( i= 0; i<MAX_NUM_FINGER; i++ ) {
		if ((!fingerInfo[i].status) ||
				(fingerInfo[i].status == TOUCH_EVENT_RELEASE))
		continue;
		else cnt++;
	}*/
printk("[SWEEP2WAKE]: inside single touch\n");
	if (s2w_st_flag == 1)
	return true;
	else
	return false;
}
#endif

 static int __devinit tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
 {
		int error;
		int retval = TPD_OK;
		u8 host_reg;

		//Disable the real PS by ljs
		mt_set_gpio_mode(GPIO_ALS_EN_PIN, GPIO_MODE_GPIO);
		mt_set_gpio_dir(GPIO_ALS_EN_PIN, GPIO_DIR_OUT);
		mt_set_gpio_pull_enable(GPIO_ALS_EN_PIN, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO_ALS_EN_PIN, GPIO_PULL_UP);
		mt_set_gpio_out(GPIO_ALS_EN_PIN, 1);

		i2c_client = client;

		TPD_DEBUG("Power on\n");

		if(MSG2133_init()!=0)
		{
		    TPD_DEBUG("Msg2133 I2C transfer error, func: %s\n", __func__);
		    hwPowerDown(MT65XX_POWER_LDO_VGP2, "TP");
		    return -1;
		}

		tpd_load_status = 1;

		msg2133_ts_get_fw_version();
		mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
		mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
		mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN,GPIO_PULL_UP);
		mt65xx_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
		mt65xx_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
		mt65xx_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_TOUCH_PANEL_POLARITY, msg2133_tpd_eint_interrupt_handler, 1);
		msleep(300);
		mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

		msleep(100);

#ifdef CONFIG_TOUCHSCREEN_MSG2133_SUPPORT_ISP
		msg2133_fts_isp_register(i2c_client);
		mutex_init(&tp_mutex);
#endif
	

#ifdef __FIRMWARE_UPDATE__

	TPD_DEBUG("Magnum i2c client name ==== %s\n",client->name);
	firmware_class = class_create(THIS_MODULE,client->name);// "ms-touchscreen-msg20xx");
//	firmware_class = class_create(THIS_MODULE,"ms-touchscreen-msg20xx");// "ms-touchscreen-msg20xx");

	if(IS_ERR(firmware_class))
	{
		TPD_DEBUG("Failed to create class(firmware)!\n");
	}

	firmware_cmd_dev = device_create(firmware_class,NULL, 0, NULL, "device");

	if(IS_ERR(firmware_cmd_dev))
	{
		TPD_DEBUG("Failed to create device(firmware_cmd_dev)!\n");
	}

	// version
	if(device_create_file(firmware_cmd_dev, &dev_attr_version) < 0)
	{
		TPD_DEBUG("Failed to create device file(%s)!\n", dev_attr_version.attr.name);
	}

	// update
	if(device_create_file(firmware_cmd_dev, &dev_attr_update) < 0)
	{
		TPD_DEBUG("Failed to create device file(%s)!\n", dev_attr_update.attr.name);
	}

	// data
	if(device_create_file(firmware_cmd_dev, &dev_attr_data) < 0)
	{
		TPD_DEBUG("Failed to create device file(%s)!\n", dev_attr_data.attr.name);
	}

	dev_set_drvdata(firmware_cmd_dev, NULL);
#endif

	mythread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
	if (IS_ERR(mythread))
	{
		error = PTR_ERR(mythread);
		TPD_DMESG(TPD_DEVICE " failed to create kernel thread: %d\n", error);
	}
	TPD_DMESG("Touch Panel Device Probe %s\n", (retval < TPD_OK) ? "FAIL" : "PASS");
#ifdef CTP_SIMULATE_PS
	tpd_load_status_ok=1;
#endif
	//edit by Magnum 2012-8-8 try to soleve tp go bad in FTM
	tpd_initialize(client);
	return retval;

 }

 static int tpd_initialize(struct i2c_client * client)
 {
	int retval = TPD_OK;
	tpd_reset();
	return retval;
 }
 static int __devexit tpd_i2c_remove(struct i2c_client *client)

 {
	int error;
	i2c_unregister_device(client);
	mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#ifdef CONFIG_TOUCHSCREEN_MSG2133_SUPPORT_ISP
	misc_deregister(&msg2133_fts_isp_device);
	wake_lock_destroy(&g_pts->wake_lock);
	g_pts = NULL;
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&early_suspend);
#endif /* CONFIG_HAS_EARLYSUSPEND */
	input_unregister_device(tpd->dev);
	TPD_DEBUG("TPD removed\n");

	return 0;
 }

static  struct i2c_driver tpd_i2c_driver = {
    .probe = tpd_i2c_probe,
    .remove =  __devexit_p(tpd_i2c_remove),
    .detect = tpd_i2c_detect,
    .driver.name = "msg2133",//TPD_DEVICE,
    .id_table = msg2133_i2c_id,
//    .address_data = &addr_data,
};

static int tpd_local_init(void)
 {
	int retval ,i;
	boot_mode = get_boot_mode();
	// Software reset mode will be treated as normal boot
	if(boot_mode==3) boot_mode = NORMAL_BOOT;
	TPD_DEBUG("tpd_local_init boot mode = %d\n",boot_mode);
	TPD_DMESG("MSG2133 I2C Touchscreen Driver (Built %s @ %s)\n", __DATE__, __TIME__);
	retval = i2c_add_driver(&tpd_i2c_driver);

#ifdef TPD_HAVE_BUTTON
	if (FACTORY_BOOT == boot_mode)
	{
		TPD_DEBUG("Magnum i am in Factory mode\n");
		for (i = 0; i < TPD_KEY_COUNT ; i++)
		tpd_keys_local[i] = TPD_KEYSFACTORY[i];
	}
	tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);// initialize tpd button data
#endif

	return retval;

 }

static int tpd_resume(struct i2c_client *client)
{
	int retval = TPD_OK;

#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
printk("[SWEEP2WAKE]: resume\n");
	scr_suspended = false;
	if (sweep2wake == 0 && doubletap2wake == 0)
#endif
{

	TPD_DEBUG("Magnum  ==%d!\n",ctp_suspend);
	if(ctp_suspend){
		TPD_DEBUG("ctp not suspend, resume depends on suspend...!\n");
		 return TPD_OK;
	}
#ifdef __FIRMWARE_UPDATE__

	if(update_switch==1)
	{
		return 0;
	}
#endif

	hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_2800, "TP");
	mt_set_gpio_mode(MSG_TPIO_RESET, 0x00);		//GPIO mode
	mt_set_gpio_pull_enable(MSG_TPIO_RESET, 1);	//external pull up???
	mt_set_gpio_pull_select(MSG_TPIO_RESET, GPIO_PULL_UP);
	mt_set_gpio_dir(MSG_TPIO_RESET,1);
	mt_set_gpio_out(MSG_TPIO_RESET,1);
	msleep(300);
	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	//    //200
	TPD_DEBUG("==========close cust eint touch panel reset\n");
	//   mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

	//ensure ctp can work after resume. edit by Magnum 2012-9-5
	int i = 0; int err = -1;
	for(i;i<5;i++){
		err = msg2133_ts_get_fw_version();
		if (err == 1)
		    break;
		else
		    tpd_reset();
		msleep(200);
	}
	ctp_suspend = 1;   // reset;

}
 #ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
	else if (sweep2wake > 0 || doubletap2wake > 0)
	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif

	return retval;
 }

static int tpd_suspend(struct i2c_client *client, pm_message_t message)
{
	int retval = TPD_OK;

#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
	scr_suspended = true;
printk("[SWEEP2WAKE]: early suspernd\n");
	if (sweep2wake == 0 && doubletap2wake == 0)
#endif
{

	TPD_DEBUG("================Magnum_TPD enter sleep\n");
#ifdef CTP_SIMULATE_PS
	TPD_DEBUG("Magnum ps_mode ==%d\n",ps_mode);
	if(ps_mode == 1){
		TPD_DEBUG("Magnum ps_mode ==%d, simulate PS,do not suspend!\n",ps_mode);
		 return TPD_OK;
	}
	else{
		TPD_DEBUG("Magnum ps_mode ==%d, suspend!!!!\n",ps_mode);
	}
#endif
#ifdef __FIRMWARE_UPDATE__

	if(update_switch==1)
	{
		return 0;
	}
#endif

	// mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);  // attention:  do not work ..
	TPD_DEBUG("Magnum Power Down suspend!!!!\n");
	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_EINT_PIN, GPIO_OUT_ZERO);
	/*#ifdef __FIRMWARE_UPDATE__

	if(update_switch==0)
#endif*/
	{
	msleep(10);
	hwPowerDown(MT65XX_POWER_LDO_VGP2, "TP");
	mt_set_gpio_mode(MSG_TPIO_RESET, 0x00);		//GPIO mode
	mt_set_gpio_pull_enable(MSG_TPIO_RESET, 1);	//external pull up???
	mt_set_gpio_pull_select(MSG_TPIO_RESET, GPIO_PULL_DOWN);
	mt_set_gpio_dir(MSG_TPIO_RESET,1);
	mt_set_gpio_out(MSG_TPIO_RESET,1);  //
	ctp_suspend = 0;
	TPD_DEBUG("Magnum suspend END....\n");
	}

}
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
	else if (sweep2wake > 0 || doubletap2wake > 0)
	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif

	return retval;
 }

#ifdef CONFIG_HAS_EARLYSUSPEND
 static void tpd_early_suspend(struct early_suspend *handler)
 {
	 tpd_suspend(i2c_client, PMSG_SUSPEND);
 }

 static void tpd_late_resume(struct early_suspend *handler)
 {
	 tpd_resume(i2c_client);
 }
#endif

static struct tpd_driver_t tpd_device_driver = {
		.tpd_device_name = "MSG2133",
		.tpd_local_init = tpd_local_init,
		.suspend = tpd_early_suspend,
		.resume = tpd_late_resume,
#ifdef TPD_HAVE_BUTTON
		.tpd_have_button = 1,
#else
		.tpd_have_button = 0,
#endif
		.tpd_x_res = TPD_CUST_RES_X,
		.tpd_y_res = TPD_CUST_RES_Y,	//including button area
		.tpd_get_fw_version = msg2133_get_fw_version_stored,
		.tpd_get_vendor_version = msg2133_get_vendor_version_stored,
#ifdef CTP_SIMULATE_PS    //edit by Magnum 2013-5-16
		.ctp_get_ps = msg2133_get_ps,
		.ctp_convert =ctp_convert,
#endif
};
/* called when loaded into kernel */

 static int __init tpd_driver_init(void)
{
	TPD_DEBUG("MediaTek MSG2133 touch panel driver init\n");
	i2c_register_board_info(1, &msg2133_i2c_tpd, 1);
	if(tpd_driver_add(&tpd_device_driver) < 0)
	{
		TPD_DEBUG("add MSG2133 driver failed\n");
	}
	return 0;
}

/* should never be called */
static void __exit tpd_driver_exit(void)
{
	TPD_DEBUG("MediaTek MSG2133 touch panel driver exit\n");
	//input_unregister_device(tpd->dev);
	tpd_driver_remove(&tpd_device_driver);
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);
