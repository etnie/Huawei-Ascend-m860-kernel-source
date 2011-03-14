/* drivers/input/misc/aps-12d.c
 *
 * Copyright (C) 2010 HUAWEI, Inc.
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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include "aps-12d.h"
#include <linux/hardware_self_adapt.h>
#include <linux/wakelock.h>

//#define PROXIMITY_DEBUG
#undef PROXIMITY_DEBUG

#ifdef PROXIMITY_DEBUG
#define PROXIMITY_DEBUG(fmt, args...) printk(KERN_INFO fmt, ##args)
#else
#define PROXIMITY_DEBUG(fmt, args...)
#endif
static int aps_debug_mask;
module_param_named(aps_debug, aps_debug_mask, int,
		S_IRUGO | S_IWUSR | S_IWGRP);

#define APS_DBG(x...) do {\
	if (aps_debug_mask) \
		printk(KERN_DEBUG x);\
	} while (0)

#ifndef abs
#define abs(a)  ((0 < (a)) ? (a) : -(a))
#endif

static struct workqueue_struct *aps_wq;

struct aps_data {
	uint16_t addr;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct mutex  mlock;
	struct hrtimer timer;
	struct work_struct  work;	
	int (*power)(int on);
};

static struct aps_data  *this_aps_data;

extern struct input_dev *sensor_dev;

static int aps_12d_delay = APS_12D_TIMRER;     /*1s*/
static int aps_12d_timer_count = 0;

static int aps_first_read = 1;

static int proximity_device_minor = 0;
static struct wake_lock proximity_wake_lock;

static int high_threshold = 0;
static int low_threshold = 0;

static atomic_t l_flag;
static atomic_t p_flag;

#define LSENSOR_MAX_LEVEL 7
static uint16_t lsensor_adc_table[LSENSOR_MAX_LEVEL] = {
	/*20, 32, 48, 64, 256, 1024, 4096 */
	5, 20, 32 , 64, 256, 640, 1024
};

static uint16_t lsensor_lux_table[LSENSOR_MAX_LEVEL] = {
	10, 225, 320, 640, 1280, 2600, 10240
};

/* delete some lines */
/* NOTE: 4 equals the number of item in enum aps_12d_range_sel */
#define 	MAX_APPRVALUE		100
#define		TOTAL_RANGE_NUM		4	  /* aps-12d has 4 types of range */
#define		MAX_ADC_OUTPUT		4096  /* adc max value */
#define		RANGE_FIX			400	  /* adc */
#define		ADJUST_GATE			5	  /* ADJUST_GATE */

static unsigned int range_index = 0;
static unsigned int adjust_time = 0;
static int last_event = -1;
static int pro_flag = 0;
static unsigned int low_pro_value[TOTAL_RANGE_NUM] = {500,225,55,20};
static unsigned int high_pro_value[TOTAL_RANGE_NUM] = {650,260,75,27};
static unsigned int mistake_value[TOTAL_RANGE_NUM] = {4096,1000,250,60};
static unsigned int range_reg_value[TOTAL_RANGE_NUM] = { 
							 APS_12D_RANGE_SEL_ALS_1000, \
						     APS_12D_RANGE_SEL_ALS_4000, \
						     APS_12D_RANGE_SEL_ALS_16000, \
						     APS_12D_RANGE_SEL_ALS_64000 };

