/* Copyright Statement:
 *
 * (C) 2005-2016  MediaTek Inc. All rights reserved.
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. ("MediaTek") and/or its licensors.
 * Without the prior written permission of MediaTek and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 * You may only use, reproduce, modify, or distribute (as applicable) MediaTek Software
 * if you have agreed to and been bound by the applicable license agreement with
 * MediaTek ("License Agreement") and been granted explicit permission to do so within
 * the License Agreement ("Permitted User").  If you are not a Permitted User,
 * please cease any access or use of MediaTek Software immediately.
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT MEDIATEK SOFTWARE RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES
 * ARE PROVIDED TO RECEIVER ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 */

#include "hal_flash.h"
#ifdef HAL_FLASH_MODULE_ENABLED

#include <string.h>
#include <stdlib.h>

#include "type_def.h"
#include "sfc.h"
#include "spi_flash.h"

#include "debug.h"
#include "mem_util.h"
#include "timer.h"
#include "cos_api.h"
#include "gpt.h"
#include "nvic.h"
#include "hal_nvic_internal.h"
#include "hal_log.h"
#include "memory_attribute.h"

#define sfc_time_after(a,b)  ((long)(b) - (long)(a) < 0)
#define sfc_time_before(a,b) sfc_time_after(b,a)

unsigned int g_sfc_security_enable = 0;



ATTR_TEXT_IN_TCM kal_uint32 sfc_GPT_return_current_count()
{
    kal_uint32 current_count;

    current_count = DRV_Reg32(GPT2_CNT);

    return current_count;
}

ATTR_TEXT_IN_TCM kal_uint32 sfc_get_systime(void)
{
    return sfc_GPT_return_current_count();
}

ATTR_TEXT_IN_TCM void sfc_delay_time(kal_uint32 count)
{
    kal_uint32 end_count, current;

    end_count = sfc_get_systime() + count + 1;
    current = sfc_get_systime();
    while (sfc_time_before(current, end_count)) {
        current = sfc_get_systime();
    }
}

ATTR_TEXT_IN_TCM void sfc_delay_ms(unsigned int ms)
{
    sfc_delay_time(32 * ms);	/* GPT2 based counter */
}

ATTR_TEXT_IN_TCM int sfc_getMaxGPRamSize(void)
{
    return	(MAX_SFC_GPRAM_SIZE - 32);	/* 160 actually in MT7637 HW design */
}

#if 0
/*
 * for DVT only.
 */
ATTR_TEXT_IN_TCM void sfc_set_release_mac(int release_mac)
{
    unsigned long reg;
    reg = SFC_ReadReg32(RW_SFC_MAC_CTL);

    if (release_mac) {
        reg = reg | SFC_RELEASE_MAC;
    } else {
        reg = reg & ~(SFC_RELEASE_MAC);
    }
    SFC_WriteReg32(RW_SFC_MAC_CTL, reg);
}

/*
 * for DVT only
 */
ATTR_TEXT_IN_TCM inline unsigned long sfc_get_mac_control(void)
{
    return SFC_ReadReg32(RW_SFC_MAC_CTL);
}
#endif //DVT_ONLY

