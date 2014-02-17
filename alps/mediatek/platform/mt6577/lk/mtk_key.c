/*
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */


#include <platform/mt_typedefs.h>
#include <platform/mtk_key.h>
#include <platform/boot_mode.h>
#include <platform/mt_pmic.h>

#include <target/cust_key.h>

extern mt6329_detect_powerkey(void);
extern mt6329_detect_homekey(void);

BOOL mtk_detect_key(unsigned short key)	/* key: HW keycode */
{
	unsigned short idx, bit, din;

	if (key >= KPD_NUM_KEYS)
		return FALSE;

	if (key % 9 == 8)
		key = 8;

#if 0 //KPD_PWRKEY_USE_EINT 
    if (key == 8)
    {                         /* Power key */
        idx = KPD_PWRKEY_EINT_GPIO / 16;
        bit = KPD_PWRKEY_EINT_GPIO % 16;

        din = DRV_Reg16 (GPIO_DIN_BASE + (idx << 4)) & (1U << bit);
        din >>= bit;
        if (din == KPD_PWRKEY_GPIO_DIN)
        {
            dbg_print ("power key is pressed\n");
            return true;
        }
        return false;
    }
#else // check by PMIC
    if (key == 8)
    {                         /* Power key */
        if (1 == mt6329_detect_powerkey())
        {
            //dbg_print ("power key is pressed\n");
            return TRUE;
        }
        return FALSE;
    }    
#endif

#if 1
	if(get_chip_eco_ver() == CHIP_E2) /* for E1 board's VR301 issue */
	{
		if (key == MT65XX_PMIC_RST_KEY) 
		{
			if (1 == mt6329_detect_homekey())
			{
				return TRUE;
			}
			// return false; /* to support common EVB */
		}
	}
#endif
	idx = key / 16;
	bit = key % 16;

	din = DRV_Reg16(KP_MEM1 + (idx << 2)) & (1U << bit);
	if (!din) {
		printf("key %d is pressed\n", key);
		return TRUE;
	}
	return FALSE;
}

BOOL mtk_detect_pmic_just_rst()
{
	//kal_uint8 just_rst;
	kal_uint32 just_rst=0;
	kal_uint32 ret=0;
	
	printf("detecting pmic just reset\n");
	if(get_chip_eco_ver() == CHIP_E2)
	{
		ret=pmic_read_interface(0x15, &just_rst, 0x01, 0x07);
		if(just_rst)
		{
			printf("Just recover form a reset\n"); 
			pmic_bank1_config_interface(0x22, 0x01, 0x01, 0x07);
			return TRUE;
		}
	}
	return FALSE;
}


BOOL mt65XX_detect_special_key(void)
{
	unsigned short idx1, idx2, bit1, bit2, din, din1, din2;
  
#ifdef TINNO_PROJECT_S9070
	      idx1 = MT65XX_FACTORY_KEY / 16;
	      bit1 = MT65XX_FACTORY_KEY % 16;	
	      din1 = DRV_Reg16(KP_MEM1 + (idx1 << 2)) & (1U << bit1);
	      printf("mt6577_detect_key MT65XX_FACTORY_KEY din1=%x\n",din1);
#else
	      din1 = mt6329_detect_homekey();
               printf("mt6577_detect_key mt6577_detect_homekey din1=%x\n",din1);
	      if (din1 == 1)
	       din1 = 0;
	      else
	       din1 = 1;
#endif


	idx2 = MT65XX_RECOVERY_KEY / 16;
	bit2 = MT65XX_RECOVERY_KEY % 16;
	din2 = DRV_Reg16(KP_MEM1 + (idx2 << 2)) & (1U << bit2);
	printf("mt6577_detect_key din2=%x\n",din2);

	din = din1 + din2;
	printf("mt6577_detect_key din=%x\n",din);

	if (!din) {
		//printf("key %d is pressed\n", key);
		return TRUE;
	}
	return FALSE;
}

unsigned short mt65XX_get_key(void)
{
	unsigned short idx, bit, din;

	din = DRV_Reg16(KP_MEM1);

	printf("mt6577_detect_key din=%x\n",din);

	return din;
}