static unsigned int up_range_value[TOTAL_RANGE_NUM] = {0};
static int prox_first_read = 1;
static int base_value = 0;
static uint8_t aps_current_index = 0;
/*accord different phone to set current*/
static unsigned int aps_current[TOTAL_RANGE_NUM] = {APS_12D_IRDR_SEL_50MA,
			APS_12D_IRDR_SEL_25MA,APS_12D_IRDR_SEL_12P5MA,APS_12D_IRDR_SEL_6P25MA
};
/*this table is used for u8300*/
static uint16_t lsensor_adc_table_U8300[LSENSOR_MAX_LEVEL] = {
	2, 20, 32 , 64, 256, 640, 1024
};
static inline int aps_i2c_reg_read(struct aps_data *aps , int reg)
{
	int val = 0;
	int ret = 0;
	int i = 0;

	mutex_lock(&aps->mlock);

	/* First write reg, then read reg data, resolve i2c error */
	//val = i2c_smbus_read_byte_data(aps->client, reg);
	for( i = 0; i < 3; i++){
		ret = i2c_smbus_write_byte(aps->client, reg);
		if(ret >= 0)
			break;
	}
	if ( 3 == i )
		printk(KERN_ERR "%s: failed to write reg[%d], err=%d\n", __FUNCTION__, reg, val);

	for( i = 0; i < 3; i++){
		val = i2c_smbus_read_byte(aps->client);
		if(val >= 0)
			break;
	}
	if ( 3 == i )
		printk(KERN_ERR "%s: failed to read reg[%d], err=%d\n", __FUNCTION__, reg, val);

	mutex_unlock(&aps->mlock);

	return val;
}
static inline int aps_i2c_reg_write(struct aps_data *aps, int reg, uint8_t val)
{
	int ret = 0;
	int i = 0;

	mutex_lock(&aps->mlock);

	for( i = 0; i < 3; i++){
		ret = i2c_smbus_write_byte_data(aps->client, reg, val);
		if( ret >= 0)
			break;
	}
	if(3 == i)
		printk(KERN_ERR "%s: failed to write %d to reg[%d], err=%d\n", __FUNCTION__, val, reg, ret);

	mutex_unlock(&aps->mlock);

	return ret;
}

static int aps_12d_open(struct inode *inode, struct file *file)
{	
	PROXIMITY_DEBUG("aps_12d_open enter, timer_count=%d\n", aps_12d_timer_count);

	if( proximity_device_minor == iminor(inode) ){
		printk("%s:proximity_device_minor == iminor(inode)", __func__);
		wake_lock( &proximity_wake_lock);
		
		/* 0 is close, 1 is far */
		input_report_abs(this_aps_data->input_dev, ABS_DISTANCE, 1);			
		input_sync(this_aps_data->input_dev);
		PROXIMITY_DEBUG("%s:proximity = %d", __func__, 1);
	}
	if( 0 == aps_12d_timer_count )
		hrtimer_start(&this_aps_data->timer, ktime_set(1, 0), HRTIMER_MODE_REL);

	aps_12d_timer_count++;
	
	return nonseekable_open(inode, file);
}

static int aps_12d_release(struct inode *inode, struct file *file)
{
	PROXIMITY_DEBUG("aps_12d_release enter, timer_count=%d\n ", aps_12d_timer_count);

	aps_12d_timer_count--;
	/*
 	 *app maybe can't sleep when the scream has been turned off,
 	 *so this tag will not initializtion.
 	 */
	if(machine_is_msm7x25_u8300())
	{
		aps_first_read = 1;
	}
	if( 0 == aps_12d_timer_count ) {
		hrtimer_cancel(&this_aps_data->timer);
		aps_12d_delay = APS_12D_TIMRER;
	}
	if( proximity_device_minor == iminor(inode) ){
		prox_first_read = 1;
		base_value = 0;
/*when ring off,configure the current and lux once again*/
		if (range_index)
		{
			pro_flag = 1;
		}
		printk("%s: proximity_device_minor == iminor(inode)", __func__);
		wake_unlock( &proximity_wake_lock);
	}
	   
	return 0;
}