ATTR_TEXT_IN_TCM void sfc_Init(int security_enable)
{
    unsigned int reg;

    g_sfc_security_enable = 0;


    /*
     * TODO/FIXME:   CM4 Rom code has initialized these argument by eFuse, and we may ignore
     *               configure them again here.
     */
#ifdef LOADER_COSIM_ENABLE
    SFC_WriteReg32(RW_SFC_MAC_CTL,	0x00010000);
    SFC_WriteReg32(RW_SFC_DIRECT_CTL,	0x0B0B7790);	/* all: IC reset default value */
    SFC_WriteReg32(RW_SFC_MISC_CTL,	0xB2000100);	/* all: IC reset default value, except "NO_RELOAD" */
    SFC_WriteReg32(RW_SFC_MISC_CTL2,	0x00000000);
    SFC_WriteReg32(RW_SFC_MAC_OUTL,	0x00000000);
    // Note: SFC_base + 0x18 will not be cleared after WDT reset / time out
    SFC_WriteReg32(RW_SFC_STA2_CTL,	0x00000000);
#else
    SFC_WriteReg32(RW_SFC_MAC_CTL,	0x00010000);
    SFC_WriteReg32(RW_SFC_DIRECT_CTL,	0x0B0B7790);	/* all: IC reset default value */
    SFC_WriteReg32(RW_SFC_MISC_CTL,	0xB2C00010);	/* all: IC reset default value, except "NO_RELOAD" and "latch delay" */
    SFC_WriteReg32(RW_SFC_MISC_CTL2,	0x00000000);
    SFC_WriteReg32(RW_SFC_MAC_OUTL,	0x00000000);
    // Note: SFC_base + 0x18 will not be cleared after WDT reset / time out
    SFC_WriteReg32(RW_SFC_STA2_CTL,	0x00000000);
#endif

    reg = SFC_ReadReg32(RW_SFC_MISC_CTL);
    reg &= ~(0x00400000);
    SFC_WriteReg32(RW_SFC_MISC_CTL,	reg);	/* all: IC reset default value, except "NO_RELOAD"  */
    sfc_delay_ms(1);

    reg = SFC_ReadReg32(RW_SFC_MISC_CTL);
    reg |= 0x00400000;
    SFC_WriteReg32(RW_SFC_MISC_CTL,	reg);	/* all: IC reset default value, except "NO_RELOAD"  */
    sfc_delay_ms(1);

    reg = SFC_ReadReg32(RW_SFC_MISC_CTL);
    reg &= ~(0x00400000);
    SFC_WriteReg32(RW_SFC_MISC_CTL,	reg);	/* all: IC reset default value, except "NO_RELOAD"  */
    sfc_delay_ms(1);

    reg = SFC_ReadReg32(RW_SFC_MISC_CTL);
    reg |= 0x00400000;
    SFC_WriteReg32(RW_SFC_MISC_CTL,	reg);	/* all: IC reset default value, except "NO_RELOAD"  */
    sfc_delay_ms(1);

    /*
     * Security Boot Stuff Init
     */
    if (security_enable) {
        g_sfc_security_enable = 1;
//        SFC_WriteReg32(CM4_SFC_BASE + 0x8100, 0xDEADBEEF);
//        SFC_WriteReg32(CM4_SFC_BASE + 0x8104, 0x12345678);
//        SFC_WriteReg32(CM4_SFC_BASE + 0x8108, 0x23456789);
//        SFC_WriteReg32(CM4_SFC_BASE + 0x810C, 0x3456789A);

//        SFC_WriteReg32(CM4_SFC_BASE + 0x8124, 0x00050000);
//        SFC_WriteReg32(CM4_SFC_BASE + 0x8128, 0x50000005);
//        SFC_WriteReg32(CM4_SFC_BASE + 0x812C, 0x00000005);

        SFC_WriteReg32(CM4_SFC_BASE + 0x8000, 0x10000000 /* + offset */);
        SFC_WriteReg32(CM4_SFC_BASE + 0x8004, 0x10000000 /* + offset */);

        SFC_WriteReg32(CM4_SFC_BASE + 0x8008, 0x00000003);
        SFC_WriteReg32(CM4_SFC_BASE + 0x8008, 0x00000000);
        SFC_WriteReg32(CM4_SFC_BASE + 0x800C, 0x00000087);
        SFC_WriteReg32(CM4_SFC_BASE + 0x800C, 0x00000000);

        SFC_WriteReg32(CM4_SFC_BASE + 0x8014, 0x00011000);
    }
}

ATTR_TEXT_IN_TCM int sfc_GetDirectMode(void)
{
    unsigned long qpi_en = (SFC_ReadReg32(RW_SFC_DIRECT_CTL) & SFC_QPI_EN);
    unsigned long sfc_read_mode = (SFC_ReadReg32(RW_SFC_DIRECT_CTL) & SFC_DR_MODE_MASK) >> SFC_DR_MODE_OFFSET;

    /*
     * TODO: confirm with designer???!!!
     */
    if (sfc_read_mode == 0x7 /* && qpi_en == 0*/) {
        return QPI;
    }
    if (qpi_en) {
        return QPI;
    }
    if (sfc_read_mode == 0x0) {
        return SPI;
    }
    if (sfc_read_mode == 0x1) {
        return SPI;
    }
    return SF_UNDEF;

}


