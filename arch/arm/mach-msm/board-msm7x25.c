/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2009, Code Aurora Forum. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/bootmem.h>
#include <linux/usb/mass_storage_function.h>
#include <linux/power_supply.h>

#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/flash.h>
#include <asm/setup.h>

#include <asm/mach/mmc.h>
#include <mach/vreg.h>
#include <mach/mpp.h>
#include <mach/gpio.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/msm_rpcrouter.h>
#include <mach/msm_hsusb.h>
#include <mach/rpc_hsusb.h>
#include <mach/rpc_pmapp.h>
#include <mach/msm_serial_hs.h>
#include <mach/memory.h>

#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/i2c.h>
#include <linux/android_pmem.h>
#include <mach/camera.h>

#include "devices.h"
#include "socinfo.h"
#include "clock.h"
#include "msm-keypad-devices.h"
#include "pm.h"

#include "linux/hardware_self_adapt.h"
//add by mzh
#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI_TM1319
#include <linux/synaptics_i2c_rmi.h>
#endif


#ifdef CONFIG_HUAWEI_MSM_VIBRATOR
#include "msm_vibrator.h"
#endif

#ifdef CONFIG_HUAWEI_FEATURE_OFN_KEY
#include "msm_ofn_key.h"
#endif

#ifdef CONFIG_HUAWEI_JOGBALL
#include "jogball_device.h"
#endif

#ifdef CONFIG_USB_AUTO_INSTALL
#include "../../../drivers/usb/function/usb_switch_huawei.h"
#include "../../../arch/arm/mach-msm/proc_comm.h"
#include "smd_private.h"

#define USB_SERIAL_LEN 20
static unsigned char usb_serial_num[USB_SERIAL_LEN]="hw";
/* keep the parameters transmitted from SMEM */
smem_huawei_vender usb_para_data;

/* keep the boot mode transfered from APPSBL */
unsigned int usb_boot_mode = 0;

/* keep usb parameters transfered from modem */
app_usb_para usb_para_info;

/* all the pid used by mobile */
usb_pid_stru usb_pid_array[]={
    {PID_ONLY_CDROM,     PID_NORMAL,     PID_UDISK, PID_AUTH},     /* for COMMON products */
    {PID_ONLY_CDROM_TMO, PID_NORMAL_TMO, PID_UDISK, PID_AUTH_TMO}, /* for TMO products */
};

/* pointer to the member of usb_pid_array[], according to the current product */
usb_pid_stru *curr_usb_pid_ptr = &usb_pid_array[0];
void set_usb_sn(char *sn_ptr);
#endif 

/* add lcd_id and camera_id for uni platform purpers */
static unsigned int camera_id = 0;
static unsigned int lcd_id = 0;
static unsigned int ts_id = 0;
static unsigned int sub_board_id = 0;

/* prealloc memory for 7x25 platform */
#define MSM_PMEM_MDP_SIZE	0xb21000
#define MSM_PMEM_ADSP_SIZE	0xc00000
#define MSM_FB_SIZE		0x200000
#define PMEM_KERNEL_EBI1_SIZE	0x80000

static DEFINE_MUTEX(lcdc_config);
//add by mzh
#ifdef CONFIG_HUAWEI_BATTERY

static struct platform_device huawei_battery_device = {
	.name = "huawei_battery",
	.id		= -1,
};

#endif
#ifdef CONFIG_USB_FUNCTION
/* add a lun for MS in autorun feature */
static struct usb_mass_storage_platform_data usb_mass_storage_pdata = {
#ifdef CONFIG_USB_AUTO_INSTALL
    .nluns          = 0x01,
#endif
    .buf_size       = 16384,
    .vendor         = "Android",
    .product        = "Adapter",
	.release        = 0xffff,
};
static struct usb_mass_storage_platform_data usb_mass_storage_tmo_pdata = {
#ifdef CONFIG_USB_AUTO_INSTALL
    .nluns          = 0x01,
#endif
    .buf_size       = 16384,
    .vendor         = "T-Mobile",
    .product        = "3G Phone",
	.release        = 0xffff,
};

static struct platform_device mass_storage_device = {
	.name           = "usb_mass_storage",
	.id             = -1,
	.dev            = {
		.platform_data          = &usb_mass_storage_pdata,
	},
};


static struct usb_function_map usb_functions_map[] = {
	{"modem", 0},
#ifdef CONFIG_HUAWEI_USB_FUNCTION_PCUI
	{"pcui", 1},
#endif // endif CONFIG_HUAWEI_USB_FUNCTION_PCUI
	{"mass_storage", 2},
	{"adb", 3},
	{"diag", 4},
#ifdef CONFIG_HUAWEI_USB_CONSOLE
	{"serial_console", 5},
#endif    
};

/* dynamic composition */
static struct usb_composition usb_tmo_func_composition[] = {
	{
		.product_id         = 0x9012,
		.functions	    = 0x5, /* 0101 */
	},

	{
		.product_id         = 0x9013,
		.functions	    = 0x15, /* 10101 */
	},

	{
		.product_id         = 0x9014,
		.functions	    = 0x30, /* 110000 */
	},

	{
		.product_id         = 0x9016,
		.functions	    = 0xD, /* 01101 */
	},

	{
		.product_id         = 0x9017,
		.functions	    = 0x1D, /* 11101 */
	},

	{
		.product_id         = 0xF000,
		.functions	    = 0x10, /* 10000 */
	},

	{
		.product_id         = 0xF009,
		.functions	    = 0x20, /* 100000 */
	},
/* add a usb composition for the only MS setting */
#ifdef CONFIG_USB_AUTO_INSTALL
	{
		.product_id         = PID_ONLY_CDROM_TMO,
		.functions	    = 0x04, /* 000100, ONLY CDROM */
	},
	{
		.product_id         = PID_UDISK,
		.functions	    = 0x04, /* 000100, ONLY UDISK */
	},
#endif
	{
        .product_id         = PID_NORMAL_TMO,
#ifdef CONFIG_HUAWEI_DIAG_DEBUG
#ifdef CONFIG_HUAWEI_USB_CONSOLE
	   .functions	    = 0x3F, /* 00111111 */
#else  // else CONFIG_HUAWEI_USB_CONSOLE
       .functions	    = 0x1F, /* 00011111 */
#endif // endif CONFIG_HUAWEI_USB_CONSOLE     

#else  // else CONFIG_HUAWEI_DIAG_DEBUG
       .functions       = 0x0F, /* 00001111 */
#endif // endif CONFIG_HUAWEI_USB_FUNCTION_PCUI
	},
	{
		.product_id     = PID_AUTH_TMO,
		.functions	    = 0x01F, /* 011111 */
	},
};

/* dynamic composition */
static struct usb_composition usb_func_composition[] = {
	{
		.product_id         = 0x9012,
		.functions	    = 0x5, /* 0101 */
	},

	{
		.product_id         = 0x9013,
		.functions	    = 0x15, /* 10101 */
	},

	{
		.product_id         = 0x9014,
		.functions	    = 0x30, /* 110000 */
	},

	{
		.product_id         = 0x9016,
		.functions	    = 0xD, /* 01101 */
	},

	{
		.product_id         = 0x9017,
		.functions	    = 0x1D, /* 11101 */
	},

	{
		.product_id         = 0xF000,
		.functions	    = 0x10, /* 10000 */
	},

	{
		.product_id         = 0xF009,
		.functions	    = 0x20, /* 100000 */
	},

/* add a usb composition for the only MS setting */
#ifdef CONFIG_USB_AUTO_INSTALL
	{
		.product_id         = PID_ONLY_CDROM,
		.functions	    = 0x04, /* 000100, ONLY CDROM */
	},
	{
		.product_id         = PID_UDISK,
		.functions	    = 0x04, /* 000100, ONLY UDISK */
	},
#endif
	{
        .product_id         = PID_NORMAL,
#ifdef CONFIG_HUAWEI_DIAG_DEBUG
#ifdef CONFIG_HUAWEI_USB_CONSOLE
	   .functions	    = 0x3F, /* 00111111 */
#else  // else CONFIG_HUAWEI_USB_CONSOLE
       .functions	    = 0x1F, /* 00011111 */
#endif // endif CONFIG_HUAWEI_USB_CONSOLE     

#else  // else CONFIG_HUAWEI_DIAG_DEBUG
       .functions       = 0x0F, /* 00001111 */
#endif // endif CONFIG_HUAWEI_USB_FUNCTION_PCUI
	},
	{
		.product_id     = PID_AUTH,
		.functions	    = 0x01F, /* 011111 */
	},
};
#endif

/* serial number should not be NULL when the luns is more than 1 */
static struct msm_hsusb_platform_data msm_hsusb_pdata = {
#ifdef CONFIG_USB_FUNCTION
	.version	= 0x0100,
	.phy_info	= (USB_PHY_INTEGRATED | USB_PHY_MODEL_65NM),
    .vendor_id          = 0x12D1,
    .product_name       = "Android Mobile Adapter",
    .serial_number      = NULL,
    .manufacturer_name  = "Huawei Incorporated",
	.compositions	= usb_func_composition,
	.num_compositions = ARRAY_SIZE(usb_func_composition),
	.function_map   = usb_functions_map,
	.num_functions	= ARRAY_SIZE(usb_functions_map),
	.config_gpio    = NULL,
#endif
};
static struct msm_hsusb_platform_data msm_hsusb_tmo_pdata = {
#ifdef CONFIG_USB_FUNCTION
	.version	= 0x0100,
	.phy_info	= (USB_PHY_INTEGRATED | USB_PHY_MODEL_65NM),
    .vendor_id          = 0x12D1,
    .product_name       = "T-Mobile 3G Phone",
    .serial_number      = NULL,
    .manufacturer_name  = "Huawei Incorporated",
	.compositions	= usb_tmo_func_composition,
	.num_compositions = ARRAY_SIZE(usb_func_composition),
	.function_map   = usb_functions_map,
	.num_functions	= ARRAY_SIZE(usb_functions_map),
	.config_gpio    = NULL,
#endif
};

#define SND(desc, num) { .name = #desc, .id = num }
static struct snd_endpoint snd_endpoints_list[] = {
	SND(HANDSET, 0),
	SND(MONO_HEADSET, 2),
	SND(HEADSET, 3),
	SND(SPEAKER, 6),
	SND(TTY_HEADSET, 8),
	SND(TTY_VCO, 9),
	SND(TTY_HCO, 10),
	SND(BT, 12),
	SND(IN_S_SADC_OUT_HANDSET, 16),
	SND(IN_S_SADC_OUT_SPEAKER_PHONE, 25),
#ifdef CONFIG_HUAWEI_KERNEL
	SND(FM_HEADSET, 26),
	SND(FM_SPEAKER, 27),
	SND(BT_EC_OFF,  28),
	SND(HEADSET_AND_SPEAKER,29),
	SND(CURRENT,  31),

#endif
};
#undef SND

static struct msm_snd_endpoints msm_device_snd_endpoints = {
	.endpoints = snd_endpoints_list,
	.num = sizeof(snd_endpoints_list) / sizeof(struct snd_endpoint)
};

static struct platform_device msm_device_snd = {
	.name = "msm_snd",
	.id = -1,
	.dev    = {
		.platform_data = &msm_device_snd_endpoints
	},
};

#define DEC0_FORMAT ((1<<MSM_ADSP_CODEC_MP3)| \
	(1<<MSM_ADSP_CODEC_AAC)|(1<<MSM_ADSP_CODEC_WMA)| \
	(1<<MSM_ADSP_CODEC_WMAPRO)|(1<<MSM_ADSP_CODEC_AMRWB)| \
	(1<<MSM_ADSP_CODEC_AMRNB)|(1<<MSM_ADSP_CODEC_WAV)| \
	(1<<MSM_ADSP_CODEC_ADPCM)|(1<<MSM_ADSP_CODEC_YADPCM)| \
	(1<<MSM_ADSP_CODEC_EVRC)|(1<<MSM_ADSP_CODEC_QCELP))
#define DEC1_FORMAT ((1<<MSM_ADSP_CODEC_WAV)|(1<<MSM_ADSP_CODEC_ADPCM)| \
	(1<<MSM_ADSP_CODEC_YADPCM)|(1<<MSM_ADSP_CODEC_QCELP)| \
	(1<<MSM_ADSP_CODEC_MP3))
#define DEC2_FORMAT ((1<<MSM_ADSP_CODEC_WAV)|(1<<MSM_ADSP_CODEC_ADPCM)| \
	(1<<MSM_ADSP_CODEC_YADPCM)|(1<<MSM_ADSP_CODEC_QCELP)| \
	(1<<MSM_ADSP_CODEC_MP3))