static int
aps_12d_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	   unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	short flag;

	switch (cmd) 
	{
		case ECS_IOCTL_APP_SET_LFLAG:
			if (copy_from_user(&flag, argp, sizeof(flag)))
				return -EFAULT;
				break;
		case ECS_IOCTL_APP_SET_PFLAG:
			if (copy_from_user(&flag, argp, sizeof(flag)))
				return -EFAULT;
				break;
		case ECS_IOCTL_APP_SET_DELAY:
			if (copy_from_user(&flag, argp, sizeof(flag)))
				return -EFAULT;
				break;
		
			default:
				break;
	}

	switch (cmd) 
	{
		case ECS_IOCTL_APP_SET_LFLAG:
			atomic_set(&l_flag, flag);
			break;

		case ECS_IOCTL_APP_GET_LFLAG:  /*get open acceleration sensor flag*/
			flag = atomic_read(&l_flag);
			break;

		case ECS_IOCTL_APP_SET_PFLAG:
			atomic_set(&p_flag, flag);
			if(machine_is_msm7x25_u8300())
			{
				if( flag )
				{
					/*
					 * this means the proximity sensor is open.
					 * so init the range_index to zero 
					 */
					range_index = 0;
					adjust_time = 0;
					last_event = -1;
					PROXIMITY_DEBUG("reinit range_index to 0!\n");
				}
			}
			break;

		case ECS_IOCTL_APP_GET_PFLAG:  /*get open acceleration sensor flag*/
			flag = atomic_read(&p_flag);
			break;

		case ECS_IOCTL_APP_SET_DELAY:
			if(flag)
				aps_12d_delay = flag;
			else
				aps_12d_delay = 200;   /*200ms*/
			break;

		case ECS_IOCTL_APP_GET_DELAY:
			flag = aps_12d_delay;
			break;
			
		default:
			break;
	}

	switch (cmd) 
	{
		case ECS_IOCTL_APP_GET_LFLAG:
			if (copy_to_user(argp, &flag, sizeof(flag)))
				return -EFAULT;
			break;

		case ECS_IOCTL_APP_GET_PFLAG:
			if (copy_to_user(argp, &flag, sizeof(flag)))
				return -EFAULT;
			break;

		case ECS_IOCTL_APP_GET_DELAY:
			if (copy_to_user(argp, &flag, sizeof(flag)))
			return -EFAULT;
			
			break;
			
		default:
			break;
	}
	return 0;
	
}

static struct file_operations aps_12d_fops = {
	.owner = THIS_MODULE,
	.open = aps_12d_open,
	.release = aps_12d_release,
	.ioctl = aps_12d_ioctl,
};

static struct miscdevice light_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "light",
	.fops = &aps_12d_fops,
};

static struct miscdevice proximity_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "proximity",
	.fops = &aps_12d_fops,
};