ATTR_TEXT_IN_TCM void sfc_direct_read_setup(unsigned long reg, int mode)
{
    unsigned long tmp = 0x0;
    switch (mode) {
        case SPI:
            tmp |= SFC_DR_SPI_FAST_READ_MODE;
            break;
        case SPIQ:
            tmp |= SFC_DR_QPI_FAST_READ_MODE;
            break;
        case QPI:
            tmp |= SFC_QPI_EN;
            tmp |= SFC_DR_QPI_FAST_READ_MODE;
            break;
    }
    tmp |= reg;
    SFC_WriteReg32(RW_SFC_DIRECT_CTL, tmp);
}


#if 0
void sfc_Switch_Controller_Mode(int mode, int dummycyc, unsigned char read_cmd)
{
    unsigned int direct;
    unsigned int dummy;

    dummy = (dummycyc - 1) & 0xF;

    direct  =  SFC_ReadReg32(RW_SFC_DIRECT_CTL);
    direct &= ~SFC_DR_MODE_MASK;

    switch (mode)   {
        case SPI:
            direct &= ~SFC_QPI_EN;
            direct |=  SFC_DR_SPI_FAST_READ_MODE;
            break;
        case SPIQ:
            /* TODO !!?? */
            ASSERT(0);
            direct &= ~SFC_QPI_EN;
            direct |=  SFC_DR_QPI_FAST_READ_MODE;
            break;
        case QPI:
            /* TODO !!!??? */
            //direct |= SFC_QPI_EN;
            direct &= ~SFC_QPI_EN;
            direct |=  SFC_DR_QPI_FAST_READ_MODE;
            break;
        default:
            ASSERT(0);
    }

    direct &= ~(SFC_DR_CMD1_DUMMY_CYC_MASK | SFC_DR_CMD2_DUMMY_CYC_MASK);
    direct |= ((dummy << SFC_DR_CMD2_DUMMY_CYC_OFFSET) | (dummy << SFC_DR_CMD1_DUMMY_CYC_OFFSET));

    if (read_cmd > 0) {
        direct &= ~SFC_DR_CMD1_MASK;
        direct |= ((read_cmd & 0xFF) << SFC_DR_CMD1_OFFSET);
    }

    SFC_WriteReg32(RW_SFC_DIRECT_CTL, direct);
}
#endif


//-----------------------------------------------------------------------------
/*!
  @brief
    Issue generic command List to serial Flash

  @param[in] cmdlist Pointer to the list of the commands that to be sent to serial Flash.

  @remarks This function is only called by bootloader.
           Other callers must be aware of interrupts during the MAC mode
*/
ATTR_TEXT_IN_TCM void sfc_Command_List(const unsigned char *cmdlist)
{
    unsigned int i = 0;
    unsigned int val, force_qpi = 0;

    if (!cmdlist) {
        return;
    }

    val = SFC_ReadReg32(RW_SFC_MAC_CTL); // denotes the orignal mode
    /*
        qpi = val |  SFC_QPI_EN;
        spi = val &~ SFC_QPI_EN;
    */
    while (1)   {
        switch (cmdlist[i]) {
            case SPI:
            case SPIQ:
                //SFC_WriteReg32(RW_SFC_MAC_CTL, spi);
                break;   // Switch to SPI mode
            case QPI:
                force_qpi = 1;
                //SFC_WriteReg32(RW_SFC_MAC_CTL, qpi);
                break;   // Switch to QPI mode
            default:
                SFC_WriteReg32(RW_SFC_MAC_CTL, val);
                return;  // Resume to orignal mode and returns
        }

        sfc_transfer(&cmdlist[i + 2], cmdlist[i + 1], NULL, 0, force_qpi); // send command

        i += (cmdlist[i + 1] + 2);

        flash_wait_ready(1);
    }
}