#ifdef CONFIG_ARCH_MSM7X25
#define DEC3_FORMAT 0
#define DEC4_FORMAT 0
#else
#define DEC3_FORMAT ((1<<MSM_ADSP_CODEC_WAV)|(1<<MSM_ADSP_CODEC_ADPCM)| \
	(1<<MSM_ADSP_CODEC_YADPCM)|(1<<MSM_ADSP_CODEC_QCELP))
#define DEC4_FORMAT (1<<MSM_ADSP_CODEC_MIDI)
#endif

static unsigned int dec_concurrency_table[] = {
	/* Audio LP */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DMA)), 0,
	0, 0, 0,

	/* Concurrency 1 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),

	 /* Concurrency 2 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),

	/* Concurrency 3 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),

	/* Concurrency 4 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),

	/* Concurrency 5 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),

	/* Concurrency 6 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),
};

#define DEC_INFO(name, queueid, decid, nr_codec) { .module_name = name, \
	.module_queueid = queueid, .module_decid = decid, \
	.nr_codec_support = nr_codec}

static struct msm_adspdec_info dec_info_list[] = {
	DEC_INFO("AUDPLAY0TASK", 13, 0, 11), /* AudPlay0BitStreamCtrlQueue */
	DEC_INFO("AUDPLAY1TASK", 14, 1, 5),  /* AudPlay1BitStreamCtrlQueue */
	DEC_INFO("AUDPLAY2TASK", 15, 2, 5),  /* AudPlay2BitStreamCtrlQueue */
#ifdef CONFIG_ARCH_MSM7X25
	DEC_INFO("AUDPLAY3TASK", 16, 3, 0),  /* AudPlay3BitStreamCtrlQueue */
	DEC_INFO("AUDPLAY4TASK", 17, 4, 0),  /* AudPlay4BitStreamCtrlQueue */
#else
	DEC_INFO("AUDPLAY3TASK", 16, 3, 4),  /* AudPlay3BitStreamCtrlQueue */
	DEC_INFO("AUDPLAY4TASK", 17, 4, 1),  /* AudPlay4BitStreamCtrlQueue */
#endif
};

static struct msm_adspdec_database msm_device_adspdec_database = {
	.num_dec = ARRAY_SIZE(dec_info_list),
	.num_concurrency_support = (ARRAY_SIZE(dec_concurrency_table) / \
					ARRAY_SIZE(dec_info_list)),
	.dec_concurrency_table = dec_concurrency_table,
	.dec_info_list = dec_info_list,
};

static struct platform_device msm_device_adspdec = {
	.name = "msm_adspdec",
	.id = -1,
	.dev    = {
		.platform_data = &msm_device_adspdec_database
	},
};

static struct android_pmem_platform_data android_pmem_kernel_ebi1_pdata = {
	.name = PMEM_KERNEL_EBI1_DATA_NAME,
	/* if no allocator_type, defaults to PMEM_ALLOCATORTYPE_BITMAP,
	 * the only valid choice at this time. The board structure is
	 * set to all zeros by the C runtime initialization and that is now
	 * the enum value of PMEM_ALLOCATORTYPE_BITMAP, now forced to 0 in
	 * include/linux/android_pmem.h.
	 */
	.cached = 0,
};

static struct android_pmem_platform_data android_pmem_pdata = {
	.name = "pmem",
	.allocator_type = PMEM_ALLOCATORTYPE_BITMAP,
	.cached = 1,
};

static struct android_pmem_platform_data android_pmem_adsp_pdata = {
	.name = "pmem_adsp",
	.allocator_type = PMEM_ALLOCATORTYPE_BITMAP,
	.cached = 0,
};

static struct platform_device android_pmem_device = {
	.name = "android_pmem",
	.id = 0,
	.dev = { .platform_data = &android_pmem_pdata },
};

static struct platform_device android_pmem_adsp_device = {
	.name = "android_pmem",
	.id = 1,
	.dev = { .platform_data = &android_pmem_adsp_pdata },
};

static struct platform_device android_pmem_kernel_ebi1_device = {
	.name = "android_pmem",
	.id = 4,
	.dev = { .platform_data = &android_pmem_kernel_ebi1_pdata },
};
static struct platform_device hs_device = {
	.name   = "msm-handset",
	.id     = -1,
	.dev    = {
		.platform_data = "7k_handset",
	},
};


#define LCDC_CONFIG_PROC          21
#define LCDC_UN_CONFIG_PROC       22
#define LCDC_API_PROG             0x30000066
#define LCDC_API_VERS             0x00010001

#define GPIO_OUT_132    132
#define GPIO_OUT_131    131
#define GPIO_OUT_103    103
#define GPIO_OUT_102    102
#define GPIO_OUT_101    101 



static struct msm_rpc_endpoint *lcdc_ep;

static int msm_fb_lcdc_config(int on)
{
	int rc = 0;
	struct rpc_request_hdr hdr;
	mutex_lock(&lcdc_config);
	if (on)
		pr_info("lcdc config\n");
	else
		pr_info("lcdc un-config\n");

	lcdc_ep = msm_rpc_connect_compatible(LCDC_API_PROG, LCDC_API_VERS, 0);
	if (IS_ERR(lcdc_ep)) {
		printk(KERN_ERR "%s: msm_rpc_connect failed! rc = %ld\n",
			__func__, PTR_ERR(lcdc_ep));
		mutex_unlock(&lcdc_config);
		return -EINVAL;
	}

	rc = msm_rpc_call(lcdc_ep,
				(on) ? LCDC_CONFIG_PROC : LCDC_UN_CONFIG_PROC,
				&hdr, sizeof(hdr),
				5 * HZ);
	if (rc)
		printk(KERN_ERR
			"%s: msm_rpc_call failed! rc = %d\n", __func__, rc);

	msm_rpc_close(lcdc_ep);
	mutex_unlock(&lcdc_config);
	return rc;
}

static int gpio_array_num_for_spi[] = 
{
    GPIO_OUT_131, /* spi_clk */
    GPIO_OUT_132, /* spi_cs  */
    GPIO_OUT_103, /* spi_sdi */
    GPIO_OUT_101, /* spi_sdoi */
    GPIO_OUT_102  /* LCD reset*/
};

static struct msm_gpio lcd_panel_gpios[] = {
	{ GPIO_CFG(98, 1, GPIO_OUTPUT,  GPIO_NO_PULL, GPIO_2MA), "lcdc_red5" },
	{ GPIO_CFG(99, 1, GPIO_OUTPUT,  GPIO_NO_PULL, GPIO_2MA), "lcdc_red6" },
	{ GPIO_CFG(100, 1, GPIO_OUTPUT,  GPIO_NO_PULL, GPIO_2MA), "lcdc_red7" },
	{ GPIO_CFG(111, 1, GPIO_OUTPUT,  GPIO_NO_PULL, GPIO_2MA), "lcdc_grn1" },
	{ GPIO_CFG(112, 1, GPIO_OUTPUT,  GPIO_NO_PULL, GPIO_2MA), "lcdc_grn0" },
	{ GPIO_CFG(113, 1, GPIO_OUTPUT,  GPIO_NO_PULL, GPIO_2MA), "lcdc_blu7" },
	{ GPIO_CFG(114, 1, GPIO_OUTPUT,  GPIO_NO_PULL, GPIO_2MA), "lcdc_blu6" },
	{ GPIO_CFG(115, 1, GPIO_OUTPUT,  GPIO_NO_PULL, GPIO_2MA), "lcdc_blu5" },
	{ GPIO_CFG(116, 1, GPIO_OUTPUT,  GPIO_NO_PULL, GPIO_2MA), "lcdc_blu4" },
	{ GPIO_CFG(117, 1, GPIO_OUTPUT,  GPIO_NO_PULL, GPIO_2MA), "lcdc_blu3" },
	{ GPIO_CFG(118, 1, GPIO_OUTPUT,  GPIO_NO_PULL, GPIO_2MA), "lcdc_blu2" },
	{ GPIO_CFG(119, 1, GPIO_OUTPUT,  GPIO_NO_PULL, GPIO_2MA), "lcdc_grn4" },
	{ GPIO_CFG(120, 1, GPIO_OUTPUT,  GPIO_NO_PULL, GPIO_2MA), "lcdc_grn3" },
	{ GPIO_CFG(121, 1, GPIO_OUTPUT,  GPIO_NO_PULL, GPIO_2MA), "lcdc_grn2" },
	{ GPIO_CFG(125, 1, GPIO_OUTPUT,  GPIO_NO_PULL, GPIO_2MA), "lcdc_blu5" },
	{ GPIO_CFG(126, 1, GPIO_OUTPUT,  GPIO_NO_PULL, GPIO_2MA), "lcdc_blu6" },
};

static void lcdc_gpio_init(void)
{
	if (gpio_request(GPIO_OUT_131, "spi_clk"))
		pr_err("failed to request gpio spi_clk\n");
	if (gpio_request(GPIO_OUT_132, "spi_cs"))
		pr_err("failed to request gpio spi_cs\n");
	if (gpio_request(GPIO_OUT_103, "spi_sdi"))
		pr_err("failed to request gpio spi_sdi\n");
	if (gpio_request(GPIO_OUT_101, "spi_sdoi"))
		pr_err("failed to request gpio spi_sdoi\n");
	if (gpio_request(GPIO_OUT_102, "gpio_dac"))
		pr_err("failed to request gpio_dac\n");
}