static void aps_12d_work_func(struct work_struct *work)
{
	int flag = -1;
	// delete flag_old
	int ret;
	int reg_val_lsb;
	int reg_val_msb;
	int	sesc = aps_12d_delay/1000;
	int nsesc = (aps_12d_delay%1000)*1000000;
	int ir_count = 0;
	int ps_count = 0;
	uint16_t als_count = 0;
	uint8_t als_level = 0;
	/* del als_level_old */
	uint8_t i;
	int sub_value = 0;
	struct aps_data *aps = container_of(work, struct aps_data, work);

	PROXIMITY_DEBUG("aps_12d_work_func enter\n ");
	if(machine_is_msm7x25_u8300())
	{
		if (pro_flag)
		{
			/*set current to 25mA,lux range is 1000.*/
			aps_current_index = 1;
			aps_i2c_reg_write(aps, APS_12D_REG_CMD2, \
	                         	(uint8_t)(aps_current[aps_current_index] << 6 | \
	   									APS_12D_FREQ_SEL_DC << 4 | \
	                                   	APS_12D_RES_SEL_12 << 2 | \
	                                   	APS_12D_RANGE_SEL_ALS_1000));
/*delete some line,no need to delay 30ms*/
			low_threshold = 170;
			high_threshold = 180;
			pro_flag = 0;
		}
	}
	if (atomic_read(&p_flag)) {
		/* Command 1 register: IR once */
re_adjust:
		if(machine_is_msm7x25_u8300())
		{
			/* init the range to the num last time we set */
			if(( range_index > 0 ) && ( range_index < TOTAL_RANGE_NUM ))
			{
				/*when range > 1000,set current to 50mA*/
				aps_current_index = 0;
				aps_i2c_reg_write(aps, APS_12D_REG_CMD2, \
						(uint8_t)(aps_current[aps_current_index] << 6 | \
									APS_12D_FREQ_SEL_DC << 4 | \
									APS_12D_RES_SEL_12 << 2 | \
									range_reg_value[range_index]));
/*delete some line,no need to delay 30ms*/
				low_threshold = low_pro_value[range_index];
				high_threshold = high_pro_value[range_index];
				PROXIMITY_DEBUG("lgf:re_adjust,range_index = %d\n",range_index);
			}
			else
			{
				PROXIMITY_DEBUG("lgf:BUG: range_index may be equal 0!!!!\n");
				range_index = 0;
				aps_current_index = 1;
			}
		}
	    ret = aps_i2c_reg_write(aps, APS_12D_REG_CMD1, APS_12D_IR_ONCE);
	    msleep(60);
	    reg_val_lsb = aps_i2c_reg_read(aps, APS_12D_DATA_LSB);
	    reg_val_msb = aps_i2c_reg_read(aps, APS_12D_DATA_MSB);
	    ir_count = ((uint16_t)reg_val_msb << 8) + (uint16_t)reg_val_lsb;
	    PROXIMITY_DEBUG("IR once lsb=%d; msb=%d; ir_count=%d \n", reg_val_lsb, reg_val_msb, ir_count);
	    if (ir_count > 0xFFF){
		    PROXIMITY_DEBUG("get wrong ir value, ir_count=%d \n", ir_count);
		    ir_count = 0xFFF;
	    }
	    if (ir_count < 0){
		    PROXIMITY_DEBUG("get wrong ir value, ir_count=%d \n", ir_count);
		    ir_count = 0;
	    }
		/*
		 * auto adjust the range
		 * stratety:
		 * if current adc value >= up_range_value[i]
		 *     switch to upper range
		 * if current adc value < down_range_value[i]
		 *     switch to lower range
		 */
		if(machine_is_msm7x25_u8300())
		{
			if(ir_count > up_range_value[range_index])
			{
				if(adjust_time < 3)
				{
					if(range_index < TOTAL_RANGE_NUM-1)
					{
						PROXIMITY_DEBUG("lgf:switch to upper range: %d.\n", range_index);
						range_index++;
						adjust_time++;
						goto re_adjust;
					}
					else
					{
						PROXIMITY_DEBUG("lgf:infrared ray TOO HIGH?\n");
					}
				}
				else
				{
					PROXIMITY_DEBUG("lgf:proximity readjust exceed max retry times.\n");
				}
			}
		}
	    msleep(30);

	    ret = aps_i2c_reg_write(aps, APS_12D_REG_CMD1, APS_12D_PROXIMITY_ONCE);
	    msleep(60);
	    reg_val_lsb = aps_i2c_reg_read(aps, APS_12D_DATA_LSB);
	    reg_val_msb = aps_i2c_reg_read(aps, APS_12D_DATA_MSB);
	    ps_count = ((uint16_t)reg_val_msb << 8) + (uint16_t)reg_val_lsb;
	    PROXIMITY_DEBUG("PS once lsb=%d; msb=%d; ps_count=%d \n", reg_val_lsb, reg_val_msb, ps_count);
	    if (ps_count > 0xFFF){
		    PROXIMITY_DEBUG("get wrong ps value, ps_count=%d \n", ps_count);
		    ps_count = 0xFFF;
	    }
	    if (ps_count < 0){
		    PROXIMITY_DEBUG("get wrong ps value, ps_count=%d \n", ps_count);
		    ps_count = 0;
	    }
        APS_DBG("the ps-ir is %d!\n",ps_count - ir_count);
/*delete some lines*/
	    if (machine_is_msm7x25_u8300())
	    {
			if (!range_index)
			{
				sub_value = ps_count - ir_count;
 			if (prox_first_read)
 			{
 				if (sub_value < MAX_APPRVALUE)
	        	{
			    	PROXIMITY_DEBUG("approach sensor hole at first\n");
					goto sendflag;
	        	}
				/*get base value*/
	 	        base_value = sub_value;
	            prox_first_read = 0;
	        }
				ps_count = sub_value;
				ir_count = base_value;
			}
			else
			{
				/* ps - ir is wrong value */
				if((ps_count - ir_count > mistake_value[range_index]) || (ps_count - ir_count < 0))
				{
					PROXIMITY_DEBUG("lgf:ps - ir is wrong value\n");
					goto sendflag;
				}
			}
		}
	    if((ps_count - ir_count) > high_threshold)
		    flag = 0;
	    else if( (ps_count - ir_count) < low_threshold )
	    	flag = 1;
	    else{
		    PROXIMITY_DEBUG("the value is in the threshold, do not report. \n");
	    }
sendflag:
		if (machine_is_msm7x25_u8300())
	    {
			/* skip invalid event */
			if(-1 != flag)
			{
				if(1 == flag)
				{
					/* report far event immediately */
					/* 0 is close, 1 is far */
					input_report_abs(aps->input_dev, ABS_DISTANCE, flag);
					input_sync(aps->input_dev);
					PROXIMITY_DEBUG("lgf:report far!!!!\n");
				}
				else if(last_event != flag)
				{
					PROXIMITY_DEBUG("NOTE: skip unstable data: %s !!!\n", flag ? "far" : "close");
				}
				else
				{
					PROXIMITY_DEBUG("report distance flag=%d \n", flag);
					input_report_abs(aps->input_dev, ABS_DISTANCE, flag);
					input_sync(aps->input_dev);
				}
			}
			/* save the last event */
			last_event = flag;
			/* set lux to 1000 */
			if (range_index != 0)
			{
				aps_i2c_reg_write(aps, APS_12D_REG_CMD2, \
	                         (uint8_t)(APS_12D_IRDR_SEL_25MA << 6 | \
	                                   APS_12D_FREQ_SEL_DC << 4 | \
	                                   APS_12D_RES_SEL_12 << 2 | \
	                                   APS_12D_RANGE_SEL_ALS_1000));
			}
		}
		else
		{
			PROXIMITY_DEBUG("report distance flag=%d \n", flag);
			/* 0 is close, 1 is far */
			input_report_abs(aps->input_dev, ABS_DISTANCE, flag);
			input_sync(aps->input_dev);
		}
	}

	if (atomic_read(&p_flag) && atomic_read(&l_flag))
	    msleep(120);
	
	if (atomic_read(&l_flag)) {
		ret = aps_i2c_reg_write(aps, APS_12D_REG_CMD1, APS_12D_ALS_ONCE);
		msleep(60);
		reg_val_lsb = aps_i2c_reg_read(aps, APS_12D_DATA_LSB);
		reg_val_msb = aps_i2c_reg_read(aps, APS_12D_DATA_MSB);
		als_count = ((uint16_t)reg_val_msb << 8) + (uint16_t)reg_val_lsb;
		PROXIMITY_DEBUG("ALS once lsb=%d; msb=%d; als_count=%d \n", reg_val_lsb, reg_val_msb, als_count);

		if (als_count > 0xFFF){
			PROXIMITY_DEBUG("get wrong als value, als_count=%d \n", als_count);
			als_count = 0xFFF;
		}

		als_level = LSENSOR_MAX_LEVEL - 1;
		for (i = 0; i < ARRAY_SIZE(lsensor_adc_table); i++){
			if (als_count < lsensor_adc_table[i]){
				als_level = i;
				break;
			}
		}
		PROXIMITY_DEBUG("report adc level=%d \n", als_level);
		
		if(aps_first_read)
		{
			/* report a invalid key first */
			aps_first_read = 0;
			input_report_abs(aps->input_dev, ABS_LIGHT, -1);
			input_sync(aps->input_dev);
		}
		else
		{
			PROXIMITY_DEBUG("report lux value=%d \n", lsensor_lux_table[als_level]);
			input_report_abs(aps->input_dev, ABS_LIGHT, lsensor_lux_table[als_level]);
			input_sync(aps->input_dev);
		}
		
	}
	
	if (atomic_read(&p_flag) || atomic_read(&l_flag))
		hrtimer_start(&aps->timer, ktime_set(sesc, nsesc), HRTIMER_MODE_REL);
	
}