//-----------------------------------------------------------------------------
/*!
  @brief
    Send commands placed in GPRAM (in macro mode)
*/
ATTR_TEXT_IN_TCM static unsigned long sfc_MacTrigger(void)
{
    unsigned long  val = 0;
    unsigned long  misc_ctl = 0;

    val = SFC_ReadReg32(RW_SFC_MAC_CTL);

    // trigger SFC
    val |= (SFC_TRIG | SFC_MAC_EN);

    SFC_WriteReg32(RW_SFC_MAC_CTL, val);

    // wait for SFC ready
    while (!(SFC_ReadReg32(RW_SFC_MAC_CTL) & SFC_WIP_READY))
        ;
    while ((SFC_ReadReg32(RW_SFC_MAC_CTL) & SFC_WIP))
        ;

    return misc_ctl;
}

/*!
  @brief
    Leaves macro mode
  @remarks
    MT6251, MT6252, MT6255
*/
ATTR_TEXT_IN_TCM static void sfc_MacLeave(void)
{
    unsigned long val;

    // clear SF_TRIG and leave MACRO mode
    val = SFC_ReadReg32(RW_SFC_MAC_CTL);
    val &= ~(SFC_TRIG | SFC_MAC_XIO_SEL | SFC_MAC_SEL);
    SFC_WriteReg32(RW_SFC_MAC_CTL, val);

    //Disable SFC_MAC_EN must after polling SFC_WIP_READY for MT6250 HW DCM
    //Because once SFC_MAC_EN is disabled, Idle bit of SFC will be set, HW DCM may gate SFCLK.
    //Then SFC_WIP_READAY will keep high because it depends on SFCLK.
    val = SFC_ReadReg32(RW_SFC_MAC_CTL);
    val &= ~(SFC_MAC_EN);
    SFC_WriteReg32(RW_SFC_MAC_CTL, val);
}


ATTR_TEXT_IN_TCM static void sfc_MacWaitReady(void)
{
#ifdef SF_1T_WORKAROUND
    unsigned long  misc_ctl;
#endif
    //unsigned long reg;

#ifdef SF_1T_WORKAROUND
    misc_ctl = SFC_MacTrigger();
    sfc_MacLeave();
    SFC_WriteReg32(RW_SFC_MISC_CTL, misc_ctl);
#else
    sfc_MacTrigger();
    sfc_MacLeave();
#endif
}


ATTR_TEXT_IN_TCM static void sfc_MacEnable(int force_qpi)
{
    unsigned long val;

    val = SFC_ReadReg32(RW_SFC_MAC_CTL);

    if (SFC_IsQPIMode() || force_qpi)    {	/* if direct mode is QPI, then Macro mode is QPI also.*/
        val |= SFC_MAC_XIO_SEL;
    }

    val |= SFC_MAC_EN;

    SFC_WriteReg32(RW_SFC_MAC_CTL, val);
}

ATTR_TEXT_IN_TCM int sfc_transfer(const unsigned char *cmd, const unsigned long cmd_len, unsigned char *data, const unsigned long data_len, unsigned int force_qpi)
{
    unsigned int	tmp, i, j;
    unsigned char	*p_data, *p_tmp;
    unsigned int savedMask;

    if ((cmd_len + data_len) > MAX_SFC_GPRAM_SIZE) {
        return -1;
    }

    p_tmp = (unsigned char *)(&tmp);
    p_data = ((unsigned char *)RW_SFC_GPRAM_DATA);

    for (i = 0; i < cmd_len; p_data += 4) {
        for (j = 0, tmp = 0; i < cmd_len && j < 4; i++, j++) {
            p_tmp[j] = cmd[i];
        }
        SFC_WriteReg32(p_data, tmp);

        if (i == 4 && g_sfc_security_enable) {
            SFC_WriteReg32(CM4_SFC_BASE + 0x8008, (1 << 6)); //0x00000040;
            SFC_WriteReg32(CM4_SFC_BASE + 0x800C, (1 << 8)); //0x00000100;

        }
    }

    savedMask = save_and_set_interrupt_mask();
    if (g_sfc_security_enable) {
        SFC_WriteReg32(CM4_SFC_BASE + 0x8008, 0x00000000);
        SFC_WriteReg32(CM4_SFC_BASE + 0x800C, 0x00000000);
    }

    SFC_WriteReg32(RW_SFC_MAC_OUTL, cmd_len);
    SFC_WriteReg32(RW_SFC_MAC_INL, data_len);

    sfc_MacEnable(force_qpi);
    sfc_MacWaitReady();
    restore_interrupt_mask(savedMask);

    for (i = 0, p_data = ((unsigned char *)RW_SFC_GPRAM_DATA + cmd_len); i < data_len; ++i, ++data, ++p_data) {
        *data = SFC_ReadReg8(p_data);
    }

    return data_len;
}