static uint32_t lcdc_gpio_table[] = {
	GPIO_CFG(GPIO_OUT_131, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
	GPIO_CFG(GPIO_OUT_132, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
	GPIO_CFG(GPIO_OUT_103, 0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA),
	GPIO_CFG(GPIO_OUT_101, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
	GPIO_CFG(GPIO_OUT_102, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
};

static void config_lcdc_gpio_table(uint32_t *table, int len, unsigned enable)
{
	int n, rc;
	for (n = 0; n < len; n++) {
		rc = gpio_tlmm_config(table[n],
			enable ? GPIO_ENABLE : GPIO_DISABLE);
		if (rc) {
			printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, table[n], rc);
			break;
		}
	}
}

#ifdef CONFIG_FB_MSM_LCDC_AUTO_DETECT
/* add LCD for U8150 */
static void lcdc_config_gpios(int enable)
{
	config_lcdc_gpio_table(lcdc_gpio_table,
                           ARRAY_SIZE(lcdc_gpio_table), enable);
}

static struct msm_panel_common_pdata lcdc_panel_data = 
{
	.panel_config_gpio = lcdc_config_gpios,
	.gpio_num          = gpio_array_num_for_spi,
};

static struct platform_device lcdc_ili9325_panel_device = 
{
	.name   = "lcdc_ili9325_qvga",
	.id     = 0,
	.dev    = {
		.platform_data = &lcdc_panel_data,
	}
};
static struct platform_device lcdc_ili9331b_panel_device = 
{
    .name = "lcd_ili9331b_qvga",
    .id   = 0,
    .dev  = {
        .platform_data = &lcdc_panel_data,
    }
};

static struct platform_device lcdc_s6d74a0_panel_device = 
{
	.name   = "lcdc_s6d74a0_hvga",
	.id     = 0,
	.dev    = {
		.platform_data = &lcdc_panel_data,
	}
};


static struct platform_device lcdc_spfd5408b_panel_device = 
{
	.name   = "lcdc_spfd08b_qvga",
	.id     = 0,
	.dev    = {
		.platform_data = &lcdc_panel_data,
	}
};


/*add hx8357a LCD for U8300*/
static struct platform_device lcdc_hx8357a_panel_device = 
{
	.name   = "lcdc_hx8357a_qvga",
	.id     = 0,
	.dev    = {
		.platform_data = &lcdc_panel_data,
	}
};
/*add hx8357a hvga LCD for U8500*/
static struct platform_device lcdc_hx8357a_hvga_panel_device = 
{
	.name   = "lcdc_hx8357a_hvga",
	.id     = 0,
	.dev    = {
		.platform_data = &lcdc_panel_data,
	}
};
static struct platform_device lcdc_ili9481ds_panel_device = 
{
    .name   = "lcdc_ili9481_inn",
    .id     = 0,
    .dev    = {
        .platform_data = &lcdc_panel_data,
    }
};

/* U8300 need to support the HX8368a ic driver of TRULY LCD */
static struct platform_device lcdc_hx8368a_panel_device = 
{
	.name   = "lcdc_hx8368a_qvga",
	.id     = 0,
	.dev    = {
		.platform_data = &lcdc_panel_data,
	}
};

static struct platform_device lcdc_hx8347d_panel_device = 
{
	.name   = "lcdc_hx8347d_qvga",
	.id     = 0,
	.dev    = {
		.platform_data = &lcdc_panel_data,
	}
};

static struct platform_device lcdc_ili9325c_panel_device = 
{
	.name   = "lcd_ili9325c_qvga",
	.id     = 0,
	.dev    = {
		.platform_data = &lcdc_panel_data,
	}
};

#endif


#define MSM_FB_LCDC_VREG_OP(name, op) \
do { \
	vreg = vreg_get(0, name); \
	if (vreg_##op(vreg)) \
		printk(KERN_ERR "%s: %s vreg operation failed \n", \
			(vreg_##op == vreg_enable) ? "vreg_enable" \
				: "vreg_disable", name); \
} while (0)

static void msm_fb_lcdc_power_save(int on)
{
	int rc;
	if (on)
    {
		rc = msm_gpios_enable(lcd_panel_gpios, ARRAY_SIZE(lcd_panel_gpios));
		if (rc < 0) 
        {
			printk(KERN_ERR "%s: gpio config failed: %d\n",
				__func__, rc);
		}
	} 
    else
    {
		msm_gpios_disable(lcd_panel_gpios, ARRAY_SIZE(lcd_panel_gpios));
    }

}

static struct lcdc_platform_data lcdc_pdata = {
	.lcdc_gpio_config = msm_fb_lcdc_config,
	.lcdc_power_save   = msm_fb_lcdc_power_save,
};


static struct resource msm_fb_resources[] = {
	{
		.flags  = IORESOURCE_DMA,
	}
};

static int msm_fb_detect_panel(const char *name)
{
    int ret = -EPERM;
    return ret;
}

static struct msm_fb_platform_data msm_fb_pdata = {
    .detect_client = msm_fb_detect_panel,
    .mddi_prescan = 1,
};

static struct platform_device msm_fb_device = {
	.name   = "msm_fb",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_fb_resources),
	.resource       = msm_fb_resources,
	.dev    = {
		.platform_data = &msm_fb_pdata,
	}
};

static struct platform_device rgb_leds_device = {
	.name   = "rgb-leds",
	.id     = 0,
};

#ifdef CONFIG_BT
static struct platform_device msm_bt_power_device = {
	.name = "bt_power",
};

enum {
	BT_WAKE,
	BT_RFR,
	BT_CTS,
	BT_RX,
	BT_TX,
	BT_PCM_DOUT,
	BT_PCM_DIN,
	BT_PCM_SYNC,
	BT_PCM_CLK,
	BT_HOST_WAKE,
};

static unsigned bt_config_power_on[] = {
    GPIO_CFG(42, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* WAKE */
	GPIO_CFG(43, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* RFR */
	GPIO_CFG(44, 2, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),	/* CTS */
	GPIO_CFG(45, 2, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),	/* Rx */
	GPIO_CFG(46, 3, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* Tx */
	
#ifndef CONFIG_HUAWEI_KERNEL
	/* qualcomm origin config  */
	GPIO_CFG(68, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* PCM_DOUT */
	GPIO_CFG(69, 1, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),   /* PCM_DIN */
	GPIO_CFG(70, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* PCM_SYNC */
	GPIO_CFG(71, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* PCM_CLK */
#else	
	/* huawei config  */
	GPIO_CFG(68, 1, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PCM_DOUT */
	GPIO_CFG(69, 1, GPIO_INPUT , GPIO_PULL_DOWN, GPIO_2MA), /* PCM_DIN */
	GPIO_CFG(70, 1, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PCM_SYNC */
	GPIO_CFG(71, 1, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PCM_CLK */
#endif	

    GPIO_CFG(88, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
	GPIO_CFG(83, 0, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),	/* HOST_WAKE*/	
};
static unsigned bt_config_power_off[] = {
	GPIO_CFG(42, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* wake*/
#ifndef CONFIG_HUAWEI_KERNEL
	/* qualcomm origin config  */
	GPIO_CFG(43, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* RFR */
	GPIO_CFG(44, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* CTS */
	GPIO_CFG(45, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* Rx */
	GPIO_CFG(46, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* Tx */
#else	
	/* huawei config  */
	GPIO_CFG(43, 0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA),	/* RFR */
	GPIO_CFG(44, 0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA),	/* CTS */
	GPIO_CFG(45, 0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA),	/* Rx */
	GPIO_CFG(46, 0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA),	/* Tx */
#endif	
	GPIO_CFG(68, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PCM_DOUT */
	GPIO_CFG(69, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PCM_DIN */
	GPIO_CFG(70, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PCM_SYNC */
	GPIO_CFG(71, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PCM_CLK */
    GPIO_CFG(88, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* reset */
	GPIO_CFG(83, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* HOST_WAKE */
};

static int bluetooth_power(int on)
{
	int pin, rc;

	printk(KERN_DEBUG "%s\n", __func__);

	if (on) {
		for (pin = 0; pin < ARRAY_SIZE(bt_config_power_on); pin++) {
			rc = gpio_tlmm_config(bt_config_power_on[pin],
					      GPIO_ENABLE);
			if (rc) {
				printk(KERN_ERR
				       "%s: gpio_tlmm_config(%#x)=%d\n",
				       __func__, bt_config_power_on[pin], rc);
				return -EIO;
			}
		}

            rc = gpio_direction_output(88, 1);  /*bton:88 -->1*/
            if (rc) {
			printk(KERN_ERR "%s: generation BTS4020 main clock is failed (%d)\n",
			       __func__, rc);
			return -EIO;
		}
        
	} else {
        rc = gpio_direction_output(88, 0);  /*btoff:88 -->0*/
		for (pin = 0; pin < ARRAY_SIZE(bt_config_power_off); pin++) {
			rc = gpio_tlmm_config(bt_config_power_off[pin],
					      GPIO_ENABLE);
			if (rc) {
				printk(KERN_ERR
				       "%s: gpio_tlmm_config(%#x)=%d\n",
				       __func__, bt_config_power_off[pin], rc);
				return -EIO;
			}
		}
	}
	return 0;
}

static void __init bt_power_init(void)
{
	msm_bt_power_device.dev.platform_data = &bluetooth_power;
}
#else
#define bt_power_init(x) do {} while (0)
#endif


static struct platform_device msm_device_pmic_leds = {
	.name   = "pmic-leds",
	.id = -1,
};

static struct resource bluesleep_resources[] = {
	{
		.name	= "gpio_host_wake",
		.start	= 83,
		.end	= 83,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "gpio_ext_wake",
		.start	= 42,
		.end	= 42,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "host_wake",
		.start	= MSM_GPIO_TO_INT(83),
		.end	= MSM_GPIO_TO_INT(83),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device msm_bluesleep_device = {
	.name = "bluesleep",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(bluesleep_resources),
	.resource	= bluesleep_resources,
};

static int  touch_power_init(void)
{
	struct vreg *v_gp5;
	int ret = 0;

	/* power on touchscreen */
	v_gp5 = vreg_get(NULL,"gp5");
	ret = IS_ERR(v_gp5);
	if(ret) 
	{
		printk(KERN_ERR "%s: error v_gp5 pointer is NULL! \n", __func__);
		return ret;
	}
	
	ret = vreg_set_level(v_gp5,2800);
	if (ret)
	{
		printk(KERN_ERR "%s: fail to set vreg level, err=%d \n", __func__, ret);
		return ret;
	}
	
	ret = vreg_enable(v_gp5);
	if (ret){
	   	printk(KERN_ERR "%s: fail to enable vreg gp5\n", __func__);
	   	return ret; 
	}

	return ret;
}

static struct i2c_board_info i2c_devices[] = {
#ifdef CONFIG_HUAWEI_CAMERA
#ifdef CONFIG_MT9T013
	{
		I2C_BOARD_INFO("mt9t013_liteon", 0x6C >> 1),
	},
	{
		I2C_BOARD_INFO("mt9t013_byd", 0x6C),
	},
#endif
#ifdef CONFIG_HUAWEI_CAMERA_SENSOR_OV3647
	{
		I2C_BOARD_INFO("ov3647", 0x90 >> 1),
	},
#endif //CONFIG_HUAWEI_CAMERA_SENSOR_OV3647
#ifdef CONFIG_HUAWEI_CAMERA_SENSOR_OV7690
	{
		I2C_BOARD_INFO("ov7690", 0x42 >> 1),
	},
#endif //CONFIG_HUAWEI_CAMERA_SENSOR_OV7690
#ifdef CONFIG_HUAWEI_CAMERA_SENSOR_HIMAX0356
	{
		I2C_BOARD_INFO("himax0356", 0x68 >> 1),
	},
#endif //CONFIG_HUAWEI_CAMERA_SENSOR_OV7690
#ifdef CONFIG_HUAWEI_CAMERA_SENSOR_MT9D113
	{
		I2C_BOARD_INFO("mt9d113", 0x78 >> 1),
	},
#endif //CONFIG_HUAWEI_CAMERA_SENSOR_MT9D113
#ifdef CONFIG_HUAWEI_CAMERA_SENSOR_S5K5CA
	{
		I2C_BOARD_INFO("s5k5ca", 0x5a >> 1),
	},
#endif //CONFIG_HUAWEI_CAMERA_SENSOR_S5K5CA
#ifdef CONFIG_HUAWEI_CAMERA_SENSOR_S5K4CDGX
	{
		I2C_BOARD_INFO("s5k4cdgx", 0xac >> 1),	
	},
#endif 
#endif //CONFIG_HUAWEI_CAMERA
//mzh touch
#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI_TM1319
    {
            I2C_BOARD_INFO(SYNAPTICS_I2C_RMI_NAME, 0x20),                
            .irq = MSM_GPIO_TO_INT(29)  /*gpio 20 is interupt for touchscreen.*/
    },
#endif

#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI_TM
    {
            I2C_BOARD_INFO("synaptics-tm", 0x24),                
            .irq = MSM_GPIO_TO_INT(29)  /*gpio 29 is interupt for touchscreen.*/
    },
#endif
#ifndef CONFIG_MELFAS_RESTORE_FIRMWARE
#ifdef CONFIG_ACCELEROMETER_ADXL345
    {
        I2C_BOARD_INFO("GS", 0xA6 >> 1),	  
        .irq = MSM_GPIO_TO_INT(19)    //MEMS_INT1
    },	
#endif

#ifdef CONFIG_ACCELEROMETER_ST_L1S35DE
    {
        I2C_BOARD_INFO("gs_st", 0x38 >> 1),	  
       .irq = MSM_GPIO_TO_INT(19)    //MEMS_INT1
    },	  

/* Add issue-fixed for c8600 t1 board: 
 *    issue: SDO pin of ST_L1S35DE is pulled up by hardware guy.
 *       fixed way: Change i2c addr from 0x38 to 0x3A */     
    {
        I2C_BOARD_INFO("gs_st", 0x3A >> 1),	  
       .irq = MSM_GPIO_TO_INT(19)    //MEMS_INT1
    },	

#endif
    
#ifdef CONFIG_ACCELEROMETER_MMA7455L
    {
        I2C_BOARD_INFO("freescale", 0x38 >> 1),	  
        .irq = MSM_GPIO_TO_INT(19)    //MEMS_INT1
    },
#endif	

#ifdef CONFIG_SENSORS_AKM8973
    {
        I2C_BOARD_INFO("akm8973", 0x3c >> 1),//7 bit addr, no write bit
        .irq = MSM_GPIO_TO_INT(107)
    },
#endif 
#endif
#ifdef CONFIG_MOUSE_OFN_AVAGO_A320
	{
		I2C_BOARD_INFO("avago_OFN", 0x33),
        .irq = MSM_GPIO_TO_INT(37)
	},
#endif	

//i2c address conficts with himax0356,so write fake i2c address in init phase.
#ifdef CONFIG_QWERTY_KEYPAD_ADP5587
	{
		I2C_BOARD_INFO("adp5587", 0x6a >> 1),// actual address 0x68, fake address 0x6a
		.irq = MSM_GPIO_TO_INT(39)
	},
#endif 

#ifdef CONFIG_TOUCHSCREEN_MELFAS
	{
		I2C_BOARD_INFO("melfas-ts", 0x22),                
		.irq = MSM_GPIO_TO_INT(29)  /*gpio 29 is interupt for touchscreen.*/
	},

	{
		I2C_BOARD_INFO("melfas-ts", 0x23),                
		.irq = MSM_GPIO_TO_INT(29)  /*gpio 29 is interupt for touchscreen.*/
	},
	
#endif
#ifdef CONFIG_TOUCHSCREEN_CYPRESS
	{
		I2C_BOARD_INFO("cypress-ts", 0x25),                
		.irq = MSM_GPIO_TO_INT(29)  /*gpio 29 is interupt for touchscreen.*/
	},	
#endif

#ifndef CONFIG_MELFAS_RESTORE_FIRMWARE
#ifdef CONFIG_PROXIMITY_EVERLIGHT_APS_12D
  {
    I2C_BOARD_INFO("aps-12d", 0x88 >> 1),
  },
#endif


#ifdef CONFIG_SENSORS_ST_LSM303DLH
	{
		I2C_BOARD_INFO("st303_gs", 0x32 >> 1),                
		//.irq = MSM_GPIO_TO_INT() 
	},
	{
		I2C_BOARD_INFO("st303_compass", 0x3e >> 1),/* actual i2c address is 0x3c    */             
		//.irq = MSM_GPIO_TO_INT() 
	},
#endif
#endif

};

#ifdef CONFIG_MSM_CAMERA
static uint32_t camera_off_gpio_table[] = {
	/* parallel CAMERA interfaces */
	GPIO_CFG(0,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT0 */
	GPIO_CFG(1,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT1 */
	GPIO_CFG(2,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT2 */
	GPIO_CFG(3,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT3 */
	GPIO_CFG(4,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT4 */
	GPIO_CFG(5,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT5 */
	GPIO_CFG(6,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT6 */
	GPIO_CFG(7,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT7 */
	GPIO_CFG(8,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT8 */
	GPIO_CFG(9,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT9 */
	GPIO_CFG(10, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT10 */
	GPIO_CFG(11, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT11 */
	GPIO_CFG(12, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* PCLK */
	GPIO_CFG(13, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* HSYNC_IN */
	GPIO_CFG(14, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* VSYNC_IN */
	GPIO_CFG(15, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), /* MCLK */
};

static uint32_t camera_on_gpio_table[] = {
	/* parallel CAMERA interfaces */
   GPIO_CFG(0,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT0 */
   GPIO_CFG(1,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT1 */
   GPIO_CFG(2,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT2 */
   GPIO_CFG(3,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT3 */
   GPIO_CFG(4,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT4 */
   GPIO_CFG(5,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT5 */
   GPIO_CFG(6,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT6 */
   GPIO_CFG(7,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT7 */
   GPIO_CFG(8,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT8 */
   GPIO_CFG(9,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT9 */
   GPIO_CFG(10, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT10 */
   GPIO_CFG(11, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT11 */
   GPIO_CFG(12, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_16MA), /* PCLK */
   GPIO_CFG(13, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* HSYNC_IN */
   GPIO_CFG(14, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* VSYNC_IN */
   GPIO_CFG(15, 1, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_16MA), /* MCLK */
   GPIO_CFG(92, 0, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_2MA),/*CAMIF_SHDN_INS */
   GPIO_CFG(90, 0, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_2MA),/*CAMIF_SHDN_OUTS */
   GPIO_CFG(89, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA), /* reset */
   GPIO_CFG(91, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA), /* vcm */
};

static void config_gpio_table(uint32_t *table, int len)
{
	int n, rc;
	for (n = 0; n < len; n++) {
		rc = gpio_tlmm_config(table[n], GPIO_ENABLE);
		if (rc) {
			printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, table[n], rc);
			break;
		}
	}
}

static void config_camera_on_gpios(void)
{
	config_gpio_table(camera_on_gpio_table,
		ARRAY_SIZE(camera_on_gpio_table));
}

static void config_camera_off_gpios(void)
{
	config_gpio_table(camera_off_gpio_table,
		ARRAY_SIZE(camera_off_gpio_table));
}

#ifdef CONFIG_HUAWEI_CAMERA
static int32_t sensor_vreg_enable
(
    struct msm_camera_sensor_vreg *sensor_vreg,
    uint8_t vreg_num
)

{
    struct vreg *vreg_handle;
    uint8_t temp_vreg_sum;
    int32_t rc;
    
    if(sensor_vreg == NULL)
    {
        return 0;
    }
    
    for(temp_vreg_sum = 0; temp_vreg_sum < vreg_num;temp_vreg_sum++)
    {
        vreg_handle = vreg_get(0, sensor_vreg[temp_vreg_sum].vreg_name);
    	if (!vreg_handle) {
    		printk(KERN_ERR "vreg_handle get failed\n");
    		return -EIO;
    	}
    	rc = vreg_set_level(vreg_handle, sensor_vreg[temp_vreg_sum].mv);
    	if (rc) {
    		printk(KERN_ERR "vreg_handle set level failed\n");
    		return -EIO;
    	}
    	rc = vreg_enable(vreg_handle);
    	if (rc) {
    		printk(KERN_ERR "vreg_handle enable failed\n");
    		return -EIO;
    	}
    }
    return 0;
}

static int32_t sensor_vreg_disable(
       struct msm_camera_sensor_vreg *sensor_vreg,
       uint8_t vreg_num)
{
    struct vreg *vreg_handle;
    uint8_t temp_vreg_sum;
    int32_t rc;
    
    if(sensor_vreg == NULL)
    {
        return 0;
    }

    for(temp_vreg_sum = 0; temp_vreg_sum < vreg_num;temp_vreg_sum++)
    {
    
		/*power on camera can reduce i2c error. */
        if(sensor_vreg[temp_vreg_sum].always_on)
        {
            continue;
        }
		
        vreg_handle = vreg_get(0, sensor_vreg[temp_vreg_sum].vreg_name);
    	if (!vreg_handle) {
    		printk(KERN_ERR "vreg_handle get failed\n");
    		return -EIO;
    	}
    	rc = vreg_disable(vreg_handle);
    	if (rc) {
    		printk(KERN_ERR "vreg_handle disable failed\n");
    		return -EIO;
    	}
    }
    return 0;
}
 struct msm_camera_sensor_vreg sensor_vreg_array[] = {
    {
		.vreg_name   = "gp6",
		.mv	  = 2800,
		.always_on = 1,
	}, 
    {
		.vreg_name   = "gp1",
		.mv	  = 2600,
		.always_on = 1,
	},    
    {
		.vreg_name   = "gp2",
		.mv	  = 1800,
		.always_on = 1,
	},
    {
		.vreg_name   = "boost",
		.mv	  = 5000,
		.always_on = 0,
	},     
};
static bool board_support_flash(void)
{
    if(machine_is_msm7x25_u8110() || machine_is_msm7x25_u8300() || machine_is_msm7x25_um840())
    {
        return true;
    }
    else
    {
        return false;
    }
}
#endif //CONFIG_HUAWEI_CAMERA


static struct msm_camera_device_platform_data msm_camera_device_data = {
	.camera_gpio_on  = config_camera_on_gpios,
	.camera_gpio_off = config_camera_off_gpios,
	.ioext.mdcphy = MSM_MDC_PHYS,
	.ioext.mdcsz  = MSM_MDC_SIZE,
	.ioext.appphy = MSM_CLK_CTL_PHYS,
	.ioext.appsz  = MSM_CLK_CTL_SIZE,
#ifdef CONFIG_HUAWEI_CAMERA 	
	.get_board_support_flash = board_support_flash,
#endif
};

#ifdef CONFIG_HUAWEI_CAMERA

#ifdef CONFIG_MT9T013
static struct msm_camera_sensor_info msm_camera_sensor_mt9t013_byd_data = {
	.sensor_name	= "23060032-MT-1",
	.sensor_reset	= 89,
	.sensor_pwd	= 90,
	.vcm_pwd	= 91,
	.pdata		= &msm_camera_device_data,
    .flash_type		= MSM_CAMERA_FLASH_LED,
    .sensor_module_id  = 30,
    .sensor_module_value  = 1,
    .sensor_vreg  = sensor_vreg_array,
    .vreg_num     = ARRAY_SIZE(sensor_vreg_array),
    .vreg_enable_func = sensor_vreg_enable,
    .vreg_disable_func = sensor_vreg_disable,
    .slave_sensor = 0,
    //    .master_init_control_slave = sensor_master_init_control_slave, 
};

static struct platform_device msm_camera_sensor_mt9t013_byd = {
	.name	   = "msm_camera_byd3m",
	.id        = -1,
	.dev	    = {
		.platform_data = &msm_camera_sensor_mt9t013_byd_data,
	},
};
#endif

#ifdef CONFIG_MT9T013
static struct msm_camera_sensor_info msm_camera_sensor_mt9t013_liteon_data = {
	.sensor_name	= "23060032-MT-2",
	.sensor_reset	= 89,
	.sensor_pwd	= 90,
	.vcm_pwd	= 91,
	.pdata		= &msm_camera_device_data,
    .flash_type		= MSM_CAMERA_FLASH_LED,
    .sensor_module_id  = 30,
    .sensor_module_value  = 0,
    .sensor_vreg  = sensor_vreg_array,
    .vreg_num     = ARRAY_SIZE(sensor_vreg_array),
    .vreg_enable_func = sensor_vreg_enable,
    .vreg_disable_func = sensor_vreg_disable,
    .slave_sensor = 0,
    //    .master_init_control_slave = sensor_master_init_control_slave,
};

static struct platform_device msm_camera_sensor_mt9t013_liteon = {
	.name	   = "msm_camera_liteon3m",
	.id        = -1,
	.dev	    = {
		.platform_data = &msm_camera_sensor_mt9t013_liteon_data,
	},
};
#endif

#ifdef CONFIG_HUAWEI_CAMERA_SENSOR_OV3647
static struct msm_camera_sensor_info msm_camera_sensor_ov3647_data = {
	.sensor_name	= "23060032-OV",
	.sensor_reset	= 89,
	.sensor_pwd	= 90,
	.vcm_pwd	= 91,
	.pdata		= &msm_camera_device_data,
    .flash_type		= MSM_CAMERA_FLASH_LED,

    .sensor_module_id  = 30,
    .sensor_module_value  = 0,
    .sensor_vreg  = sensor_vreg_array,
    .vreg_num     = ARRAY_SIZE(sensor_vreg_array),
    .vreg_enable_func = sensor_vreg_enable,
    .vreg_disable_func = sensor_vreg_disable,
    .slave_sensor = 0,
    //    .master_init_control_slave = sensor_master_init_control_slave,

};

static struct platform_device msm_camera_sensor_ov3647 = {
	.name	   = "msm_camera_ov3647",
	.id        = -1,
	.dev	    = {
		.platform_data = &msm_camera_sensor_ov3647_data,
	},
};
#endif
#ifdef CONFIG_HUAWEI_CAMERA_SENSOR_OV7690	
static struct msm_camera_sensor_info msm_camera_sensor_ov7690_data = {
	.sensor_name	= "23060036-OV",
	.sensor_reset	= 89,
	.sensor_pwd	= 92,
	.vcm_pwd	= 91,
	.pdata		= &msm_camera_device_data,
    .flash_type		= MSM_CAMERA_FLASH_LED,

    .sensor_module_id  = 30,
    .sensor_module_value  = 0,
    .sensor_vreg  = sensor_vreg_array,
    .vreg_num     = ARRAY_SIZE(sensor_vreg_array),
    .vreg_enable_func = sensor_vreg_enable,
    .vreg_disable_func = sensor_vreg_disable,
        .slave_sensor = 1,
    //    .master_init_control_slave = sensor_master_init_control_slave,
};

static struct platform_device msm_camera_sensor_ov7690 = {
	.name	   = "msm_camera_ov7690",
	.id        = -1,
	.dev	    = {
		.platform_data = &msm_camera_sensor_ov7690_data,
	},
};
#endif
#ifdef CONFIG_HUAWEI_CAMERA_SENSOR_HIMAX0356	
static struct msm_camera_sensor_info msm_camera_sensor_himax0356_data = {
	.sensor_name	= "23060041-Hm",
	.sensor_reset	= 89,
	.sensor_pwd	= 92,
	.vcm_pwd	= 91,
	.pdata		= &msm_camera_device_data,
    .flash_type		= MSM_CAMERA_FLASH_LED,

    .sensor_module_id  = 30,
    .sensor_module_value  = 0,
    .sensor_vreg  = sensor_vreg_array,
    .vreg_num     = ARRAY_SIZE(sensor_vreg_array),
    .vreg_enable_func = sensor_vreg_enable,
    .vreg_disable_func = sensor_vreg_disable,
    .slave_sensor = 1,
    //    .master_init_control_slave = sensor_master_init_control_slave,
};

static struct platform_device msm_camera_sensor_himax0356 = {
	.name	   = "msm_camera_himax0356",
	.id        = -1,
	.dev	    = {
		.platform_data = &msm_camera_sensor_himax0356_data,
	},
};
#endif //CONFIG_HUAWEI_CAMERA_SENSOR_HIMAX0356
#ifdef CONFIG_HUAWEI_CAMERA_SENSOR_MT9D113
static struct msm_camera_sensor_info msm_camera_sensor_mt9d113_data = {
	.sensor_name	= "23060038-MT",
	.sensor_reset	= 89,
	.sensor_pwd	= 90,
	.vcm_pwd	= 91,
	.pdata		= &msm_camera_device_data,
    .flash_type		= MSM_CAMERA_FLASH_LED,

    .sensor_module_id  = 30,
    .sensor_module_value  = 0,
    .sensor_vreg  = sensor_vreg_array,
    .vreg_num     = ARRAY_SIZE(sensor_vreg_array),
    .vreg_enable_func = sensor_vreg_enable,
    .vreg_disable_func = sensor_vreg_disable,
    .slave_sensor = 0,
    //    .master_init_control_slave = sensor_master_init_control_slave,
};

static struct platform_device msm_camera_sensor_mt9d113 = {
	.name	   = "msm_camera_mt9d113",
	.id        = -1,
	.dev	    = {
		.platform_data = &msm_camera_sensor_mt9d113_data,
	},
};
#endif
#ifdef CONFIG_HUAWEI_CAMERA_SENSOR_S5K5CA
static struct msm_camera_sensor_info msm_camera_sensor_s5k5ca_data = {
	.sensor_name	= "23060043-SAM",
	.sensor_reset	= 89,
	.sensor_pwd	= 90,
	.vcm_pwd	= 91,
	.pdata		= &msm_camera_device_data,
    .flash_type		= MSM_CAMERA_FLASH_LED,

    .sensor_module_id  = 30,
    .sensor_module_value  = 0,
    .sensor_vreg  = sensor_vreg_array,
    .vreg_num     = ARRAY_SIZE(sensor_vreg_array),
    .vreg_enable_func = sensor_vreg_enable,
    .vreg_disable_func = sensor_vreg_disable,
    .slave_sensor = 0,
    //    .master_init_control_slave = sensor_master_init_control_slave,
};

static struct platform_device msm_camera_sensor_s5k5ca = {
	.name	   = "msm_camera_s5k5ca",
	.id        = -1,
	.dev	    = {
		.platform_data = &msm_camera_sensor_s5k5ca_data,
	},
};
#endif
#ifdef CONFIG_HUAWEI_CAMERA_SENSOR_S5K4CDGX
static struct msm_camera_sensor_info msm_camera_sensor_s5k4cdgx_data = {
	.sensor_name	= "23060050-SAM-2",
	.sensor_reset	= 89,	
	.sensor_pwd	= 90,
	.vcm_pwd	= 91,
	.pdata		= &msm_camera_device_data,
    .flash_type		= MSM_CAMERA_FLASH_LED,

    .sensor_module_id  = 30,
    .sensor_module_value  = 0,
    .sensor_vreg  = sensor_vreg_array,
    .vreg_num     = ARRAY_SIZE(sensor_vreg_array),
    .vreg_enable_func = sensor_vreg_enable,
    .vreg_disable_func = sensor_vreg_disable,
    .slave_sensor = 0,
    //    .master_init_control_slave = sensor_master_init_control_slave,
};

static struct platform_device msm_camera_sensor_s5k4cdgx = {
	.name	   = "msm_camera_s5k4cdgx",
	.id        = -1,
	.dev	    = {
		.platform_data = &msm_camera_sensor_s5k4cdgx_data,
	},
};
#endif
#endif/* CONFIG_HUAWEI_CAMERA*/
#endif

static struct platform_device msm_wlan_ar6000 = {
	.name		= "wlan_ar6000",
	.id		= 1,
	.num_resources	= 0,
	.resource	= NULL,
};

static struct platform_device huawei_serial_device = {
	.name = "hw_ss_driver",
	.id		= -1,
};

static struct platform_device *devices[] __initdata = {
#ifdef CONFIG_HUAWEI_WIFI_SDCC
	&msm_wlan_ar6000,
#endif
#if !defined(CONFIG_MSM_SERIAL_DEBUGGER)
	&msm_device_uart3,
#endif
	&msm_device_smd,
	&msm_device_dmov,
	&msm_device_nand,
	&msm_device_hsusb_peripheral,
#ifdef CONFIG_USB_FUNCTION
	&mass_storage_device,
#endif
	&msm_device_i2c,
	&msm_device_tssc,
	&android_pmem_kernel_ebi1_device,
	&android_pmem_device,
	&android_pmem_adsp_device,
	&msm_fb_device,
#ifdef CONFIG_FB_MSM_LCDC_AUTO_DETECT
    &lcdc_ili9325_panel_device,
    &lcdc_ili9331b_panel_device,
    &lcdc_s6d74a0_panel_device,
    &lcdc_spfd5408b_panel_device,
    &lcdc_hx8357a_panel_device,
/* U8300 need to support the HX8368a ic driver of TRULY LCD */
    &lcdc_hx8368a_panel_device,
    &lcdc_hx8347d_panel_device,
    &lcdc_ili9325c_panel_device,
    &lcdc_hx8357a_hvga_panel_device,
    &lcdc_ili9481ds_panel_device,
#endif
	&msm_device_uart_dm1,
#ifdef CONFIG_BT
	&msm_bt_power_device,
#endif
	&msm_device_pmic_leds,
	&msm_device_snd,
	&msm_device_adspdec,
#ifdef CONFIG_HUAWEI_CAMERA
#ifdef CONFIG_HUAWEI_CAMERA_SENSOR_OV7690
	&msm_camera_sensor_ov7690,
#endif
#ifdef CONFIG_HUAWEI_CAMERA_SENSOR_HIMAX0356	
	&msm_camera_sensor_himax0356,
#endif //CONFIG_HUAWEI_CAMERA_SENSOR_HIMAX0356
#ifdef CONFIG_MT9T013
	&msm_camera_sensor_mt9t013_byd,
#endif
#ifdef CONFIG_MT9T013
	&msm_camera_sensor_mt9t013_liteon,
#endif
#ifdef CONFIG_HUAWEI_CAMERA_SENSOR_OV3647
	&msm_camera_sensor_ov3647,
#endif
#ifdef CONFIG_HUAWEI_CAMERA_SENSOR_MT9D113	
        &msm_camera_sensor_mt9d113,
#endif
#ifdef CONFIG_HUAWEI_CAMERA_SENSOR_S5K5CA
        &msm_camera_sensor_s5k5ca,
#endif		
#ifdef CONFIG_HUAWEI_CAMERA_SENSOR_S5K4CDGX
        &msm_camera_sensor_s5k4cdgx,
#endif		
#endif
	&msm_bluesleep_device,
	&hs_device,

//add by mzh
#ifdef CONFIG_HUAWEI_BATTERY

	&huawei_battery_device,
#endif
	&huawei_serial_device,
#ifdef CONFIG_HUAWEI_RGB_KEY_LIGHT
	&rgb_leds_device,
#endif
};

static struct msm_panel_common_pdata mdp_pdata = {
	.gpio = 97,
};

static void __init msm_fb_add_devices(void)
{
	msm_fb_register_device("mdp", &mdp_pdata);
	msm_fb_register_device("pmdh", 0);
	msm_fb_register_device("lcdc", &lcdc_pdata);
}

extern struct sys_timer msm_timer;

static void __init msm7x2x_init_irq(void)
{
	msm_init_irq();
}

/* Turbo Mode, AXI bus work at 160M */
static struct msm_acpu_clock_platform_data msm7x2x_clock_data = {
	.acpu_switch_time_us = 50,
	.max_speed_delta_khz = 256000,
	.vdd_switch_time_us = 62,
	.power_collapse_khz = 19200000,
	.wait_for_irq_khz = 128000000,
	.max_axi_khz = 160000,
};

void msm_serial_debug_init(unsigned int base, int irq,
			   struct device *clk_device, int signal_irq);

#ifdef CONFIG_MMC
static void sdcc_gpio_init(void)
{
#ifdef CONFIG_HUAWEI_WIFI_SDCC
	int rc = 0;	
	unsigned gpio_pin = 124;

	board_support_bcm_wifi(&gpio_pin);
	
	if (gpio_request(gpio_pin, "wifi_wow_irq"))		
		printk("failed to request gpio wifi_wow_irq\n");	

	rc = gpio_tlmm_config(GPIO_CFG((gpio_pin), 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), GPIO_ENABLE);	

	if (rc)		
		printk(KERN_ERR "%s: Failed to configure GPIO[%d] = %d\n",__func__, gpio_pin, rc);
#endif
	/* SDC1 GPIOs */
#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
	if (gpio_request(51, "sdc1_data_3"))
		pr_err("failed to request gpio sdc1_data_3\n");
	if (gpio_request(52, "sdc1_data_2"))
		pr_err("failed to request gpio sdc1_data_2\n");
	if (gpio_request(53, "sdc1_data_1"))
		pr_err("failed to request gpio sdc1_data_1\n");
	if (gpio_request(54, "sdc1_data_0"))
		pr_err("failed to request gpio sdc1_data_0\n");
	if (gpio_request(55, "sdc1_cmd"))
		pr_err("failed to request gpio sdc1_cmd\n");
	if (gpio_request(56, "sdc1_clk"))
		pr_err("failed to request gpio sdc1_clk\n");
#endif

	/* SDC2 GPIOs */
#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
	if (gpio_request(62, "sdc2_clk"))
		pr_err("failed to request gpio sdc2_clk\n");
	if (gpio_request(63, "sdc2_cmd"))
		pr_err("failed to request gpio sdc2_cmd\n");
	if (gpio_request(64, "sdc2_data_3"))
		pr_err("failed to request gpio sdc2_data_3\n");
	if (gpio_request(65, "sdc2_data_2"))
		pr_err("failed to request gpio sdc2_data_2\n");
	if (gpio_request(66, "sdc2_data_1"))
		pr_err("failed to request gpio sdc2_data_1\n");
	if (gpio_request(67, "sdc2_data_0"))
		pr_err("failed to request gpio sdc2_data_0\n");
#endif
}

static unsigned sdcc_cfg_data[][6] = {
	/* SDC1 configs */
	{
	GPIO_CFG(51, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(52, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(53, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(54, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(55, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(56, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
	},
	/* SDC2 configs */
	{
	GPIO_CFG(62, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
	GPIO_CFG(63, 2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(64, 2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(65, 2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(66, 2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(67, 2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	},
};

static unsigned long vreg_sts, gpio_sts;
static struct vreg *vreg_mmc;

static void msm_sdcc_setup_gpio(int dev_id, unsigned int enable)
{
	int i, rc;

	if (!(test_bit(dev_id, &gpio_sts)^enable))
		return;

	if (enable)
		set_bit(dev_id, &gpio_sts);
	else
		clear_bit(dev_id, &gpio_sts);

	for (i = 0; i < ARRAY_SIZE(sdcc_cfg_data[dev_id - 1]); i++) {
		rc = gpio_tlmm_config(sdcc_cfg_data[dev_id - 1][i],
			enable ? GPIO_ENABLE : GPIO_DISABLE);
		if (rc)
			printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, sdcc_cfg_data[dev_id - 1][i], rc);
	}
}

static uint32_t msm_sdcc_setup_power(struct device *dv, unsigned int vdd)
{
	int rc = 0;
	struct platform_device *pdev;

	pdev = container_of(dv, struct platform_device, dev);
	msm_sdcc_setup_gpio(pdev->id, !!vdd);

	if (vdd == 0) {
		if (!vreg_sts)
			return 0;

		clear_bit(pdev->id, &vreg_sts);

		if (!vreg_sts) {
            //rc = vreg_disable(vreg_mmc);
			if (rc)
				printk(KERN_ERR "%s: return val: %d \n",
                       __func__, rc);
		}
		return 0;
	}

	if (!vreg_sts) {
        /*wifi 上电不在此处处理,如SD卡需要在此处上电,请修改vreg_mmc*/
        //rc = vreg_set_level(vreg_mmc, 2850);
        if (!rc)
            //rc = vreg_enable(vreg_mmc);
        if (rc)
            printk(KERN_ERR "%s: return val: %d \n",
                   __func__, rc);
    }
	set_bit(pdev->id, &vreg_sts);
	return 0;
}

static struct mmc_platform_data msm7x2x_sdcc_data = {
	.ocr_mask	= MMC_VDD_28_29,
	.translate_vdd	= msm_sdcc_setup_power,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,  
};

#ifdef CONFIG_HUAWEI_WIFI_SDCC 
static uint32_t msm_sdcc_setup_power_wifi(struct device *dv,
                                          unsigned int vdd)
{
	struct platform_device *pdev;
    
	pdev = container_of(dv, struct platform_device, dev);
	msm_sdcc_setup_gpio(pdev->id, !!vdd);
    
	return 0;
}

static struct mmc_platform_data msm7x2x_sdcc_data_wifi = {
	.ocr_mask	= MMC_VDD_28_29,
	.translate_vdd	= msm_sdcc_setup_power_wifi
};
#endif 

#if CONFIG_HUAWEI_WIFI_SDCC
#define PREALLOC_WLAN_NUMBER_OF_SECTIONS	4
#define PREALLOC_WLAN_NUMBER_OF_BUFFERS		160
#define PREALLOC_WLAN_SECTION_HEADER		24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 1024)

#define WLAN_SKB_BUF_NUM	16

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

typedef struct wifi_mem_prealloc_struct {
	void *mem_ptr;
	unsigned long size;
} wifi_mem_prealloc_t;

static wifi_mem_prealloc_t wifi_mem_array[PREALLOC_WLAN_NUMBER_OF_SECTIONS] = {
	{ NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER) }
};

static void *bcm_wifi_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_NUMBER_OF_SECTIONS)
		return wlan_static_skb;
	if ((section < 0) || (section > PREALLOC_WLAN_NUMBER_OF_SECTIONS))
		return NULL;
	if (wifi_mem_array[section].size < size)
		return NULL;
	return wifi_mem_array[section].mem_ptr;
}

int __init bcm_init_wifi_mem(void)
{
	int i;

	for(i=0;( i < WLAN_SKB_BUF_NUM );i++) {
		if (i < (WLAN_SKB_BUF_NUM/2))
			wlan_static_skb[i] = dev_alloc_skb(4096); //malloc skb 4k buffer
		else
			wlan_static_skb[i] = dev_alloc_skb(8192); //malloc skb 8k buffer
	}
	for(i=0;( i < PREALLOC_WLAN_NUMBER_OF_SECTIONS );i++) {
		wifi_mem_array[i].mem_ptr = kmalloc(wifi_mem_array[i].size,
							GFP_KERNEL);
		if (wifi_mem_array[i].mem_ptr == NULL)
			return -ENOMEM;
	}
	
	printk("bcm_init_wifi_mem kmalloc wifi_mem_array successfully \n");
	return 0;
}

struct wifi_platform_data {     
void *(*mem_prealloc)(int section, unsigned long size);
};

static struct wifi_platform_data bcm_wifi_control = {
	.mem_prealloc	= bcm_wifi_mem_prealloc,
};

static struct platform_device bcm_wifi_device = {
        /* bcm4319_wlan device */
        .name           = "bcm4319_wlan",
        .id             = 1,
        .num_resources  = 0,
        .resource       = NULL,
        .dev            = {
                .platform_data = &bcm_wifi_control,
        },
};

#endif

static void __init msm7x2x_init_mmc(void)
{
    vreg_mmc = vreg_get(NULL, "mmc");
    if (IS_ERR(vreg_mmc)) {
        printk(KERN_ERR "%s: vreg get failed (%ld)\n",
               __func__, PTR_ERR(vreg_mmc));
        return;
    }
	sdcc_gpio_init();
#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
//add by mzh for update baseline begin
	//msm_add_sdcc(1, &msm7x2x_sdcc_data);
	msm_add_sdcc(1, &msm7x2x_sdcc_data, 0, 0);
//add by mzh for update baseline end
#endif
#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
#ifdef CONFIG_HUAWEI_WIFI_SDCC 
//add by mzh for update baseline begin
	//msm_add_sdcc(2, &msm7x2x_sdcc_data_wifi);
	msm_add_sdcc(2, &msm7x2x_sdcc_data_wifi, 0, 0);
//add by mzh for update baseline end

#endif 
#endif

#ifdef CONFIG_HUAWEI_WIFI_SDCC
	bcm_init_wifi_mem();
	platform_device_register(&bcm_wifi_device);
#endif

}
#else
#define msm7x2x_init_mmc() do {} while (0)
#endif


static struct msm_pm_platform_data msm7x25_pm_data[MSM_PM_SLEEP_MODE_NR] = {
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].latency = 16000,

	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN].latency = 12000,

	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].latency = 2000,
};

#if 0
#ifdef CONFIG_HUAWEI_U8220_JOGBALL
static unsigned jogball_gpio_table[] = {
        GPIO_CFG(38, 0, GPIO_OUTPUT,  GPIO_PULL_UP, GPIO_2MA),
        GPIO_CFG(31, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA),	
        GPIO_CFG(32, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA),	
        GPIO_CFG(36, 0, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),	       
        GPIO_CFG(37, 0, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),	  
	
};

static void config_jogball_gpios(void)
{
	config_gpio_table(jogball_gpio_table,
		ARRAY_SIZE(jogball_gpio_table));
    
}
#endif
#endif

static void
msm_i2c_gpio_config(int iface, int config_type)
{
	int gpio_scl;
	int gpio_sda;
	if (iface) {
		gpio_scl = 95;
		gpio_sda = 96;
	} else {
		gpio_scl = 60;
		gpio_sda = 61;
	}
	if (config_type) {
		gpio_tlmm_config(GPIO_CFG(gpio_scl, 1, GPIO_INPUT,
					GPIO_NO_PULL, GPIO_16MA), GPIO_ENABLE);
		gpio_tlmm_config(GPIO_CFG(gpio_sda, 1, GPIO_INPUT,
					GPIO_NO_PULL, GPIO_16MA), GPIO_ENABLE);
	} else {
		gpio_tlmm_config(GPIO_CFG(gpio_scl, 0, GPIO_OUTPUT,
					GPIO_NO_PULL, GPIO_16MA), GPIO_ENABLE);
		gpio_tlmm_config(GPIO_CFG(gpio_sda, 0, GPIO_OUTPUT,
					GPIO_NO_PULL, GPIO_16MA), GPIO_ENABLE);
	}
}

static struct msm_i2c_platform_data msm_i2c_pdata = {
	.clk_freq = 100000,
	.rmutex = NULL,
	.pri_clk = 60,
	.pri_dat = 61,
	.msm_i2c_config_gpio = msm_i2c_gpio_config,
};

static void __init msm_device_i2c_init(void)
{
	if (gpio_request(60, "i2c_pri_clk"))
		pr_err("failed to request gpio i2c_pri_clk\n");
	if (gpio_request(61, "i2c_pri_dat"))
		pr_err("failed to request gpio i2c_pri_dat\n");

    msm_i2c_pdata.pm_lat =
		msm7x25_pm_data[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN]
		.latency;
    
    msm_device_i2c.dev.platform_data = &msm_i2c_pdata;
}

#ifdef CONFIG_USB_AUTO_INSTALL
/* provide a method to map pid_index to usb_pid, 
 * pid_index is kept in NV(4526). 
 * At power up, pid_index is read in modem and transfer to app in share memory.
 * pid_index can be modified through write file fixusb(msm_hsusb_store_fixusb).
*/
u16 pid_index_to_pid(u32 pid_index)
{
    u16 usb_pid = 0xFFFF;
    
    switch(pid_index)
    {
        case CDROM_INDEX:
            usb_pid = curr_usb_pid_ptr->cdrom_pid;
            break;
        case NORM_INDEX:
            usb_pid = curr_usb_pid_ptr->norm_pid;
            break;
        case AUTH_INDEX:
            usb_pid = curr_usb_pid_ptr->auth_pid;
            break;
            
        /* set the USB pid to multiport when the index is 0
           This is happened when the NV is not set or set 
           to zero 
        */
        case ORI_INDEX:
        default:
            usb_pid = curr_usb_pid_ptr->norm_pid;
            break;
    }

    USB_PR("%s, pid_index=%d, usb_pid=0x%x\n", __func__, pid_index, usb_pid);
    
    return usb_pid;
}

/*  
 * Get usb parameter from share memory and set usb serial number accordingly.
 */
static void proc_usb_para(void)
{
    smem_huawei_vender *usb_para_ptr;
    //u16 pid;
    char *vender_name="t-mobile";

    USB_PR("< %s\n", __func__);

    /* initialize */
    usb_para_info.usb_pid_index = 0;
    usb_para_info.usb_pid = PID_NORMAL;
    
    /* now the smem_id_vendor0 smem id is a new struct */
    usb_para_ptr = (smem_huawei_vender*)smem_alloc(SMEM_ID_VENDOR0, sizeof(smem_huawei_vender));
    if (!usb_para_ptr)
    {
    	USB_PR("%s: Can't find usb parameter\n", __func__);
        return;
    }

    USB_PR("vendor:%s,country:%s\n", usb_para_ptr->vender_para.vender_name, usb_para_ptr->vender_para.country_name);

    memcpy(&usb_para_data, usb_para_ptr, sizeof(smem_huawei_vender));
    
    /* decide usb pid array according to the vender name */
    if(!memcmp(usb_para_ptr->vender_para.vender_name, vender_name, strlen(vender_name)))
    {
        curr_usb_pid_ptr = &usb_pid_array[1];
        USB_PR("USB setting is TMO\n");
    }
    else
    {
        curr_usb_pid_ptr = &usb_pid_array[0];
        USB_PR("USB setting is NORMAL\n");
    }

    USB_PR("smem usb_serial=%s, usb_pid_index=%d\n", usb_para_ptr->usb_para.usb_serial, usb_para_ptr->usb_para.usb_pid_index);

    usb_para_info.usb_pid_index = usb_para_ptr->usb_para.usb_pid_index;
    usb_para_info.usb_pid = pid_index_to_pid(usb_para_ptr->usb_para.usb_pid_index);
    
    USB_PR("curr_usb_pid_ptr: 0x%x, 0x%x, 0x%x, 0x%x\n", 
        curr_usb_pid_ptr->cdrom_pid, 
        curr_usb_pid_ptr->norm_pid, 
        curr_usb_pid_ptr->udisk_pid,
        curr_usb_pid_ptr->auth_pid);
    USB_PR("usb_para_info: usb_pid_index=%d, usb_pid = 0x%x>\n", 
        usb_para_info.usb_pid_index, 
        usb_para_info.usb_pid);

}

/* set usb serial number */
void set_usb_sn(char *sn_ptr)
{
    if(sn_ptr == NULL)
    {
        ((struct msm_hsusb_platform_data *)(msm_device_hsusb_peripheral.dev.platform_data))->serial_number = NULL;
        //msm_hsusb_pdata.serial_number = NULL;
        USB_PR("set USB SN to NULL\n");
    }
    else
    {
        memcpy(usb_serial_num, sn_ptr, strlen(sn_ptr));
        ((struct msm_hsusb_platform_data *)(msm_device_hsusb_peripheral.dev.platform_data))->serial_number = (char *)usb_serial_num;
        //msm_hsusb_pdata.serial_number = (char *)usb_serial_num;
        USB_PR("set USB SN to %s\n", usb_serial_num);

    }
}
#endif

static void __init msm7x2x_init(void)
{
	if (socinfo_init() < 0)
		BUG();

#ifdef CONFIG_HUAWEI_CAMERA
    sensor_vreg_disable(sensor_vreg_array,ARRAY_SIZE(sensor_vreg_array));
#endif

#if defined(CONFIG_MSM_SERIAL_DEBUGGER)
	msm_serial_debug_init(MSM_UART3_PHYS, INT_UART3,
                          &msm_device_uart3.dev, 1);
#endif

	msm_acpu_clock_init(&msm7x2x_clock_data);

	msm_hsusb_pdata.swfi_latency =
		msm7x25_pm_data
		[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].latency;

#ifdef CONFIG_USB_AUTO_INSTALL
    proc_usb_para();
#endif  /* #ifdef CONFIG_USB_AUTO_INSTALL */

    if(&usb_pid_array[1] == curr_usb_pid_ptr)
    {
        msm_device_hsusb_peripheral.dev.platform_data = &msm_hsusb_tmo_pdata;
        mass_storage_device.dev.platform_data = &usb_mass_storage_tmo_pdata;
    }
    else
    {
        msm_device_hsusb_peripheral.dev.platform_data = &msm_hsusb_pdata;
    }

    if(NORM_INDEX == usb_para_info.usb_pid_index)
    {
        set_usb_sn(USB_SN_STRING);
    }


	platform_add_devices(devices, ARRAY_SIZE(devices));
#ifdef CONFIG_MSM_CAMERA
	config_camera_off_gpios(); /* might not be necessary */
#endif
	(void)touch_power_init();	
	msm_device_i2c_init();
	i2c_register_board_info(0, i2c_devices, ARRAY_SIZE(i2c_devices));

#ifdef CONFIG_SURF_FFA_GPIO_KEYPAD
	if (machine_is_msm7x25_ffa() || machine_is_msm7x27_ffa())
		platform_device_register(&keypad_device_7k_ffa);
	else
		platform_device_register(&keypad_device_surf);
#endif

#ifdef CONFIG_HUAWEI_GPIO_KEYPAD
	if (machine_is_msm7x25_c8600() || machine_is_msm7x25_m860())
		platform_device_register(&keypad_device_c8600);
	else if ( machine_is_msm7x25_u8110() || machine_is_msm7x25_u8100()  \
             || machine_is_msm7x25_u8105() || machine_is_msm7x25_u8107() \
             || machine_is_msm7x25_u8109() )
		platform_device_register(&keypad_device_u8100);
	else if (machine_is_msm7x25_u7610())
		platform_device_register(&keypad_device_u7610);
    else if (machine_is_msm7x25_u8120())
		platform_device_register(&keypad_device_u8120);
	else if (machine_is_msm7x25_u8300())
	    platform_device_register(&keypad_device_u8300);
	else if ( machine_is_msm7x25_u8150() || machine_is_msm7x25_c8150()  \
             || machine_is_msm7x25_c8500() )
	    platform_device_register(&keypad_device_u8150);
	else if (machine_is_msm7x25_u8500() || machine_is_msm7x25_um840())
	    platform_device_register(&keypad_device_u8500);
	else
		platform_device_register(&keypad_device_c8600);  //default use c6800 keypad
#endif
	lcdc_gpio_init();
#ifdef CONFIG_HUAWEI_JOGBALL
	//config_jogball_gpios();
	//init_jogball();
	if (machine_is_msm7x25_c8600() || machine_is_msm7x25_m860())
	{
	    platform_device_register(&jogball_device);
	}
	else if(machine_is_msm7x25_u8300())
	{
	    if(HW_VER_SUB_SURF != get_hw_sub_board_id())
	    {
	        platform_device_register(&jogball_device_u8300);
	    }
	    else
	    {
	        platform_device_register(&jogball_device);	
	    }
	}
#endif 

	msm_fb_add_devices();
	msm7x2x_init_mmc();
	bt_power_init();
    msm_pm_set_platform_data(msm7x25_pm_data);
#ifdef CONFIG_HUAWEI_MSM_VIBRATOR
	init_vibrator_device();
#endif
#ifdef CONFIG_HUAWEI_FEATURE_OFN_KEY
    init_ofn_ok_key_device();
#endif

}

static unsigned pmem_kernel_ebi1_size = PMEM_KERNEL_EBI1_SIZE;
static void __init pmem_kernel_ebi1_size_setup(char **p)
{
	pmem_kernel_ebi1_size = memparse(*p, p);
}
__early_param("pmem_kernel_ebi1_size=", pmem_kernel_ebi1_size_setup);

static unsigned pmem_mdp_size = MSM_PMEM_MDP_SIZE;
static void __init pmem_mdp_size_setup(char **p)
{
	pmem_mdp_size = memparse(*p, p);
}
__early_param("pmem_mdp_size=", pmem_mdp_size_setup);

static unsigned pmem_adsp_size = MSM_PMEM_ADSP_SIZE;
static void __init pmem_adsp_size_setup(char **p)
{
	pmem_adsp_size = memparse(*p, p);
}
__early_param("pmem_adsp_size=", pmem_adsp_size_setup);

static unsigned fb_size = MSM_FB_SIZE;
static void __init fb_size_setup(char **p)
{
	fb_size = memparse(*p, p);
}
__early_param("fb_size=", fb_size_setup);

static void __init msm_msm7x2x_allocate_memory_regions(void)
{
	void *addr;
	unsigned long size;

	size = pmem_mdp_size;
	if (size) {
		addr = alloc_bootmem(size);
		android_pmem_pdata.start = __pa(addr);
		android_pmem_pdata.size = size;
		pr_info("allocating %lu bytes at %p (%lx physical) for mdp "
                "pmem arena\n", size, addr, __pa(addr));
	}

	size = pmem_adsp_size;
	if (size) {
		addr = alloc_bootmem(size);
		android_pmem_adsp_pdata.start = __pa(addr);
		android_pmem_adsp_pdata.size = size;
		pr_info("allocating %lu bytes at %p (%lx physical) for adsp "
                "pmem arena\n", size, addr, __pa(addr));
	}

	size = fb_size ? : MSM_FB_SIZE;
    /*set the frame buffer same as oemsbl frame buffer*/
	msm_fb_resources[0].start =(resource_size_t)0x0D500000;
	msm_fb_resources[0].end = msm_fb_resources[0].start + size - 1;

	size = pmem_kernel_ebi1_size;
	if (size) {
		addr = alloc_bootmem_aligned(size, 0x100000);
		android_pmem_kernel_ebi1_pdata.start = __pa(addr);
		android_pmem_kernel_ebi1_pdata.size = size;
		pr_info("allocating %lu bytes at %p (%lx physical) for kernel"
			" ebi1 pmem arena\n", size, addr, __pa(addr));
	}
}
static void __init msm7x2x_fixup(struct machine_desc *desc,
                                 struct tag *tags,
                                 char **cmdline,
                                 struct meminfo *mi)
{
    camera_id = parse_tag_camera_id((const struct tag *)tags);
    printk("%s:camera_id=%d\n", __func__, camera_id);
        
    lcd_id = parse_tag_lcd_id((const struct tag *)tags);
    printk("%s:lcd_id=%d\n", __func__, lcd_id);

    ts_id = parse_tag_ts_id((const struct tag *)tags);
    printk("%s:ts_id=%d\n", __func__, ts_id);

    sub_board_id = parse_tag_sub_board_id((const struct tag *)tags);
    printk("%s:sub_board_id=%d\n", __func__, sub_board_id);

#ifdef CONFIG_USB_AUTO_INSTALL
    /* get the boot mode transfered from APPSBL */
    usb_boot_mode = parse_tag_boot_mode_id((const struct tag *)tags);
    USB_PR("%s,usb_boot_mode=0x%x\n", __func__, usb_boot_mode);
#endif
}

static void __init msm7x2x_map_io(void)
{
	msm_map_common_io();
	/* Technically dependent on the SoC but using machine_is
	 * macros since socinfo is not available this early and there
	 * are plans to restructure the code which will eliminate the
	 * need for socinfo.
	 */
    msm_clock_init(msm_clocks_7x25, msm_num_clocks_7x25);
	msm_msm7x2x_allocate_memory_regions();
}

/* =============================================================================
                    hardware  self adapt

   =============================================================================*/
lcd_panel_type lcd_panel_probe(void)
{
    lcd_panel_type hw_lcd_panel = LCD_NONE;

/* add U8100 series product  */
    if( machine_is_msm7x25_u8120()                                  \
        || machine_is_msm7x25_u8110() || machine_is_msm7x25_u8100() \
        || machine_is_msm7x25_u8105() || machine_is_msm7x25_u8107() \
        || machine_is_msm7x25_u8109() )
    {
        switch (lcd_id)
        {
            case 0: 
                hw_lcd_panel = LCD_SPFD5408B_KGM_QVGA;
                break;
            case 1: 
                hw_lcd_panel = LCD_ILI9325_INNOLUX_QVGA;
                break;
            case 2: 
                hw_lcd_panel = LCD_ILI9325_BYD_QVGA;
                break;
            case 3: 
                hw_lcd_panel = LCD_ILI9325_WINTEK_QVGA;
                break;
            default : 
                hw_lcd_panel = LCD_ILI9325_INNOLUX_QVGA;
                break;
                
        }

    }
    else if(machine_is_msm7x25_c8600() )
    {
        switch(lcd_id)
        {
            case 0: 
                hw_lcd_panel = LCD_S6D74A0_SAMSUNG_HVGA;
                break;
            case 1:
                hw_lcd_panel = LCD_ILI9481DS_TIANMA_HVGA;
                break;
            case 2:  
                hw_lcd_panel = LCD_ILI9481D_INNOLUX_HVGA;
                break;
                                
            default : 
                hw_lcd_panel = LCD_NONE;
                break;                    
        }

    }
    else if(machine_is_msm7x25_m860())
    {
        switch (lcd_id)
        {
            case 0: 
                hw_lcd_panel = LCD_S6D74A0_SAMSUNG_HVGA;
                break;
            case 1:
                hw_lcd_panel = LCD_ILI9481DS_TIANMA_HVGA;
                break;
            case 2:  
                hw_lcd_panel = LCD_ILI9481D_INNOLUX_HVGA;
                break;
            default : 
                hw_lcd_panel = LCD_NONE;
                break;                    
        }

    }
    else if(machine_is_msm7x25_u7610())
    {


    }

    else if(machine_is_msm7x25_u8300())
    {
        switch (lcd_id)
        {
            case 0: 
    /* U8300 need to support the HX8368a ic driver of TRULY LCD */
                hw_lcd_panel = LCD_HX8368A_TRULY_QVGA;
                break;
            case 1: 
                hw_lcd_panel = LCD_HX8368A_SEIKO_QVGA;
                break;
            case 2: 
                if(get_hw_sub_board_id() == HW_VER_SUB_SURF)  /*u8300 surf */
                {
                    hw_lcd_panel = LCD_ILI9325_BYD_QVGA;  /*for U8110 2.8' LCD*/
                }
                else
                {
                    hw_lcd_panel = LCD_NONE;
                }
                break;
            default : 
    /* U8300 need to surport the HX8368a ic driver of TRULY LCD */
                hw_lcd_panel = LCD_HX8368A_TRULY_QVGA;
                break;  
        }
    }
/* add LCD for U8150 */
    else if ( machine_is_msm7x25_u8150() || machine_is_msm7x25_c8150()  \
             || machine_is_msm7x25_c8500() )
    {
        switch (lcd_id)
        {
            case 0:
                hw_lcd_panel = LCD_HX8347D_TRULY_QVGA;
                break;
            case 1:
                hw_lcd_panel = LCD_ILI9325C_WINTEK_QVGA;
                break;
            case 2:
                hw_lcd_panel = LCD_ILI9331B_TIANMA_QVGA;
                break;
            case 3:
                hw_lcd_panel = LCD_HX8347D_INNOLUX_QVGA;
                break;
            default: 
                hw_lcd_panel = LCD_ILI9325C_WINTEK_QVGA;
                break;
        }
    }
/*add LCD for U8500*/
    else if(machine_is_msm7x25_u8500() || machine_is_msm7x25_um840())
    {
        switch (lcd_id)
        {
            case 0:
                hw_lcd_panel = LCD_HX8357A_WINTEK_HVGA;
                break;
            case 1:
                hw_lcd_panel = LCD_HX8357A_TRULY_HVGA;
                break;
            default: 
                hw_lcd_panel = LCD_HX8357A_WINTEK_HVGA;
                break;
        }
    }
    else
    {
        /*if no lcd panel installed, make innolux LCD panel as the default panel */
        hw_lcd_panel = LCD_ILI9325_INNOLUX_QVGA;
    }
    
    printk(KERN_ERR "lcd_panel:*********hw_lcd_panel == %d;***************\n", hw_lcd_panel);

    return hw_lcd_panel;
}

/*===========================================================================


FUNCTION     lcd_align_probe

DESCRIPTION
  This function probe which LCD align type should be used

DEPENDENCIES
  
RETURN VALUE
  None

SIDE EFFECTS
  None
===========================================================================*/
lcd_align_type lcd_align_probe(void)
{
    lcd_panel_type  hw_lcd_panel = LCD_NONE;
    lcd_align_type  lcd_align    =  LCD_PANEL_ALIGN_LSB;
     
    if(machine_is_msm7x25_m860()||machine_is_msm7x25_c8600())
    {               
        hw_lcd_panel = lcd_panel_probe();
        if ((hw_lcd_panel == LCD_ILI9481DS_TIANMA_HVGA) ||(hw_lcd_panel == LCD_ILI9481D_INNOLUX_HVGA))
        {
            lcd_align = LCD_PANEL_ALIGN_MSB;
        }
        else
        {
            lcd_align = LCD_PANEL_ALIGN_LSB;
        }
    }
    return lcd_align;

}

char *get_lcd_panel_name(void)
{
    lcd_panel_type hw_lcd_panel = LCD_NONE;
    char *pname = NULL;
    
    hw_lcd_panel = lcd_panel_probe();
    
    switch (hw_lcd_panel)
    {
        case LCD_S6D74A0_SAMSUNG_HVGA:
            pname = "SAMSUNG S6D74A0";
            break;
        case LCD_ILI9481D_INNOLUX_HVGA:
            pname = "INNOLUX ILI9481D";
            break;
        case LCD_ILI9481DS_TIANMA_HVGA:
            pname = "TIANMA ILI9481DS";
            break;
            
        case LCD_ILI9325_INNOLUX_QVGA:
            pname = "INNOLUX ILI9325";
            break;
        case LCD_ILI9331B_TIANMA_QVGA:
            pname = "TIANMA ILI9331B";
            break;
        case LCD_ILI9325_BYD_QVGA:
            pname = "BYD ILI9325";
            break;

        case LCD_ILI9325_WINTEK_QVGA:
            pname = "WINTEK ILI9325";
            break;

        case LCD_SPFD5408B_KGM_QVGA:
            pname = "KGM SPFD5408B";
            break;

/* U8300 need to support the HX8368a ic driver of TRULY LCD */
        case LCD_HX8368A_TRULY_QVGA:
            pname = "TRULY HX8368A";
            break;

        case LCD_HX8368A_SEIKO_QVGA:
            pname = "SEIKO HX8368A";
            break;
        case LCD_HX8347D_TRULY_QVGA:
            pname = "TRULY HX8347D";
            break;
        case LCD_HX8347D_INNOLUX_QVGA:
            pname = "INNOLUX HX8347D";
            break;
        case LCD_ILI9325C_WINTEK_QVGA:
            pname = "WINTEK ILI9325C";
            break;
        case LCD_HX8357A_TRULY_HVGA:
            pname = "TRULY HX8357A";
            break;
        case LCD_HX8357A_WINTEK_HVGA:
            pname = "WIMTEK HX8357A";
            break;
        default:
            pname = "UNKNOWN LCD";
            break;
    }

    return pname;
}

int board_surport_fingers(bool * is_surport_fingers)
{
    int result = 0;

    if (is_surport_fingers == NULL)
    {
         return -ENOMEM;
    }

    if( machine_is_msm7x25_u8500() || machine_is_msm7x25_um840())
    {
         *is_surport_fingers = true;
    }
    else    
    {
        *is_surport_fingers = false;
    }

    return result;
}

int board_use_tssc_touch(bool * use_touch_key)
{
    int result = 0;

    if (use_touch_key == NULL)
    {
         return -ENOMEM;
    }

/* add U8100 series product  */
    if( machine_is_msm7x25_u8110() || machine_is_msm7x25_u8100() \
        || machine_is_msm7x25_u8105() || machine_is_msm7x25_u8107() \
        || machine_is_msm7x25_u8109() || machine_is_msm7x25_u8150() \
        || machine_is_msm7x25_c8150() || machine_is_msm7x25_c8500() \
        )
    {
         *use_touch_key = true;
    }
    else    
    {
        *use_touch_key = false;
    }

    return result;
}

/*sub board id interface*/
hw_ver_sub_type get_hw_sub_board_id(void)
{
    return (hw_ver_sub_type)(sub_board_id&HW_VER_SUB_MASK);
}

/*this function return true if the board support OFN */
int board_support_ofn(bool * ofn_support)
{
    int ret = 0;

    if(NULL == ofn_support)
    {
        return -EPERM;
    }

    if(machine_is_msm7x25_u8120() || machine_is_msm7x25_u8500() || machine_is_msm7x25_um840())
    {
        *ofn_support = true;
    }
    else
    {
        *ofn_support = false;
    }
    return ret;
}
static bool camera_i2c_state = false;
bool camera_is_supported(void)
{
        return camera_i2c_state;
}

void set_camera_support(bool status)
{
        camera_i2c_state = status;
}
static bool st303_gs_state = false;

bool st303_gs_is_supported(void)
{
    return st303_gs_state;
}

void set_st303_gs_support(bool status)
{
    st303_gs_state = status;
}

static bool touch_state = false;

/*
 *  return: 0 ----Touch not supported!
 *             1 ----Touch has been supported!
 */
bool touch_is_supported(void)
{
    return touch_state;
}

void set_touch_support(bool status)
{
    touch_state= status;
}
/*
 *  return: 0 ----not support bcm wifi
 *          1 ----support bcm wifi
 *          *p_gpio  return MSM WAKEUP WIFI gpio value
 */
unsigned int board_support_bcm_wifi(unsigned *p_gpio)
{
	unsigned int wifi_is_bcm = 1;  /* default support bcm wifi */
	unsigned gpio_msm_wake_wifi = 17;   /* default msm_wake_wifi gpio is GPIO_17 */

	if(machine_is_msm7x25_u8150())
	{
		wifi_is_bcm = 1;
		if((get_hw_sub_board_id() == HW_VER_SUB_VA) || ((get_hw_sub_board_id() == HW_VER_SUB_VB)))
		{
			gpio_msm_wake_wifi = 124;  //GPIO_124
		}
		else
		{
			gpio_msm_wake_wifi = 17;  //GPIO_17
		}
	}
	else if(machine_is_msm7x25_u8500())
	{
		wifi_is_bcm = 1;
		if(get_hw_sub_board_id() == HW_VER_SUB_VA)
		{
			gpio_msm_wake_wifi = 124;
		}
		else
		{
			gpio_msm_wake_wifi = 17;  //GPIO_17
		}
	}
	else if(machine_is_msm7x25_um840())
	{
		wifi_is_bcm = 1;
		gpio_msm_wake_wifi = 17;
	}
	else if(machine_is_msm7x25_c8500()|| machine_is_msm7x25_c8150() || machine_is_msm7x25_m860())
	{
		wifi_is_bcm = 1;
		gpio_msm_wake_wifi = 17;
	}
	else if(machine_is_msm7x25_u8300() && (get_hw_sub_board_id() != HW_VER_SUB_SURF))
	{
		wifi_is_bcm = 1;
		gpio_msm_wake_wifi = 17;
	}
	else
	{
		wifi_is_bcm = 0;
		gpio_msm_wake_wifi = 124;
	}

	if(p_gpio != NULL)
	{
		*p_gpio = gpio_msm_wake_wifi;
	}

	printk(KERN_INFO "%s:<WIFI> wifi_is_bcm = %d, msm_wakeup_wifi_gpio = %d\n", __FUNCTION__, wifi_is_bcm, gpio_msm_wake_wifi);
    
	return wifi_is_bcm;
}

EXPORT_SYMBOL(board_support_bcm_wifi);

/* =============================================================================*/

MACHINE_START(MSM7X25_U8100, "HUAWEI U8100 BOARD")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params	= 0x00200100,
	.fixup          = msm7x2x_fixup,
	.map_io		= msm7x2x_map_io,
	.init_irq	= msm7x2x_init_irq,
	.init_machine	= msm7x2x_init,
	.timer		= &msm_timer,
MACHINE_END

MACHINE_START(MSM7X25_U8105, "HUAWEI U8105 BOARD")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params	= 0x00200100,
	.fixup          = msm7x2x_fixup,
	.map_io		= msm7x2x_map_io,
	.init_irq	= msm7x2x_init_irq,
	.init_machine	= msm7x2x_init,
	.timer		= &msm_timer,
MACHINE_END

MACHINE_START(MSM7X25_U8107, "HUAWEI U8107 BOARD")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params	= 0x00200100,
	.fixup          = msm7x2x_fixup,
	.map_io		= msm7x2x_map_io,
	.init_irq	= msm7x2x_init_irq,
	.init_machine	= msm7x2x_init,
	.timer		= &msm_timer,
MACHINE_END

MACHINE_START(MSM7X25_U8109, "HUAWEI U8109 BOARD")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params	= 0x00200100,
	.fixup          = msm7x2x_fixup,
	.map_io		= msm7x2x_map_io,
	.init_irq	= msm7x2x_init_irq,
	.init_machine	= msm7x2x_init,
	.timer		= &msm_timer,
MACHINE_END

MACHINE_START(MSM7X25_U8110, "HUAWEI U8110 BOARD")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params	= 0x00200100,
	.fixup          = msm7x2x_fixup,
	.map_io		= msm7x2x_map_io,
	.init_irq	= msm7x2x_init_irq,
	.init_machine	= msm7x2x_init,
	.timer		= &msm_timer,
MACHINE_END

    
MACHINE_START(MSM7X25_U8120, "HUAWEI U8120 BOARD ")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params	= 0x00200100,
    .fixup          = msm7x2x_fixup,
	.map_io		= msm7x2x_map_io,
	.init_irq	= msm7x2x_init_irq,
	.init_machine	= msm7x2x_init,
	.timer		= &msm_timer,
MACHINE_END

MACHINE_START(MSM7X25_U7610, "HUAWEI U7610 BOARD")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params	= 0x00200100,
	.fixup          = msm7x2x_fixup,
	.map_io		= msm7x2x_map_io,
	.init_irq	= msm7x2x_init_irq,
	.init_machine	= msm7x2x_init,
	.timer		= &msm_timer,
MACHINE_END
    
MACHINE_START(MSM7X25_U8500, "HUAWEI U8500 BOARD ")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params	= 0x00200100,
    .fixup          = msm7x2x_fixup,
	.map_io		= msm7x2x_map_io,
	.init_irq	= msm7x2x_init_irq,
	.init_machine	= msm7x2x_init,
	.timer		= &msm_timer,
MACHINE_END
MACHINE_START(MSM7X25_UM840, "HUAWEI UM840 BOARD ")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params	= 0x00200100,
    .fixup          = msm7x2x_fixup,
	.map_io		= msm7x2x_map_io,
	.init_irq	= msm7x2x_init_irq,
	.init_machine	= msm7x2x_init,
	.timer		= &msm_timer,
MACHINE_END
    
MACHINE_START(MSM7X25_U8300, "HUAWEI U8300 BOARD ")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params	= 0x00200100,
    .fixup          = msm7x2x_fixup,
	.map_io		= msm7x2x_map_io,
	.init_irq	= msm7x2x_init_irq,
	.init_machine	= msm7x2x_init,
	.timer		= &msm_timer,
MACHINE_END

MACHINE_START(MSM7X25_U8150, "HUAWEI U8150 BOARD ")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params	= 0x00200100,
    .fixup          = msm7x2x_fixup,
	.map_io		= msm7x2x_map_io,
	.init_irq	= msm7x2x_init_irq,
	.init_machine	= msm7x2x_init,
	.timer		= &msm_timer,
MACHINE_END

/*C8150 version*/
MACHINE_START(MSM7X25_C8150, "HUAWEI C8150 BOARD ")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params	= 0x00200100,
    .fixup          = msm7x2x_fixup,
	.map_io		= msm7x2x_map_io,
	.init_irq	= msm7x2x_init_irq,
	.init_machine	= msm7x2x_init,
	.timer		= &msm_timer,
MACHINE_END

MACHINE_START(MSM7X25_C8500, "HUAWEI C8500 BOARD")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params	= 0x00200100,
	.fixup          = msm7x2x_fixup,
	.map_io		= msm7x2x_map_io,
	.init_irq	= msm7x2x_init_irq,
	.init_machine	= msm7x2x_init,
	.timer		= &msm_timer,
MACHINE_END

MACHINE_START(MSM7X25_C8600, "HUAWEI C8600 BOARD")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params	= 0x00200100,
	.fixup          = msm7x2x_fixup,
	.map_io		= msm7x2x_map_io,
	.init_irq	= msm7x2x_init_irq,
	.init_machine	= msm7x2x_init,
	.timer		= &msm_timer,
MACHINE_END

MACHINE_START(MSM7X25_M860, "HUAWEI M860 BOARD")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params	= 0x00200100,
	.fixup          = msm7x2x_fixup,
	.map_io		= msm7x2x_map_io,
	.init_irq	= msm7x2x_init_irq,
	.init_machine	= msm7x2x_init,
	.timer		= &msm_timer,
MACHINE_END