static enum hrtimer_restart aps_timer_func(struct hrtimer *timer)
{
	struct aps_data *aps = container_of(timer, struct aps_data, timer);		
	queue_work(aps_wq, &aps->work);
	return HRTIMER_NORESTART;
}

static int aps_12d_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{	
	int ret;
	struct aps_data *aps;
/*delete some lines*/
	int adc_index = 0;
	int i;

	printk(KERN_INFO "aps_12d_probe enter\n ");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "aps_12d_probe: need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	if(machine_is_msm7x25_u8150() || machine_is_msm7x25_c8150())
	{
		if((get_hw_sub_board_id() == HW_VER_SUB_VA) || ((get_hw_sub_board_id() == HW_VER_SUB_VB)))
		{
			printk(KERN_ERR "aps_12d_probe: aps is not supported in c8150 and u8150 T1 board!\n");
			ret = -ENODEV;
			goto err_check_functionality_failed; 
		}
	}
	
	aps = kzalloc(sizeof(*aps), GFP_KERNEL);
	if (aps == NULL) {
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	mutex_init(&aps->mlock);

	INIT_WORK(&aps->work, aps_12d_work_func);
	aps->client = client;
	i2c_set_clientdata(client, aps);

/*delete some lines,configure aps register after some parameter has been initialized */
	if( machine_is_msm7x25_u8150() || machine_is_msm7x25_c8150() )
	{
		high_threshold = 540;
		low_threshold = 420;
	}
	else if( machine_is_msm7x25_u8500() || machine_is_msm7x25_um840())
	{
		high_threshold = 300;
		low_threshold = 280;
	}
	else if( machine_is_msm7x25_u8300() )
	{
/* delete some lines,set shutter value for u8300 */
		high_threshold = 180;
		low_threshold = 170;
		for(i = 0; i < TOTAL_RANGE_NUM; i++)
		{
			/* NOTE: do not use the last one */
			up_range_value[i] = MAX_ADC_OUTPUT - high_pro_value[i] - RANGE_FIX;
			PROXIMITY_DEBUG("lgf:up_range_value[%d] = %d.\n",i, up_range_value[i]);
		}
/* set current to 25mA */
		aps_current_index = 1;
		for(adc_index = 0;adc_index < 7;adc_index ++)
			lsensor_adc_table[adc_index] = lsensor_adc_table_U8300[adc_index];
	}
	else
	{
		high_threshold = 780;
		low_threshold = 730;
	}
	printk(KERN_INFO "aps_12d_probe send command 2\n ");
	/* move configure Command 2 register to here: xxmA,DC,12bit,Range1 */
	ret = aps_i2c_reg_write(aps, APS_12D_REG_CMD2, \
	                         (uint8_t)(aps_current[aps_current_index] << 6 | \
	                                   APS_12D_FREQ_SEL_DC << 4 | \
	                                   APS_12D_RES_SEL_12 << 2 | \
	                                   APS_12D_RANGE_SEL_ALS_1000));
	if (ret < 0) {
		goto err_detect_failed;
	}
	if (sensor_dev == NULL) {
		aps->input_dev = input_allocate_device();
		if (aps->input_dev == NULL) {
			ret = -ENOMEM;
			printk(KERN_ERR "aps_12d_probe: Failed to allocate input device\n");
			goto err_input_dev_alloc_failed;
		}
		aps->input_dev->name = "sensors";
		
		aps->input_dev->id.bustype = BUS_I2C;
		
		input_set_drvdata(aps->input_dev, aps);
		
		ret = input_register_device(aps->input_dev);
		if (ret) {
			printk(KERN_ERR "aps_probe: Unable to register %s input device\n", aps->input_dev->name);
			goto err_input_register_device_failed;
		}
		sensor_dev = aps->input_dev;
	} else {
		aps->input_dev = sensor_dev;
	}

	set_bit(EV_ABS, aps->input_dev->evbit);
	input_set_abs_params(aps->input_dev, ABS_LIGHT, 0, 10240, 0, 0);
	input_set_abs_params(aps->input_dev, ABS_DISTANCE, 0, 1, 0, 0);

	ret = misc_register(&light_device);
	if (ret) {
		printk(KERN_ERR "aps_12d_probe: light_device register failed\n");
		goto err_light_misc_device_register_failed;
	}

	ret = misc_register(&proximity_device);
	if (ret) {
		printk(KERN_ERR "aps_12d_probe: proximity_device register failed\n");
		goto err_proximity_misc_device_register_failed;
	}

	if( proximity_device.minor != MISC_DYNAMIC_MINOR ){
		proximity_device_minor = proximity_device.minor ;
	}

	wake_lock_init(&proximity_wake_lock, WAKE_LOCK_SUSPEND, "proximity");

	hrtimer_init(&aps->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aps->timer.function = aps_timer_func;
	
	aps_wq = create_singlethread_workqueue("aps_wq");

	if (!aps_wq) {
		ret = -ENOMEM;
		goto err_create_workqueue_failed;
	}
	
	this_aps_data =aps;

	printk(KERN_INFO "aps_12d_probe: Start Proximity Sensor APS-12D\n");

	return 0;
	
err_create_workqueue_failed:
	misc_deregister(&proximity_device);
err_proximity_misc_device_register_failed:
	misc_deregister(&light_device);
err_light_misc_device_register_failed:
err_input_register_device_failed:
	input_free_device(aps->input_dev);
err_input_dev_alloc_failed:
err_detect_failed:
	kfree(aps);
err_alloc_data_failed:
err_check_functionality_failed:
	return ret;
  
}

static int aps_12d_remove(struct i2c_client *client)
{
	struct aps_data *aps = i2c_get_clientdata(client);

	PROXIMITY_DEBUG("aps_12d_remove enter\n ");

	hrtimer_cancel(&aps->timer);

	misc_deregister(&light_device);
	misc_deregister(&proximity_device);

	input_unregister_device(aps->input_dev);

	kfree(aps);
	return 0;
}

static int aps_12d_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret;
	struct aps_data *aps = i2c_get_clientdata(client);

	PROXIMITY_DEBUG("aps_12d_suspend enter\n ");

	hrtimer_cancel(&aps->timer);
	ret = cancel_work_sync(&aps->work);

	ret = aps_i2c_reg_write(aps, APS_12D_REG_CMD1, APS_12D_POWER_DOWN);

	if (aps->power) {
		ret = aps->power(0);
		if (ret < 0)
			printk(KERN_ERR "aps_12d_suspend power off failed\n");
	}

	/* enable aps_first_read */
	aps_first_read = 1;
	return 0;
}

static int aps_12d_resume(struct i2c_client *client)
{
	int ret;
	struct aps_data *aps = i2c_get_clientdata(client);

	PROXIMITY_DEBUG("aps_12d_resume enter\n ");
	/*delete some lines,Command 2 register: xxmA,DC,12bit,Range1 */
	ret = aps_i2c_reg_write(aps, APS_12D_REG_CMD2, \
	                         (uint8_t)(aps_current[aps_current_index] << 6 | \
	                                   APS_12D_FREQ_SEL_DC << 4 | \
	                                   APS_12D_RES_SEL_12 << 2 | \
	                                   APS_12D_RANGE_SEL_ALS_1000));

	/* enable aps_first_read */
	aps_first_read = 1;
	hrtimer_start(&aps->timer, ktime_set(1, 0), HRTIMER_MODE_REL);

	return 0;
}

static const struct i2c_device_id aps_id[] = {
	{ "aps-12d", 0 },
	{ }
};

static struct i2c_driver aps_driver = {
	.probe		= aps_12d_probe,
	.remove		= aps_12d_remove,
	.suspend	= aps_12d_suspend,
	.resume		= aps_12d_resume,
	.id_table	= aps_id,
	.driver = {
		.name	="aps-12d",
	},
};

static int __devinit aps_12d_init(void)
{
	return i2c_add_driver(&aps_driver);
}

static void __exit aps_12d_exit(void)
{
	i2c_del_driver(&aps_driver);
	if (aps_wq)
		destroy_workqueue(aps_wq);
}

device_initcall_sync(aps_12d_init);
module_exit(aps_12d_exit);

MODULE_DESCRIPTION("Proximity Driver");
MODULE_LICENSE("GPL");