ATTR_TEXT_IN_TCM void sfc_clear_gpram(void)
{
    unsigned int	i;
    unsigned char	*p_data;

    p_data = ((unsigned char *)RW_SFC_GPRAM_DATA);

    for (i = 0; i < MAX_SFC_GPRAM_SIZE / 4; p_data += 4, i++) {
        SFC_WriteReg32(p_data, 0);
    }

    return;
}


ATTR_TEXT_IN_TCM int sfc_read(const unsigned char *cmd, const unsigned long cmd_len, unsigned char *data, const unsigned long data_len)
{
    return sfc_transfer(cmd, cmd_len, data, data_len, 0 /* follow direct mode setting */);
}

#if 0
ATTR_TEXT_IN_TCM int sfc_security_read(const unsigned char *cmd, const unsigned long cmd_len, unsigned char *data, const unsigned long data_len)
{
    return sfc_transfer(cmd, cmd_len, data, data_len, 0 /* follow direct mode setting */);
}
#endif

ATTR_TEXT_IN_TCM int sfc_write(const unsigned char *cmd, const unsigned long cmd_len, const unsigned char *data, const unsigned long data_len)
{
    unsigned char tmp[MAX_SFC_GPRAM_SIZE];
    int total = cmd_len + data_len;

    if (total >= sizeof(tmp)) {
        ASSERT(0);
    }
    memcpy(tmp, cmd, cmd_len);

    if (data && data_len) {
        memcpy(&tmp[cmd_len], data, data_len);
    }
    return sfc_transfer(tmp, total , NULL, 0, 0 /* follow direct mode setting */);
}

ATTR_TEXT_IN_TCM int sfc_security_write_crc(const unsigned char *cmd, const unsigned long cmd_len)
{
    unsigned char tmp[MAX_SFC_GPRAM_SIZE];
    unsigned long crc = SFC_ReadReg32(CM4_SFC_BASE + 0x803C);
    int total = cmd_len + sizeof(crc) /* crc length */;

    //log_hal_info("%08x\n", (unsigned int)crc);

    memcpy(tmp, cmd, cmd_len);
    memcpy(&tmp[cmd_len], &crc, sizeof(crc));

    return sfc_transfer(tmp, total , NULL, 0, 0 /* follow direct mode setting */);
}

#if 0
int sfc_security_write(const unsigned char *cmd, const unsigned long cmd_len, unsigned char *data, const unsigned long data_len)
{
    unsigned char tmp[MAX_SFC_GPRAM_SIZE];
    int total = cmd_len + data_len;

    if (total >= sizeof(tmp)) {
        ASSERT(0);
    }
    memcpy(tmp, cmd, cmd_len);

    if (data && data_len) {
        memcpy(&tmp[cmd_len], data, data_len);
    }
    return sfc_transfer(tmp, total , NULL, 0, 0 /* follow direct mode setting */);
}
#endif

ATTR_TEXT_IN_TCM int __sfc_write(const unsigned char *cmd, const unsigned long cmd_len, unsigned char *data, const unsigned long data_len, unsigned int security_enable)
{
    unsigned char tmp[160];
    int total = cmd_len + data_len;

    if (total > sizeof(tmp)) {
        ASSERT(0);
    }
    memcpy(tmp, cmd, cmd_len);

    if (data && data_len) {
        memcpy(&tmp[cmd_len], data, data_len);
    }
    return sfc_transfer(tmp, total , NULL, 0, 0 /* follow direct mode setting */);
}
#endif
